#!/usr/bin/python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Virtual Me2Me implementation.  This script runs and manages the processes
# required for a Virtual Me2Me desktop, which are: X server, X desktop
# session, and Host process.
# This script is intended to run continuously as a background daemon
# process, running under an ordinary (non-root) user account.

import sys
if sys.version_info[0] != 3 or sys.version_info[1] < 5:
  print("This script requires Python version 3.5")
  sys.exit(1)

import argparse
import atexit
import contextlib
import datetime
import getpass
import json
import logging
import os
import re
import resource
import signal
import socket
import subprocess
import syslog
import time

import psutil

from shared_lib import (
    CONFIG_DIR,
    DEBIAN_XSESSION_PATH,
    HOST_BINARY_PATH,
    MINIMUM_PROCESS_LIFETIME,
    RelaunchInhibitor,
    SCRIPT_DIR,
    SESSION_FILE_PATH,
    SYSTEM_SESSION_FILE_PATH,
    cleanup,
    create_desktop,
    display_manager_is_gdm,
    exec_self_via_login_shell,
    g_host_hash,
    terminate_process,
    watch_for_resolution_changes,
)

# If this env var is defined, extra host params will be loaded from this env var
# as a list of strings separated by space (\s+). Note that param that contains
# space is currently NOT supported and will be broken down into two params at
# the space character.
HOST_EXTRA_PARAMS_ENV_VAR = "CHROME_REMOTE_DESKTOP_HOST_EXTRA_PARAMS"

CRASH_UPLOADER_PATH = os.path.join(SCRIPT_DIR, "crash-uploader")

# Host offline reason if the host retry count is exceeded. (Note: It may or may
# not be possible to send this, depending on why the host is failing.)
HOST_OFFLINE_REASON_HOST_RETRIES_EXCEEDED = "HOST_RETRIES_EXCEEDED"

# Host offline reason if the crash-uploader retry count is exceeded.
HOST_OFFLINE_REASON_CRASH_UPLOADER_RETRIES_EXCEEDED = (
    "CRASH_UPLOADER_RETRIES_EXCEEDED"
)

# Number of processes/threads reserved for the Chrome Remote Desktop host
# process above the user session's soft RLIMIT_NPROC limit, so that thread or
# process exhaustion in the user's desktop session cannot cause pthread_create()
# or fork() in the host process to fail with EAGAIN.
HOST_RESERVED_NPROC = 4096

EPILOG = """This script is not intended for use by end-users. To configure
Chrome Remote Desktop, please install the app from the Chrome
Web Store: https://chrome.google.com/remotedesktop"""


@contextlib.contextmanager
def reserve_host_process_limits():
  """Temporarily raises the soft RLIMIT_NPROC limit while spawning the host
  process so it inherits extra thread headroom without using preexec_fn."""
  saved_limits = None
  try:
    soft, hard = resource.getrlimit(resource.RLIMIT_NPROC)
    if soft != resource.RLIM_INFINITY:
      new_soft = (soft + HOST_RESERVED_NPROC if hard == resource.RLIM_INFINITY
                  else min(soft + HOST_RESERVED_NPROC, hard))
      if new_soft > soft:
        resource.setrlimit(resource.RLIMIT_NPROC, (new_soft, hard))
        saved_limits = (soft, hard)
  except (ValueError, OSError):
    pass
  try:
    yield
  finally:
    if saved_limits is not None:
      try:
        resource.setrlimit(resource.RLIMIT_NPROC, saved_limits)
      except (ValueError, OSError):
        pass


def is_supported_platform():
  # Always assume that the system is supported if the config directory or
  # session file exist.
  if (os.path.isdir(CONFIG_DIR) or os.path.isfile(SESSION_FILE_PATH) or
      os.path.isfile(SYSTEM_SESSION_FILE_PATH)):
    return True

  # There's a bug in recent versions of GDM that will prevent a user from
  # logging in via GDM when there is already an x11 session running for that
  # user (such as the one started by CRD). Since breaking local login is a
  # pretty serious issue, we want to disallow host set up through the website.
  # Unfortunately, there's no way to return a specific error to the website, so
  # we just return False to indicate an unsupported platform. The user can still
  # set up the host using the headless setup flow, where we can at least display
  # a warning. See https://gitlab.gnome.org/GNOME/gdm/-/issues/580 for details
  # of the bug and fix.
  if display_manager_is_gdm():
    return False

  # The session chooser expects a Debian-style Xsession script.
  return os.path.isfile(DEBIAN_XSESSION_PATH)


def is_crash_reporting_enabled(config):
  # Use the value in the host config for usage_stats_consent if it exists,
  # otherwise opt into crash reporting if the owner is a Googler.
  usage_stats_consent = config.get("usage_stats_consent", None)
  if usage_stats_consent is not None:
    return usage_stats_consent
  else:
    return config.get("host_owner", "").endswith("@google.com")


class Config:
  def __init__(self, path):
    self.path = path
    self.data = {}
    self.changed = False

  def load(self):
    """Loads the config from file.

    Raises:
      IOError: Error reading data
      ValueError: Error parsing JSON
    """
    settings_file = open(self.path, 'r')
    self.data = json.load(settings_file)
    self.changed = False
    settings_file.close()

  def save(self):
    """Saves the config to file.

    Raises:
      IOError: Error writing data
      TypeError: Error serialising JSON
    """
    if not self.changed:
      return
    old_umask = os.umask(0o066)
    try:
      settings_file = open(self.path, 'w')
      settings_file.write(json.dumps(self.data, indent=2))
      settings_file.close()
      self.changed = False
    finally:
      os.umask(old_umask)

  def save_and_log_errors(self):
    """Calls self.save(), trapping and logging any errors."""
    try:
      self.save()
    except (IOError, TypeError) as e:
      logging.error("Failed to save config: " + str(e))

  def get(self, key, default=None):
    return self.data.get(key, default)

  def __getitem__(self, key):
    return self.data[key]

  def __setitem__(self, key, value):
    self.data[key] = value
    self.changed = True

  def clear(self):
    self.data = {}
    self.changed = True


class Authentication:
  """Manage authentication tokens for the host service account"""

  def __init__(self):
    # Note: Initial values are never used.
    self.service_account = None
    self.oauth_refresh_token = None

  def copy_from(self, config):
    """Loads the config and returns false if the config is invalid."""
    # service_account was added in M120 so hosts which were provisioned using
    # that build (or later) will have the new config key. Hosts which were first
    # configured with an older host version will only have xmpp_login so we need
    # to fallback to it for backward compatibility.
    self.service_account = config.get("service_account")
    if self.service_account is None:
      self.service_account = config.get("xmpp_login")
    if self.service_account is None:
      # Neither service_account nor xmpp_login exist so config is malformed.
      return False

    self.oauth_refresh_token = config.get("oauth_refresh_token")
    if self.oauth_refresh_token is None:
      return False

    return True

  def copy_to(self, config):
    config["xmpp_login"] = self.service_account
    config["service_account"] = self.service_account
    config["oauth_refresh_token"] = self.oauth_refresh_token


class Host:
  """This manages the configuration and process lifecycle for a host."""

  def __init__(self, host_config=None, extra_start_host_args=None,
               host_inhibitor=None):
    self.host_id = None
    self.host_name = None
    self.host_secret_hash = None
    self.private_key = None
    self.host_config = host_config
    self.extra_start_host_args = extra_start_host_args or []
    self.host_proc = None
    self.host_ready = False
    self.host_inhibitor = host_inhibitor or RelaunchInhibitor("host")
    self.crash_reporting_enabled = False
    self.crash_uploader_proc = None
    self.crash_uploader_inhibitor = None
    self.inhibitors = {
        self.host_inhibitor: HOST_OFFLINE_REASON_HOST_RETRIES_EXCEEDED
    }

  def copy_from(self, config):
    try:
      self.host_id = config.get("host_id")
      self.host_name = config["host_name"]
      self.host_secret_hash = config.get("host_secret_hash")
      self.private_key = config["private_key"]
    except KeyError:
      return False
    return bool(self.host_id)

  def copy_to(self, config):
    if self.host_id:
      config["host_id"] = self.host_id
    config["host_name"] = self.host_name
    config["host_secret_hash"] = self.host_secret_hash
    config["private_key"] = self.private_key

  def enable_crash_reporting(self):
    logging.info("Configuring crash reporting")
    self.crash_reporting_enabled = True
    self.crash_uploader_inhibitor = RelaunchInhibitor("Crash uploader")
    self.inhibitors[self.crash_uploader_inhibitor] = (
        HOST_OFFLINE_REASON_CRASH_UPLOADER_RETRIES_EXCEEDED
    )

  def launch_processes(self, desktop, backoff_time):
    if desktop.server_proc is not None and self.host_proc is None:
      self.launch_host(desktop, backoff_time)
    if self.crash_uploader_proc is None:
      self.launch_crash_uploader(desktop, backoff_time)

  def launch_host(self, desktop, backoff_time):
    logging.info("Launching host process")

    # Start remoting host
    args = [HOST_BINARY_PATH, "--host-config=-"]
    if desktop.ssh_auth_sockname:
      args.append("--ssh-auth-sockname=%s" % desktop.ssh_auth_sockname)

    args.extend(self.extra_start_host_args)

    # Have the host process use SIGUSR1 to signal a successful start.
    def sigusr1_handler(signum, frame):
      _ = signum, frame
      logging.info("Host ready to receive connections.")
      self.host_ready = True

    signal.signal(signal.SIGUSR1, sigusr1_handler)
    args.append("--signal-parent")

    logging.info(args)
    with reserve_host_process_limits():
      self.host_proc = subprocess.Popen(args, env=desktop.child_env,
                                        stdin=subprocess.PIPE)
    if not self.host_proc.pid:
      raise Exception("Could not start Chrome Remote Desktop host")

    try:
      self.host_proc.stdin.write(
          json.dumps(self.host_config.data).encode('UTF-8'))
      self.host_proc.stdin.flush()
    except IOError as e:
      # This can occur in rare situations, for example, if the machine is
      # heavily loaded and the host process dies quickly (maybe if the X
      # connection failed), the host process might be gone before this code
      # writes to the host's stdin. Catch and log the exception, allowing
      # the process to be retried instead of exiting the script completely.
      logging.error("Failed writing to host's stdin: " + str(e))
    finally:
      self.host_proc.stdin.close()
    self.host_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME, backoff_time)

  def launch_crash_uploader(self, desktop, backoff_time):
    if not self.crash_reporting_enabled:
      return

    if not os.path.exists(CRASH_UPLOADER_PATH):
      return

    logging.info("Launching crash uploader")

    args = [CRASH_UPLOADER_PATH]
    self.crash_uploader_proc = subprocess.Popen(args, env=desktop.child_env)

    if not self.crash_uploader_proc.pid:
      raise Exception("Could not start crash-uploader")

    self.crash_uploader_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME,
                                                 backoff_time)

  def cleanup(self):
    for proc, name in [(self.host_proc, "host"),
                       (self.crash_uploader_proc, "crash-uploader")]:
      if proc is not None:
        terminate_process(proc.pid, name)
    self.host_proc = None
    self.crash_uploader_proc = None

  def report_offline_reason(self, desktop, reason):
    """Attempt to report the specified offline reason to the registry. This
    is best effort, and requires a valid host config.
    """
    logging.info("Attempting to report offline reason: " + reason)
    args = [HOST_BINARY_PATH, "--host-config=-",
            "--report-offline-reason=" + reason]
    proc = subprocess.Popen(args, env=desktop.child_env, stdin=subprocess.PIPE)
    proc.communicate(json.dumps(self.host_config.data).encode('UTF-8'))

  def on_process_exit(self, desktop, pid, status):
    tear_down = False
    if self.host_proc is not None and pid == self.host_proc.pid:
      logging.info("Host process terminated")
      self.host_proc = None
      self.host_ready = False

      # These exit-codes must match the ones used by the host.
      # See remoting/host/base/host_exit_codes.h.
      # Delete the host or auth configuration depending on the returned error
      # code, so the next time this script is run, a new configuration
      # will be created and registered.
      if os.WIFEXITED(status):
        if os.WEXITSTATUS(status) == 100:
          logging.info("Host configuration is invalid - exiting.")
          sys.exit(0)
        elif os.WEXITSTATUS(status) == 101:
          logging.info("Host ID has been deleted - exiting.")
          self.host_config.clear()
          self.host_config.save_and_log_errors()
          sys.exit(0)
        elif os.WEXITSTATUS(status) == 102:
          logging.info("OAuth credentials are invalid - exiting.")
          sys.exit(0)
        elif os.WEXITSTATUS(status) == 103:
          logging.info("Host domain is blocked by policy - exiting.")
          sys.exit(0)
        # Nothing to do for Mac-only status 104 (login screen unsupported)
        elif os.WEXITSTATUS(status) == 105:
          logging.info("Username is blocked by policy - exiting.")
          sys.exit(0)
        elif os.WEXITSTATUS(status) == 106:
          logging.info("Host has been deleted - exiting.")
          self.host_config.clear()
          self.host_config.save_and_log_errors()
          sys.exit(0)
        elif os.WEXITSTATUS(status) == 107:
          logging.info("Remote access is disallowed by policy - exiting.")
          sys.exit(0)
        elif os.WEXITSTATUS(status) == 108:
          logging.info("This CPU is not supported - exiting.")
          sys.exit(0)
        else:
          logging.info("Host exited with status %s." % os.WEXITSTATUS(status))
      elif os.WIFSIGNALED(status):
        logging.info("Host terminated by signal %s." % os.WTERMSIG(status))

      # The host may have exited on it's own or been brought down by the display
      # server dying. Check if the display server is still running so we know
      # whom to penalize.
      if desktop.check_server_responding():
        self.host_inhibitor.record_stopped(expected=False)
      else:
        desktop.server_inhibitor.record_stopped(expected=False)
        # Only tear down if the display server isn't responding.
        tear_down = True

    if (self.crash_uploader_proc is not None and
            pid == self.crash_uploader_proc.pid):
      logging.info("Crash uploader process terminated")
      self.crash_uploader_proc = None
      self.crash_uploader_inhibitor.record_stopped(expected=False)
      # Don't tear down the host if the uploader is killed or crashes.
      tear_down = False

    return tear_down


def parse_config_arg(args):
  """Parses only the --config option from a given command-line.

  Returns:
    A two-tuple. The first element is the value of the --config option (or None
    if it is not specified), and the second is a list containing the remaining
    arguments
  """

  # By default, argparse will exit the program on error. We would like it not to
  # do that.
  class ArgumentParserError(Exception):
    pass
  class ThrowingArgumentParser(argparse.ArgumentParser):
    def error(self, message):
      raise ArgumentParserError(message)

  parser = ThrowingArgumentParser()
  parser.add_argument("--config", nargs='?', action="store")

  try:
    result = parser.parse_known_args(args)
    return (result[0].config, result[1])
  except ArgumentParserError:
    return (None, list(args))


def get_daemon_proc(config_file, require_child_process=False):
  """Checks if there is already an instance of this script running against
  |config_file|, and returns a psutil.Process instance for it. If
  |require_child_process| is true, only check for an instance with the
  --child-process flag specified.

  If a process is found without --config in the command line, get_daemon_proc
  will fall back to the old behavior of checking whether the script path matches
  the current script. This is to facilitate upgrades from previous versions.

  Returns:
    A Process instance for the existing daemon process, or None if the daemon
    is not running.
  """

  # Note: When making changes to how instances are detected, it is imperative
  # that this function retains the ability to find older versions. Otherwise,
  # upgrades can leave the user with two running sessions, with confusing
  # results.

  uid = os.getuid()
  this_pid = os.getpid()

  # This function should return the process with the --child-process flag if it
  # exists. If there's only a process without, it might be a legacy process.
  non_child_process = None

  # Support new & old psutil API. This is the right way to check, according to
  # http://grodola.blogspot.com/2014/01/psutil-20-porting.html
  if psutil.version_info >= (2, 0):
    psget = lambda x: x()
  else:
    psget = lambda x: x

  for process in psutil.process_iter():
    # Skip any processes that raise an exception, as processes may terminate
    # during iteration over the list.
    try:
      # Skip other users' processes.
      if psget(process.uids).real != uid:
        continue

      # Skip the process for this instance.
      if process.pid == this_pid:
        continue

      # |cmdline| will be [python-interpreter, script-file, other arguments...]
      cmdline = psget(process.cmdline)
      if len(cmdline) < 2:
        continue
      if (os.path.basename(cmdline[0]).startswith('python') and
          os.path.basename(cmdline[1]) == os.path.basename(sys.argv[0]) and
          "--start" in cmdline):
        process_config = parse_config_arg(cmdline[2:])[0]

        # Fall back to old behavior if there is no --config argument
        # TODO(rkjnsn): Consider removing this fallback once sufficient time
        # has passed.
        if process_config == config_file or (process_config is None and
                                             cmdline[1] == sys.argv[0]):
          if "--child-process" in cmdline:
            return process
          else:
            non_child_process = process

    except (psutil.NoSuchProcess, psutil.AccessDenied):
      continue

  return non_child_process if not require_child_process else None


def run_command_as_root(command):
  if os.getenv("DISPLAY"):
    # TODO(rickyz): Add a Polkit policy that includes a more friendly
    # message about what this command does.
    command = ["/usr/bin/pkexec"] + command
  else:
    command = ["/usr/bin/sudo", "-k", "--"] + command

  return subprocess.call(command)


class SignalHandler:
  """Reload the config file on SIGHUP. Since we pass the configuration to the
  host processes via stdin, they can't reload it, so terminate them. They will
  be relaunched automatically with the new config."""

  def __init__(self, host_config, host):
    self.host_config = host_config
    self.host = host

  def __call__(self, signum, _stackframe):
    logging.info("Caught signal: " + str(signum))
    if signum == signal.SIGHUP:
      logging.info("SIGHUP caught, restarting host.")
      try:
        self.host_config.load()
      except (IOError, ValueError) as e:
        logging.error("Failed to load config: " + str(e))
      if self.host is not None and self.host.host_proc:
        self.host.host_proc.send_signal(signal.SIGTERM)
    else:
      # Exit cleanly so the atexit handler, cleanup(), gets called.
      raise SystemExit


def setup_argument_parser():
  parser = argparse.ArgumentParser(
      usage="Usage: %(prog)s [options] [ -- [ X server options ] ]",
      epilog=EPILOG)
  parser.add_argument("-f", "--foreground", dest="foreground", default=False,
                      action="store_true",
                      help="Don't run as a background daemon.")
  parser.add_argument("--start", dest="start", default=False,
                      action="store_true",
                      help="Start the host.")
  parser.add_argument("-k", "--stop", dest="stop", default=False,
                      action="store_true",
                      help="Stop the daemon currently running.")
  parser.add_argument("--get-status", dest="get_status", default=False,
                      action="store_true",
                      help="Prints host status")
  parser.add_argument("--check-running", dest="check_running",
                      default=False, action="store_true",
                      help="Return 0 if the daemon is running, or 1 otherwise.")
  parser.add_argument("--config", dest="config", action="store",
                      help="Use the specified configuration file.")
  parser.add_argument("--reload", dest="reload", default=False,
                      action="store_true",
                      help="Signal currently running host to reload the "
                      "config.")
  parser.add_argument("--enable-and-start", dest="enable_and_start",
                      default=False, action="store_true",
                      help="Enable and start chrome-remote-desktop for the "
                      "current user.")
  # This flag is used when running the script from a build directory, or by the
  # systemd unit. It indicates that the script should not attempt to start
  # itself via systemd.
  parser.add_argument("--child-process", dest="child_process", default=False,
                      action="store_true",
                      help=argparse.SUPPRESS)
  # The script is being run in a new PAM session. Don't daemonize so the parent
  # knows when to clean up the PAM session, and attempt to exec a login shell to
  # allow the user's ~/.profile or similar to run.
  parser.add_argument("--new-session", dest="new_session", default=False,
                      action="store_true",
                      help=argparse.SUPPRESS)
  parser.add_argument("--watch-resolution", dest="watch_resolution",
                      type=int, nargs=2, default=False, action="store",
                      help=argparse.SUPPRESS)
  parser.add_argument(dest="args", nargs="*", help=argparse.SUPPRESS)
  return parser


def main():
  parser = setup_argument_parser()
  options = parser.parse_args()

  # Determine the filename of the host configuration.
  if options.config:
    config_file = options.config
  else:
    config_file = os.path.join(CONFIG_DIR, "host#%s.json" % g_host_hash)
  config_file = os.path.realpath(config_file)

  # Check for a modal command-line option (start, stop, etc.)
  if options.get_status:
    proc = get_daemon_proc(config_file)
    # Print the status string without additional logging information as they may
    # be parsed by scripts.
    if proc is not None:
      print("STARTED")
    elif is_supported_platform():
      print("STOPPED")
    else:
      print("NOT_IMPLEMENTED")
    return 0

  # TODO(sergeyu): Remove --check-running once NPAPI plugin and NM host are
  # updated to always use get-status flag instead.
  if options.check_running:
    proc = get_daemon_proc(config_file)
    return 1 if proc is None else 0

  if options.stop:
    proc = get_daemon_proc(config_file)
    if proc is None:
      logging.error("The daemon is not currently running")
    else:
      logging.info("Killing process %s" % proc.pid)
      proc.terminate()
      try:
        proc.wait(timeout=30)
      except psutil.TimeoutExpired:
        logging.error("Timed out trying to kill daemon process")
        return 1
    return 0

  if options.reload:
    proc = get_daemon_proc(config_file)
    if proc is None:
      logging.error("Reload failed: the daemon is not currently running")
      return 1
    logging.info("Reloading Chrome Remote Desktop daemon process")
    proc.send_signal(signal.SIGHUP)
    return 0

  if options.enable_and_start:
    user = getpass.getuser()

    # While systemd will generally prompt for a password via polkit if run by
    # a normal user, it won't properly fall back to prompting on the TTY if
    # stdin is redirected, such as is done by the start-host binary.
    # Additionally, some configurations can result in systemctl prompting the
    # user for their password multiple times, which can be confusing and
    # annoying. Running it as root avoids both issues.
    return run_command_as_root(["systemctl", "enable", "--now",
                                "chrome-remote-desktop@" + user])

  if options.watch_resolution:
    watch_for_resolution_changes(tuple(options.watch_resolution))
    return 0

  if not options.start:
    # If no modal command-line options specified, print an error and exit.
    print(EPILOG, file=sys.stderr)
    return 1

  # Determine whether a desktop is already active for the specified host
  # configuration.
  if get_daemon_proc(config_file, options.child_process) is not None:
    # Debian policy requires that services should "start" cleanly and return 0
    # if they are already running.
    logging.info("Service already running.")
    return 0

  if config_file != options.config:
    # --config was either not specified or isn't a canonical absolute path.
    # Replace it with the canonical path so get_daemon_proc can find us.
    sys.argv = ([sys.argv[0], "--config=" + config_file] +
                parse_config_arg(sys.argv[1:])[1])
    if options.child_process:
      os.execvp(sys.argv[0], sys.argv)

  if options.new_session:
    exec_self_via_login_shell()

  if not options.child_process:
    return run_command_as_root(["systemctl", "start",
                                "chrome-remote-desktop@" + getpass.getuser()])

  logging.info("CRD service is starting")
  logging.info("Machine hostname: %s", socket.getfqdn())
  uptime = datetime.timedelta(
      seconds=int(time.clock_gettime(time.CLOCK_BOOTTIME)))
  logging.info("Machine uptime: %s", uptime)

  if display_manager_is_gdm():
    # See https://gitlab.gnome.org/GNOME/gdm/-/issues/580 for details on the
    # bug.
    gdm_message = (
        "WARNING: This system uses GDM. Some GDM versions have a bug that "
        "prevents local login while Chrome Remote Desktop is running. If you "
        "run into this issue, you can stop Chrome Remote Desktop by visiting "
        "https://remotedesktop.google.com/access on another machine and "
        "clicking the delete icon next to this machine. It may take up to five "
        "minutes for the Chrome Remote Desktop to exit on this machine and for "
        "local login to start working again.")
    logging.warning(gdm_message)
    # Also log to syslog so the user has a higher change of discovering the
    # message if they go searching.
    syslog.syslog(syslog.LOG_WARNING | syslog.LOG_DAEMON, gdm_message)

  # Register an exit handler to clean up session process and the PID file.
  atexit.register(cleanup)

  # Load the initial host configuration.
  host_config = Config(config_file)
  try:
    host_config.load()
  except (IOError, ValueError) as e:
    print("Failed to load config: " + str(e), file=sys.stderr)
    return 1

  extra_start_host_args = []
  if HOST_EXTRA_PARAMS_ENV_VAR in os.environ:
    extra_start_host_args = re.split(
        r"\s+", os.environ[HOST_EXTRA_PARAMS_ENV_VAR].strip())

  # Verify that the initial host configuration has the necessary fields.
  auth = Authentication()
  auth_config_valid = auth.copy_from(host_config)
  host = Host(host_config=host_config,
              extra_start_host_args=extra_start_host_args)
  host_config_valid = host.copy_from(host_config)
  if not host_config_valid or not auth_config_valid:
    logging.error("Failed to load host configuration.")
    return 1

  # Register handler to re-load the configuration in response to signals.
  for s in [signal.SIGHUP, signal.SIGINT, signal.SIGTERM]:
    signal.signal(s, SignalHandler(host_config, host))

  if host.host_id:
    logging.info("Using host_id: " + host.host_id)

  if is_crash_reporting_enabled(host_config):
    host.enable_crash_reporting()

  desktop = create_desktop(extra_start_host_args=extra_start_host_args,
                           host_delegate=host)
  desktop.run(options.args)


if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG,
                      format="%(asctime)s:%(levelname)s:%(message)s")
  sys.exit(main())
