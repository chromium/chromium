#!/usr/bin/python3
# Copyright 2012 The Chromium Authors
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

import abc
import argparse
import atexit
import base64
import errno
import fcntl
import getpass
import grp
import hashlib
import json
import logging
import os
import platform
import pwd
import re
import shlex
import shutil
import signal
import socket
import string
import struct
import subprocess
import syslog
import tempfile
import threading
import time
import uuid

import psutil
import xdg.BaseDirectory
from packaging import version

# If this env var is defined, extra host params will be loaded from this env var
# as a list of strings separated by space (\s+). Note that param that contains
# space is currently NOT supported and will be broken down into two params at
# the space character.
HOST_EXTRA_PARAMS_ENV_VAR = "CHROME_REMOTE_DESKTOP_HOST_EXTRA_PARAMS"

# This script has a sensible default for the initial and maximum desktop size,
# which can be overridden either on the command-line, or via a comma-separated
# list of sizes in this environment variable.
DEFAULT_SIZES_ENV_VAR = "CHROME_REMOTE_DESKTOP_DEFAULT_DESKTOP_SIZES"

# By default, this script launches Xorg as the virtual X display, using the
# dummy display driver and void input device, unless Xorg+Dummy is deemed
# unsupported. When this environment variable is set, the script will instead
# launch Xvfb.
USE_XVFB_ENV_VAR = "CHROME_REMOTE_DESKTOP_USE_XVFB"

# The amount of video RAM the dummy driver should claim to have, which limits
# the maximum possible resolution.
# 1048576 KiB = 1 GiB, which is the amount of video RAM needed to have a
# 16384x16384 pixel frame buffer (the maximum size supported by VP8) with 32
# bits per pixel.
XORG_DUMMY_VIDEO_RAM = 1048576 # KiB

# By default, provide a maximum size that is large enough to support clients
# with large or multiple monitors. This is a comma-separated list of
# resolutions that will be made available if the X server supports RANDR. These
# defaults can be overridden in ~/.profile.
DEFAULT_SIZES = "1600x1200,3840x2560"

# Decides number of monitors and their resolution that should be run for the
# wayland session.
WAYLAND_DESKTOP_SIZES_ENV = "CHROME_REMOTE_DESKTOP_WAYLAND_DESKTOP_SIZES"

# Default wayland monitor size if `CHROME_REMOTE_DESKTOP_DEFAULT_DESKTOP_SIZES`
# env variable is not set.
DEFAULT_WAYLAND_DESKTOP_SIZES = "1280x720"

SCRIPT_PATH = os.path.abspath(sys.argv[0])
SCRIPT_DIR = os.path.dirname(SCRIPT_PATH)

if (os.path.basename(sys.argv[0]) == 'linux_me2me_host.py'):
  # Needed for swarming/isolate tests.
  HOST_BINARY_PATH = os.path.join(SCRIPT_DIR,
                                  "../../../out/Release/remoting_me2me_host")
else:
  HOST_BINARY_PATH = os.path.join(SCRIPT_DIR, "chrome-remote-desktop-host")

USER_SESSION_PATH = os.path.join(SCRIPT_DIR, "user-session")

CRASH_UPLOADER_PATH = os.path.join(SCRIPT_DIR, "crash-uploader")

CHROME_REMOTING_GROUP_NAME = "chrome-remote-desktop"

HOME_DIR = os.environ["HOME"]
CONFIG_DIR = os.path.join(HOME_DIR, ".config/chrome-remote-desktop")
SESSION_FILE_PATH = os.path.join(HOME_DIR, ".chrome-remote-desktop-session")
SYSTEM_SESSION_FILE_PATH = "/etc/chrome-remote-desktop-session"
SYSTEM_PRE_SESSION_FILE_PATH = "/etc/chrome-remote-desktop-pre-session"

DEBIAN_XSESSION_PATH = "/etc/X11/Xsession"

X_LOCK_FILE_TEMPLATE = "/tmp/.X%d-lock"
FIRST_X_DISPLAY_NUMBER = 20

# Amount of time to wait between relaunching processes.
SHORT_BACKOFF_TIME = 5
LONG_BACKOFF_TIME = 60

# How long a process must run in order not to be counted against the restart
# thresholds.
MINIMUM_PROCESS_LIFETIME = 60

# Thresholds for switching from fast- to slow-restart and for giving up
# trying to restart entirely.
SHORT_BACKOFF_THRESHOLD = 5
MAX_LAUNCH_FAILURES = SHORT_BACKOFF_THRESHOLD + 10

# Number of seconds to save session output to the log.
SESSION_OUTPUT_TIME_LIMIT_SECONDS = 300

# Number of seconds to save the display server output to the log.
SERVER_OUTPUT_TIME_LIMIT_SECONDS = 300

# Host offline reason if the X server retry count is exceeded.
HOST_OFFLINE_REASON_X_SERVER_RETRIES_EXCEEDED = "X_SERVER_RETRIES_EXCEEDED"

# Host offline reason if the wayland server retry count is exceeded.
HOST_OFFLINE_REASON_WAYLAND_SERVER_RETRIES_EXCEEDED = (
  "WAYLAND_SERVER_RETRIES_EXCEEDED")

# Host offline reason if the X session retry count is exceeded.
HOST_OFFLINE_REASON_SESSION_RETRIES_EXCEEDED = "SESSION_RETRIES_EXCEEDED"

# Host offline reason if the host retry count is exceeded. (Note: It may or may
# not be possible to send this, depending on why the host is failing.)
HOST_OFFLINE_REASON_HOST_RETRIES_EXCEEDED = "HOST_RETRIES_EXCEEDED"

# Host offline reason if the crash-uploader retry count is exceeded.
HOST_OFFLINE_REASON_CRASH_UPLOADER_RETRIES_EXCEEDED = (
  "CRASH_UPLOADER_RETRIES_EXCEEDED")

# This is the file descriptor used to pass messages to the user_session binary
# during startup. It must be kept in sync with kMessageFd in
# remoting_user_session.cc.
USER_SESSION_MESSAGE_FD = 202

# This is the exit code used to signal to wrapper that it should restart instead
# of exiting. It must be kept in sync with kRelaunchExitCode in
# remoting_user_session.cc and RestartForceExitStatus in
# chrome-remote-desktop@.service.
RELAUNCH_EXIT_CODE = 41

# This exit code is returned when a needed binary such as user-session or sg
# cannot be found.
COMMAND_NOT_FOUND_EXIT_CODE = 127

# This exit code is returned when a needed binary exists but cannot be executed.
COMMAND_NOT_EXECUTABLE_EXIT_CODE = 126

# User runtime directory. This is where the wayland socket is created by the
# wayland compositor/server for clients to connect to.
# TODO(rkjnsn): Use xdg.BaseDirectory.get_runtime_dir instead
RUNTIME_DIR_TEMPLATE = "/run/user/%s"

# Binary name for `gnome-session`.
GNOME_SESSION = "gnome-session"

# Binary name for `gnome-session-quit`.
GNOME_SESSION_QUIT = "gnome-session-quit"

# Globals needed by the atexit cleanup() handler.
g_desktop = None
g_host_hash = hashlib.md5(socket.gethostname().encode()).hexdigest()

def gen_xorg_config():
  return (
      # This causes X to load the default GLX module, even if a proprietary one
      # is installed in a different directory.
      'Section "Files"\n'
      '  ModulePath "/usr/lib/xorg/modules"\n'
      'EndSection\n'
      '\n'
      # Suppress device probing, which happens by default.
      'Section "ServerFlags"\n'
      '  Option "AutoAddDevices" "false"\n'
      '  Option "AutoEnableDevices" "false"\n'
      '  Option "DontVTSwitch" "true"\n'
      '  Option "PciForceNone" "true"\n'
      'EndSection\n'
      '\n'
      'Section "InputDevice"\n'
      # The host looks for this name to check whether it's running in a virtual
      # session
      '  Identifier "Chrome Remote Desktop Input"\n'
      # While the xorg.conf man page specifies that both of these options are
      # deprecated synonyms for `Option "Floating" "false"`, it turns out that
      # if both aren't specified, the Xorg server will automatically attempt to
      # add additional devices.
      '  Option "CoreKeyboard" "true"\n'
      '  Option "CorePointer" "true"\n'
      # The "void" driver is no longer available since Debian 11, but having an
      # InputDevice section with an invalid driver will still prevent the Xorg
      # server from using a fallback InputDevice setting. However, "Chrome
      # Remote Desktop Input" will not appear in the device list if the driver
      # is not available.
      '  Driver "void"\n'
      'EndSection\n'
      '\n'
      'Section "Device"\n'
      '  Identifier "Chrome Remote Desktop Videocard"\n'
      '  Driver "dummy"\n'
      '  VideoRam {video_ram}\n'
      'EndSection\n'
      '\n'
      'Section "Monitor"\n'
      '  Identifier "Chrome Remote Desktop Monitor"\n'
      'EndSection\n'
      '\n'
      'Section "Screen"\n'
      '  Identifier "Chrome Remote Desktop Screen"\n'
      '  Device "Chrome Remote Desktop Videocard"\n'
      '  Monitor "Chrome Remote Desktop Monitor"\n'
      '  DefaultDepth 24\n'
      '  SubSection "Display"\n'
      '    Viewport 0 0\n'
      '    Depth 24\n'
      '  EndSubSection\n'
      'EndSection\n'
      '\n'
      'Section "ServerLayout"\n'
      '  Identifier   "Chrome Remote Desktop Layout"\n'
      '  Screen       "Chrome Remote Desktop Screen"\n'
      '  InputDevice  "Chrome Remote Desktop Input"\n'
      'EndSection\n'.format(
          video_ram=XORG_DUMMY_VIDEO_RAM))


def display_manager_is_gdm():
  try:
    # Open as binary to avoid any encoding errors
    with open('/etc/X11/default-display-manager', 'rb') as file:
      if file.read().strip() in [b'/usr/sbin/gdm', b'/usr/sbin/gdm3']:
        return True
    # Fall through to process checking even if the file doesn't contain gdm.
  except:
    # If we can't read the file, move on to checking the process list.
    pass

  for process in psutil.process_iter():
    if process.name() in ['gdm', 'gdm3']:
      return True

  return False


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
    return False;

  # The session chooser expects a Debian-style Xsession script.
  return os.path.isfile(DEBIAN_XSESSION_PATH);


def is_crash_reporting_enabled(config):
  # Enable crash reporting for Google hosts or when usage_stats_consent is true.
  return (config.get("host_owner", "").endswith("@google.com") or
          config.get("usage_stats_consent", False))


def get_pipewire_session_manager():
  """Returns the PipeWire session manager supported on this system (either
  "wireplumber" or "pipewire-media-session"), or None if a supported PipeWire
  installation is not found."""

  if shutil.which("pipewire") is None:
    logging.warning("PipeWire not found. Not enabling PipeWire audio support.")
    return None

  try:
    version_output = subprocess.check_output(["pipewire", "--version"],
                                             universal_newlines=True)
  except subprocess.CalledProcessError as e:
    logging.warning("Failed to execute pipewire. Not enabling PipeWire audio"
                    + " support: " + str(e))
    return None

  match = re.search(r"pipewire (\S+)$", version_output, re.MULTILINE)
  if not match:
    logging.warning("Failed to determine pipewire version. Not enabling"
                    + " PipeWire audio support.")
    return None

  try:
    pipewire_version = version.parse(match[1])
  except version.InvalidVersion as e:
    logging.warning("Failed to parse pipewire version. Not enabling PipeWire"
                    + " audio support: " + str(e))
    return None

  if pipewire_version < version.parse("0.3.53"):
    logging.warning("Installed pipewire version is too old. Not enabling"
                    + " PipeWire audio support.")
    return None

  session_manager = None
  for binary in ["wireplumber", "pipewire-media-session"]:
    if shutil.which(binary) is not None:
      session_manager = binary
      break

  if session_manager is None:
    logging.warning("No session manager found. Not enabling PipeWire audio"
                    + " support.")
    return None

  return session_manager


def terminate_process(pid, name):
  """Terminates the process with the given |pid|. Initially sends SIGTERM, but
  falls back to SIGKILL if the process fails to exit after 10 seconds. |name|
  is used for logging. Throws psutil.NoSuchProcess if the pid doesn't exist."""

  logging.info("Sending SIGTERM to %s proc (pid=%s)",
               name, pid)
  try:
    psutil_proc = psutil.Process(pid)
    psutil_proc.terminate()

    # Use a short timeout, to avoid delaying service shutdown if the
    # process refuses to die for some reason.
    psutil_proc.wait(timeout=10)
  except psutil.TimeoutExpired:
    logging.error("Timed out - sending SIGKILL")
    psutil_proc.kill()
  except psutil.Error:
    logging.error("Error terminating process")


def terminate_command_if_running(command_line):
  """Terminate any processes that match |command_line| (including all arguments)
  exactly. Note: this does not attempt to resolve the actual path to the
  executable. As such, arg0 much match exactly."""

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

      # Skip the current process.
      if process.pid == this_pid:
        continue

      # |cmdline| will be [python-interpreter, script-file, other arguments...]
      if psget(process.cmdline) == command_line:
        terminate_process(process.pid, command_line[0]);

    except (psutil.NoSuchProcess, psutil.AccessDenied):
      continue


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

  def get(self, key, default = None):
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
  """This manages the configuration for a host."""

  def __init__(self):
    # Note: Initial values are never used.
    self.host_id = None
    self.host_name = None
    self.host_secret_hash = None
    self.private_key = None

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


class SessionOutputFilterThread(threading.Thread):
  """Reads session log from a pipe and logs the output with the provided prefix
  for amount of time defined by time_limit, or indefinitely if time_limit is
  None."""

  def __init__(self, stream, prefix, time_limit):
    threading.Thread.__init__(self)
    self.stream = stream
    self.daemon = True
    self.prefix = prefix
    self.time_limit = time_limit

  def run(self):
    started_time = time.time()
    is_logging = True
    while True:
      try:
        line = self.stream.readline();
      except IOError as e:
        print("IOError when reading session output: ", e)
        return

      if line == b"":
        # EOF reached. Just stop the thread.
        return

      if not is_logging:
        continue

      if self.time_limit and time.time() - started_time >= self.time_limit:
        is_logging = False
        print("Suppressing rest of the session output.", flush=True)
      else:
        # Pass stream bytes through as is instead of decoding and encoding.
        sys.stdout.buffer.write(self.prefix.encode(sys.stdout.encoding) + line);
        sys.stdout.flush()


class Desktop(abc.ABC):
  """Manage a single virtual desktop"""

  def __init__(self, sizes, host_config, server_inhibitor=None,
               pipewire_inhibitor=None, session_inhibitor=None,
               host_inhibitor=None):
    self.sizes = sizes
    self.host_config = host_config
    self.server_proc = None
    self.pipewire_proc = None
    self.pipewire_pulse_proc = None
    self.pipewire_session_manager = None
    self.pipewire_session_manager_proc = None
    self.pre_session_proc = None
    self.session_proc = None
    self.host_proc = None
    self.child_env = None
    self.host_ready = False
    self.server_inhibitor = server_inhibitor
    self.pipewire_inhibitor = pipewire_inhibitor
    self.session_inhibitor = session_inhibitor
    self.host_inhibitor = host_inhibitor

    self._init_child_env();

    if self.server_inhibitor is None:
      self.server_inhibitor = RelaunchInhibitor("Display server")
    if self.pipewire_inhibitor is None:
      self.pipewire_inhibitor = RelaunchInhibitor("PipeWire")
    if self.session_inhibitor is None:
      self.session_inhibitor = RelaunchInhibitor("session")
    if self.host_inhibitor is None:
      self.host_inhibitor = RelaunchInhibitor("host")
    # Map of inhibitors to the corresponding host offline reason should that
    # session component fail. None indicates that the session component isn't
    # mandatory and its failure should not result in the host shutting down.
    self.inhibitors = {
        self.server_inhibitor: HOST_OFFLINE_REASON_X_SERVER_RETRIES_EXCEEDED,
        self.pipewire_inhibitor: None,
        self.session_inhibitor: HOST_OFFLINE_REASON_SESSION_RETRIES_EXCEEDED,
        self.host_inhibitor: HOST_OFFLINE_REASON_HOST_RETRIES_EXCEEDED
    }
    # Crash reporting is disabled by default.
    self.crash_reporting_enabled = False
    self.crash_uploader_proc = None
    self.crash_uploader_inhibitor = None

  def _init_child_env(self):
    self.child_env = dict(os.environ)

    self.child_env["CHROME_REMOTE_DESKTOP_SESSION"] = "1"

    # We used to create a separate profile/chrome config home for the virtual
    # session since the virtual session was independent of the local session in
    # curtain mode, and using the same Chrome profile between sessions would
    # lead to cross talk issues. This is no longer the case given modern desktop
    # environments don't support running two graphical sessions simultaneously.
    # Therefore, we don't set the env var unless the directory already exists.
    #
    # M61 introduced CHROME_CONFIG_HOME, which allows specifying a different
    # config base path while still using different user data directories for
    # different channels (Stable, Beta, Dev). For existing users who only have
    # chrome-profile, continue using CHROME_USER_DATA_DIR so they don't have to
    # set up their profile again.
    chrome_profile = os.path.join(CONFIG_DIR, "chrome-profile")
    chrome_config_home = os.path.join(CONFIG_DIR, "chrome-config")
    if (os.path.exists(chrome_profile)
        and not os.path.exists(chrome_config_home)):
      self.child_env["CHROME_USER_DATA_DIR"] = chrome_profile
    elif os.path.exists(chrome_config_home):
      self.child_env["CHROME_CONFIG_HOME"] = chrome_config_home

    # Ensure that the software-rendering GL drivers are loaded by the desktop
    # session, instead of any hardware GL drivers installed on the system.
    library_path = (
        "/usr/lib/mesa-diverted/%(arch)s-linux-gnu:"
        "/usr/lib/%(arch)s-linux-gnu/mesa:"
        "/usr/lib/%(arch)s-linux-gnu/dri:"
        "/usr/lib/%(arch)s-linux-gnu/gallium-pipe" %
        { "arch": platform.machine() })

    if "LD_LIBRARY_PATH" in self.child_env:
      library_path += ":" + self.child_env["LD_LIBRARY_PATH"]

    self.child_env["LD_LIBRARY_PATH"] = library_path

  def _setup_gnubby(self):
    self.ssh_auth_sockname = ("/tmp/chromoting.%s.ssh_auth_sock" %
                              os.environ["USER"])
    self.child_env["SSH_AUTH_SOCK"] = self.ssh_auth_sockname

  def _launch_pipewire(self, instance_name, runtime_path, sink_name):
    self.pipewire_session_manager = get_pipewire_session_manager()
    if self.pipewire_session_manager is None:
      return False

    try:
      for config_file in ["pipewire.conf", "pipewire-pulse.conf",
                          self.pipewire_session_manager + ".conf"]:
        with open(os.path.join(SCRIPT_DIR, config_file + ".template"),
                  "r") as infile, \
             open(os.path.join(runtime_path, config_file), "w") as outfile:
          template = string.Template(infile.read())
          outfile.write(template.substitute({
              "instance_name": instance_name,
              "runtime_path": runtime_path,
              "sink_name": sink_name}))

      logging.info("Launching pipewire")
      pipewire_cmd = ["pipewire", "-c",
                      os.path.join(runtime_path, "pipewire.conf")]
      # PulseAudio protocol support is built into PipeWire for the versions we
      # support. Invoking the pipewire binary directly instead of via the
      # pipewire-pulse symlink allows this to work even if the pipewire-pulse
      # package is not installed (e.g., if the user is still using PulseAudio
      # for local sessions).
      pipewire_pulse_cmd = ["pipewire", "-c",
                      os.path.join(runtime_path, "pipewire-pulse.conf")]
      session_manager_cmd = [
          self.pipewire_session_manager, "-c",
          os.path.join(runtime_path, self.pipewire_session_manager + ".conf")]

      # Terminate any stale processes before relaunching.
      for command in [pipewire_cmd, pipewire_pulse_cmd, session_manager_cmd]:
        terminate_command_if_running(command)

      self.pipewire_proc = subprocess.Popen(pipewire_cmd, env=self.child_env)
      self.pipewire_pulse_proc = subprocess.Popen(pipewire_pulse_cmd,
                                                  env=self.child_env)
      # MEDIA_SESSION_CONFIG_DIR is needed to use an absolute path with
      # pipewire-media-session.
      self.pipewire_session_manager_proc = subprocess.Popen(session_manager_cmd,
          env={**self.child_env, "MEDIA_SESSION_CONFIG_DIR": "/"})

      # Directs native PipeWire clients to the correct instance
      self.child_env["PIPEWIRE_REMOTE"] = instance_name

      return True
    except (IOError, OSError) as e:
      logging.error("Failed to start PipeWire: " + str(e))

      # Clean up any processes that did start
      for proc, name in [(self.pipewire_proc, "pipewire"),
                         (self.pipewire_pulse_proc, "pipewire-pulse"),
                         (self.pipewire_session_manager_proc,
                          self.pipewire_session_manager)]:
        if proc is not None:
          terminate_process(proc.pid, name)
      self.pipewire_proc = None
      self.pipewire_pulse_proc = None
      self.pipewire_session_manager_proc = None

    return False

  def _launch_pre_session(self):
    # Launch the pre-session script, if it exists. Returns true if the script
    # was launched, false if it didn't exist.
    if os.path.exists(SYSTEM_PRE_SESSION_FILE_PATH):
      pre_session_command = bash_invocation_for_script(
          SYSTEM_PRE_SESSION_FILE_PATH)

      logging.info("Launching pre-session: %s" % pre_session_command)
      self.pre_session_proc = subprocess.Popen(pre_session_command,
                                               stdin=subprocess.DEVNULL,
                                               stdout=subprocess.PIPE,
                                               stderr=subprocess.STDOUT,
                                               cwd=HOME_DIR,
                                               env=self.child_env)

      if not self.pre_session_proc.pid:
        raise Exception("Could not start pre-session")

      output_filter_thread = SessionOutputFilterThread(
          self.pre_session_proc.stdout, "Pre-session output: ", None)
      output_filter_thread.start()

      return True
    return False

  def launch_session(self, server_args, backoff_time):
    """Launches process required for session and records the backoff time
    for inhibitors so that process restarts are not attempted again until
    that time has passed."""
    logging.info("Setting up and launching session")
    self._setup_gnubby()
    self._launch_server(server_args)
    if not self._launch_pre_session():
      # If there was no pre-session script, launch the session immediately.
      self.launch_desktop_session()
    self.server_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME,
                                      backoff_time)
    self.session_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME,
                                     backoff_time)

  def _wait_for_setup_before_host_launch(self):
    """
    If a virtual desktop needs to do some setup before launching the host
    process, it can override this method and ensure that the required setup is
    done before returning from this process.
    """
    pass

  def launch_host(self, extra_start_host_args, backoff_time):
    self._wait_for_setup_before_host_launch()
    logging.info("Launching host process")

    # Start remoting host
    args = [HOST_BINARY_PATH, "--host-config=-"]
    if self.audio_pipe:
      args.append("--audio-pipe-name=%s" % self.audio_pipe)
    if self.ssh_auth_sockname:
      args.append("--ssh-auth-sockname=%s" % self.ssh_auth_sockname)

    args.extend(extra_start_host_args)

    # Have the host process use SIGUSR1 to signal a successful start.
    def sigusr1_handler(signum, frame):
      _ = signum, frame
      logging.info("Host ready to receive connections.")
      self.host_ready = True
      ParentProcessLogger.release_parent_if_connected(True)

    signal.signal(signal.SIGUSR1, sigusr1_handler)
    args.append("--signal-parent")

    logging.info(args)
    self.host_proc = subprocess.Popen(args, env=self.child_env,
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

  def enable_crash_reporting(self):
    logging.info("Configuring crash reporting")
    self.crash_reporting_enabled = True
    self.crash_uploader_inhibitor = RelaunchInhibitor("Crash uploader")
    self.inhibitors[self.crash_uploader_inhibitor] = (
        HOST_OFFLINE_REASON_CRASH_UPLOADER_RETRIES_EXCEEDED
    )

  def launch_crash_uploader(self, backoff_time):
    if not self.crash_reporting_enabled:
      return

    logging.info("Launching crash uploader")

    args = [CRASH_UPLOADER_PATH]
    self.crash_uploader_proc = subprocess.Popen(args, env=self.child_env)

    if not self.crash_uploader_proc.pid:
      raise Exception("Could not start crash-uploader")

    self.crash_uploader_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME,
                                               backoff_time)

  def cleanup(self):
    """Send SIGTERM to all procs and wait for them to exit. Will fallback to
    SIGKILL if a process doesn't exit within 10 seconds.
    """
    for proc, name in [(self.host_proc, "host"),
                       (self.crash_uploader_proc, "crash-uploader"),
                       (self.session_proc, "session"),
                       (self.pre_session_proc, "pre-session"),
                       (self.pipewire_proc, "pipewire"),
                       (self.pipewire_pulse_proc, "pipewire-pulse"),
                       (self.pipewire_session_manager_proc,
                        self.pipewire_session_manager),
                       (self.server_proc, "display server")]:
      if proc is not None:
        terminate_process(proc.pid, name)
    self.server_proc = None
    self.pipewire_proc = None
    self.pipewire_pulse_proc = None
    self.pipewire_session_manager_proc = None
    self.pre_session_proc = None
    self.session_proc = None
    self.host_proc = None
    self.crash_uploader_proc = None

  def report_offline_reason(self, reason):
    """Attempt to report the specified offline reason to the registry. This
    is best effort, and requires a valid host config.
    """
    logging.info("Attempting to report offline reason: " + reason)
    args = [HOST_BINARY_PATH, "--host-config=-",
            "--report-offline-reason=" + reason]
    proc = subprocess.Popen(args, env=self.child_env, stdin=subprocess.PIPE)
    proc.communicate(json.dumps(self.host_config.data).encode('UTF-8'))

  def on_process_exit(self, pid, status):
    """Checks for which process has exited and whether or not the exit was
    expected. Returns a boolean indicating whether or not tear down of the
    processes is needed."""
    tear_down = False
    pipewire_process = False
    if self.server_proc is not None and pid == self.server_proc.pid:
      logging.info("Display server process terminated")
      self.server_proc = None
      self.server_inhibitor.record_stopped(expected=False)
      tear_down = True

    if (self.pre_session_proc is not None and
        pid == self.pre_session_proc.pid):
      self.pre_session_proc = None
      if status == 0:
        logging.info("Pre-session terminated successfully. Starting session.")
        self.launch_desktop_session()
      else:
        logging.info("Pre-session failed. Tearing down.")
        # The pre-session may have exited on its own or been brought down by
        # the display server dying. Check if the display server is still running
        # so we know whom to penalize.
        if self.check_server_responding():
          # Pre-session and session use the same inhibitor.
          self.session_inhibitor.record_stopped(expected=False)
        else:
          self.server_inhibitor.record_stopped(expected=False)
        # Either way, we want to tear down the session.
        tear_down = True

    if self.pipewire_proc is not None and pid == self.pipewire_proc.pid:
      logging.info("PipeWire process terminated")
      self.pipewire_proc = None
      pipewire_process = True

    if (self.pipewire_pulse_proc is not None
        and pid == self.pipewire_pulse_proc.pid):
      logging.info("PipeWire-Pulse process terminated")
      self.pipewire_pulse_proc = None
      pipewire_process = True

    if (self.pipewire_session_manager_proc is not None
        and pid == self.pipewire_session_manager_proc.pid):
      logging.info(self.pipewire_session_manager + " process terminated")
      self.pipewire_session_manager_proc = None
      pipewire_process = True

    if pipewire_process:
      self.pipewire_inhibitor.record_stopped(expected=False)
      # Terminate other PipeWire-related processes to start fresh.
      for proc, name in [(self.pipewire_proc, "pipewire"),
                         (self.pipewire_pulse_proc, "pipewire-pulse"),
                         (self.pipewire_session_manager_proc,
                          self.pipewire_session_manager)]:
        if proc is not None:
          terminate_process(proc.pid, name)
      self.pipewire_proc = None
      self.pipewire_pulse_proc = None
      self.pipewire_session_manager_proc = None

    if self.session_proc is not None and pid == self.session_proc.pid:
      logging.info("Session process terminated")
      self.session_proc = None
      # The session may have exited on its own or been brought down by the
      # display server dying. Check if the display server is still running so we
      # know whom to penalize.
      if self.check_server_responding():
        self.session_inhibitor.record_stopped(expected=False)
      else:
        self.server_inhibitor.record_stopped(expected=False)
      # Either way, we want to tear down the session.
      tear_down = True

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
      if self.check_server_responding():
        self.host_inhibitor.record_stopped(expected=False)
      else:
        self.server_inhibitor.record_stopped(expected=False)
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

  def aggregate_failure_count(self):
    failure_count = 0
    for inhibitor, offline_reason in self.inhibitors.items():
      if inhibitor.running:
        inhibitor.record_stopped(True)
      # Only count mandatory processes
      if offline_reason is not None:
        failure_count += inhibitor.failures
    return failure_count

  def setup_audio(self, host_id, backoff_time):
    """Launches a CRD-specific instance of PipeWire for audio forwarding within
    the session and sets up the restart inhibitor for it, if supported on this
    system. Otherwise, falls back to writing a legacy PulseAudio
    configuration."""
    self.audio_pipe = None

    # PipeWire and PulseAudio uses UNIX sockets for communication. The length of
    # a UNIX socket name is limited to 108 characters, so audio will not work
    # properly if the path is too long. To workaround this problem we use only
    # first 10 symbols (60 bits) of the base64url-encoded hash of the host id.
    suffix = base64.urlsafe_b64encode(hashlib.sha256(
        host_id.encode("utf-8")).digest()).decode("ascii")[0:10]
    runtime_dirname = "crd_audio#%s" % suffix
    pipewire_instance = runtime_dirname + "/pipewire"
    runtime_path = os.path.join(
        xdg.BaseDirectory.get_runtime_dir(strict=False), runtime_dirname)
    if len(runtime_path) + len("/pipewire") >= 108:
      logging.error("Audio will not be enabled because audio UNIX socket path" +
                    " is too long.")
      self.pipewire_inhibitor.disable()
      return

    sink_name = "chrome_remote_desktop_session"
    pipe_name = os.path.join(runtime_path, "fifo_output")

    try:
      if not os.path.exists(runtime_path):
        os.mkdir(runtime_path)
    except IOError as e:
      logging.error("Failed to create audio runtime path: " + str(e))
      self.pipewire_inhibitor.disable()
      return

    self.audio_pipe = pipe_name

    # Used both with PipeWire-Pulse and PulseAudio
    self.child_env["PULSE_RUNTIME_PATH"] = runtime_path
    self.child_env["PULSE_SINK"] = sink_name

    # Configure and launch PipeWire if supported on this system.
    if self._launch_pipewire(pipewire_instance, runtime_path, sink_name):
      self.pipewire_inhibitor.record_started(MINIMUM_PROCESS_LIFETIME,
                                             backoff_time)
      return

    self.pipewire_inhibitor.disable()

    # Used only by the PulseAudio daemon in a legacy setup.
    self.child_env["PULSE_CONFIG_PATH"] = runtime_path
    self.child_env["PULSE_STATE_PATH"] = runtime_path

    # Write a legacy PulseAudio config. This isn't used by PipeWire, but allows
    # users with a legacy configuration without PipeWire where PulseAudio is
    # started by their session to continue functioning.
    try:
      with open(os.path.join(runtime_path, "daemon.conf"), "w") as pulse_config:
        pulse_config.write("default-sample-format = s16le\n")
        pulse_config.write("default-sample-rate = 48000\n")
        pulse_config.write("default-sample-channels = 2\n")

      with open(os.path.join(runtime_path, "default.pa"), "w") as pulse_script:
        pulse_script.write("load-module module-native-protocol-unix\n")
        pulse_script.write(
            ("load-module module-pipe-sink sink_name=%s file=\"%s\" " +
             "rate=48000 channels=2 format=s16le\n") %
            (sink_name, pipe_name))
    except IOError as e:
      logging.error("Failed to write pulseaudio config: " + str(e))

  @abc.abstractmethod
  def launch_desktop_session(self):
    """Start desktop session."""
    pass

  @abc.abstractmethod
  def check_server_responding(self):
    """Checks if the display server is responding to connections."""
    return False


class WaylandDesktop(Desktop):
  """Manage a single virtual wayland based desktop"""

  WL_SOCKET_CHECK_DELAY_SECONDS = 1
  WL_SOCKET_CHECK_TIMEOUT_SECONDS = 5
  WL_SERVER_REPLY_TIMEOUT_SECONDS = 1
  # We scan for the unused socket starting from number 1. If we are not able to
  # find anything between 1 and 100 then we error out since there could be a
  # socket leak and we don't want to keep retrying forever.
  MAX_WAYLAND_SOCKET_NUM = 100

  def __init__(self, sizes, host_config):
    self.debug = False
    self._wayland_socket = None
    self._runtime_dir = None
    super(WaylandDesktop, self).__init__(sizes, host_config)
    self.inhibitors[self.server_inhibitor] \
        = HOST_OFFLINE_REASON_WAYLAND_SERVER_RETRIES_EXCEEDED
    global g_desktop
    assert(g_desktop is None)
    g_desktop = self

  @property
  def runtime_dir(self):
    if not self._runtime_dir:
      self._runtime_dir = RUNTIME_DIR_TEMPLATE % os.getuid()
    return self._runtime_dir

  def _init_child_env(self):
    super(WaylandDesktop, self)._init_child_env()
    self.child_env["GDK_BACKEND"] = "wayland,x11"
    self.child_env["XDG_SESSION_TYPE"] = "wayland"
    self.child_env["XDG_RUNTIME_DIR"] = self.runtime_dir

    if self.debug:
      self.child_env["G_MESSAGES_DEBUG"] = "all"
      self.child_env["GDK_DEBUG"]  = "all"
      self.child_env["G_DEBUG"] = "fatal-criticals"
      self.child_env["WAYLAND_DEBUG"] = "1"

  def _get_unused_wayland_socket(self):
    """
    Return a candidate wayland socket that is not already taken by another
    compositor.
    """
    socket_num = starting_socket_num = 0
    full_sock_path = os.path.join(self.runtime_dir, "wayland-%s" % socket_num)
    while ((os.path.exists(full_sock_path)) and
            socket_num <= self.MAX_WAYLAND_SOCKET_NUM):
      socket_num += 1
      full_sock_path = os.path.join(self.runtime_dir, "wayland-%s" % socket_num)
    if socket_num > self.MAX_WAYLAND_SOCKET_NUM:
      logging.error("Unable to find an unused wayland socket (searched between "
                    "'wayland-%s' to 'wayland-%s' under runtime directory",
                    starting_socket_num,
                    self.MAX_WAYLAND_SOCKET_NUM, self.runtime_dir)
      return None
    return "wayland-%s" % socket_num

  @staticmethod
  def _is_gnome_session_present():
    if not shutil.which(GNOME_SESSION):
      logging.warning("Unable to find '%s' on the host" % GNOME_SESSION)
      return False
    return True

  def _launch_server(self, *args, **kwargs):
    if not self._is_gnome_session_present():
      logging.error("Only GNOME based wayland hosts are supported currently. "
                    "If the host is a GNOME host, please ensure that "
                    "'gnome-shell' is installed on it")
      # Error won't be fixed without user intervention so we quit here without
      # attempting to relaunch.
      sys.exit(1)
    logging.info("Launching wayland server.")
    self._wayland_socket = self._get_unused_wayland_socket()
    if self._wayland_socket is None:
      logging.error("Unable to find unused wayland socket, running compositor "
                    "is going to fail")
      sys.exit(1)
    else:
      self.child_env["WAYLAND_DISPLAY"] = self._wayland_socket
    self.server_proc = subprocess.Popen([GNOME_SESSION],
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT,
                                        env=self.child_env)

    if not self.server_proc.pid:
      raise Exception("Could not start wayland session")

    output_filter_thread = SessionOutputFilterThread(self.server_proc.stdout,
        "Wayland server output: ", SERVER_OUTPUT_TIME_LIMIT_SECONDS)
    output_filter_thread.start()

  def _wait_for_wayland_compositor_running(self):
    """
    Waits for wayland socket to be created by the wayland compositor. Returns
    true if socket is created within the allowed timeout, else false.
    """
    full_socket_path = os.path.join(self.runtime_dir, self._wayland_socket)
    start_time = time.time()
    while not os.path.exists(full_socket_path):
      time_passed = time.time() - start_time
      if time_passed >= self.WL_SOCKET_CHECK_TIMEOUT_SECONDS:
        break
      logging.info("Wayland socket not yet present. Will wait for %s seconds "
                   "for compositor to create it (remaining wait time: %s "
                   "seconds)" %
                   (self.WL_SOCKET_CHECK_DELAY_SECONDS,
                    int(self.WL_SOCKET_CHECK_TIMEOUT_SECONDS - time_passed)))
      time.sleep(self.WL_SOCKET_CHECK_DELAY_SECONDS)
    if not os.path.exists(full_socket_path):
      logging.error("Waited for wayland compositor to create wayland "
                    "socket: %s, but it didn't happen in %s seconds" %
                    (full_socket_path, self.WL_SOCKET_CHECK_TIMEOUT_SECONDS))
      return False
    logging.info("Wayland socket detected in %s seconds: " %
                 str(time.time() - start_time))
    return True

  def launch_desktop_session(self):
    """
    Restarts the portal services so that they can connect to the wayland socket.
    This helps host process to talk to call into the the xdg-desktop-portal
    APIs.
    """
    if not self._wait_for_wayland_compositor_running():
      logging.error("Aborting wayland session since compositor isn't running")
      sys.exit(1)
    logging.info("Wayland compositor is running, restarting the portal "
                 "services now")
    try:
      subprocess.check_output(["systemctl", "--user", "import-environment"],
                              stderr=subprocess.STDOUT,
                              env=self.child_env)
    except subprocess.CalledProcessError as err:
      logging.error("Unable to import env vars into systemd, "
                    "returncode: %s, output: %s" % (err.returncode,
                                                    err.output))
      # Host process will not be functional without these services.
      sys.exit(1)

    try:
      subprocess.check_output(["systemctl", "--user", "restart",
                               "xdg-desktop-portal",
                               "xdg-desktop-portal-gnome",
                               "xdg-desktop-portal-gtk"],
                               stderr=subprocess.STDOUT, env=self.child_env)
    except subprocess.CalledProcessError as err:
      logging.error("Unable to restart portal services on the host, "
                    "returncode: %s, output: %s" % (err.returncode, err.output))
      # Host process will not be functional without these services.
      sys.exit(1)
    logging.info("Done restarting the portal services")

  def _wait_for_setup_before_host_launch(self):
    return self._wait_for_wayland_compositor_running()

  def cleanup(self):
    if self.host_proc is not None:
      logging.info("Sending SIGTERM to host proc (pid=%s)", self.host_proc.pid)
      try:
        psutil_proc = psutil.Process(self.host_proc.pid)
        psutil_proc.terminate()

        # Use a short timeout, to avoid delaying service shutdown if the
        # process refuses to die for some reason.
        psutil_proc.wait(timeout=10)
      except psutil.TimeoutExpired:
        logging.error("Timed out - sending SIGKILL")
        psutil_proc.kill()
      except psutil.Error:
        logging.error("Error terminating process")
      self.host_proc = None

    # We only support gnome-session, which is currently managed by CRD itself.
    logging.info("Executing %s" % GNOME_SESSION_QUIT)
    if shutil.which(GNOME_SESSION_QUIT):
      cleanup_proc = subprocess.Popen(
        [GNOME_SESSION_QUIT, "--force", "--no-prompt"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=self.child_env)
      stdout, stderr = cleanup_proc.communicate()
      if stderr:
        logging.error("Failed to execute %s:\n%s" %
                      (GNOME_SESSION_QUIT, stderr))
      self.session_proc = None
    else:
      logging.warning("No %s found on the system" % GNOME_SESSION_QUIT)

    super(WaylandDesktop, self).cleanup()
    if self._wayland_socket:
      full_socket_path = os.path.join(self.runtime_dir, self._wayland_socket)
      for to_remove in (full_socket_path, "%s.lock" % full_socket_path):
        try:
          os.remove(to_remove)
        except FileNotFoundError:
          pass
      self._wayland_socket = None

  def check_server_responding(self):
    """
    Connects to the server that is listening on the wayland socket.
    If the connection succeeds, it means that the server is still up and
    running.
    """
    try:
      with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(os.path.join(self.runtime_dir, self._wayland_socket))
        # Asks the server for the global registry object
        # (See: https://wayland-book.com/registry.html)
        sock.sendall(struct.pack("<III", 0x00000001, 0x000C0001, 0x00000002))

        num_bytes_received = 0
        NUM_BYTES_EXPECTED = 32
        # We don't want to wait forever for a reply so we set a timeout here.
        sock.settimeout(self.WL_SERVER_REPLY_TIMEOUT_SECONDS)
        while num_bytes_received < NUM_BYTES_EXPECTED:
            data = sock.recv(NUM_BYTES_EXPECTED)
            if len(data) == 0:  # Expect empty reply if server dies
               break
            num_bytes_received += len(data)
            logging.debug("Wayland server replied with: %s" % data)
        if not num_bytes_received:
          # If we don't receive a reply at all then the server is likely not
          # listening on the socket.
          return False
    except socket.error as err:
        logging.error("Wayland server is not responding: %s" % err)
        return False
    return True


class XDesktop(Desktop):
  """Manage a single virtual X desktop"""

  def __init__(self, sizes, host_config):
    super(XDesktop, self).__init__(sizes, host_config)
    self.xorg_conf = None
    self.audio_pipe = None
    self.server_supports_randr = False
    self.randr_add_sizes = False
    self.ssh_auth_sockname = None
    self.use_xvfb = self.should_use_xvfb()
    global g_desktop
    assert(g_desktop is None)
    g_desktop = self

  @staticmethod
  def should_use_xvfb():
    """Return whether XVFB should be used. This will be true if USE_XVFB_ENV_VAR
    is set, or if installed dependencies can't support Xorg+Dummy. Note that
    this method performs expensive IO so the output should be cached."""

    if USE_XVFB_ENV_VAR in os.environ:
      return True

    # Check if xserver-xorg-video-dummy is up-to-date. Older versions don't
    # support the DUMMY* outputs and can't be used.
    # Unfortunately, dummy_drv.so doesn't seem to have any version info so we
    # have to query the dpkg database.
    try:
      video_dummy_info = subprocess.check_output(
          ['dpkg-query', '-s', 'xserver-xorg-video-dummy'])
      match = re.search(
          br'^Version: (\S+)$', video_dummy_info, re.MULTILINE)
      if not match:
        logging.error('Version line is not found')
        return True
      version = match[1]
      retcode = subprocess.call(
          ['dpkg', '--compare-versions', version, 'ge', '1:0.4.0'])
      if retcode != 0:
        logging.info('xserver-xorg-video-dummy is not up-to-date')
        return True
    except subprocess.CalledProcessError:
      logging.info('xserver-xorg-video-dummy is not installed')
      return True
    except Exception as e:
      logging.warning(
          'Failed to get xserver-xorg-video-dummy version: ' + str(e))

    return False

  @staticmethod
  def get_unused_display_number():
    """Return a candidate display number for which there is currently no
    X Server lock file"""
    display = FIRST_X_DISPLAY_NUMBER
    while os.path.exists(X_LOCK_FILE_TEMPLATE % display):
      display += 1
    return display

  def _init_child_env(self):
    super(XDesktop, self)._init_child_env()
    # Force GDK to use the X11 backend, as otherwise parts of the host that use
    # GTK can end up connecting to an active Wayland display instead of the
    # CRD X11 session.
    self.child_env["GDK_BACKEND"] = "x11"
    self.child_env["XDG_SESSION_TYPE"] = "x11"

  def launch_session(self, *args, **kwargs):
    logging.info("Launching X server and X session.")
    super(XDesktop, self).launch_session(*args, **kwargs)

  # Returns child environment not containing TMPDIR.
  # Certain values of TMPDIR can break the X server (crbug.com/672684), so we
  # want to make sure it isn't set in the environment used to start the server.
  def _x_env(self):
    if "TMPDIR" not in self.child_env:
      return self.child_env
    else:
      env_copy = dict(self.child_env)
      del env_copy["TMPDIR"]
      return env_copy

  def check_server_responding(self):
    """Checks if the X server is responding to connections."""
    exit_code = subprocess.call("xdpyinfo", env=self.child_env,
                                stdout=subprocess.DEVNULL)
    return exit_code == 0

  def _wait_for_x(self):
    # Wait for X to be active.
    for _test in range(20):
      if self.check_server_responding():
        logging.info("X server is active.")
        return
      time.sleep(0.5)

    raise Exception("Could not connect to X server.")

  def _launch_xvfb(self, display, x_auth_file, extra_x_args):
    max_width = max([width for width, height in self.sizes])
    max_height = max([height for width, height in self.sizes])

    logging.info("Starting Xvfb on display :%d" % display)
    screen_option = "%dx%dx24" % (max_width, max_height)
    self.server_proc = subprocess.Popen(
        ["Xvfb", ":%d" % display,
         "-auth", x_auth_file,
         "-nolisten", "tcp",
         "-noreset",
         "-screen", "0", screen_option
        ] + extra_x_args, env=self._x_env())
    if not self.server_proc.pid:
      raise Exception("Could not start Xvfb.")

    self._wait_for_x()

    exit_code = subprocess.call("xrandr", env=self.child_env,
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL)
    if exit_code == 0:
      # RandR is supported
      self.server_supports_randr = True
      self.randr_add_sizes = True

  def _launch_xorg(self, display, x_auth_file, extra_x_args):
    config_dir = tempfile.mkdtemp(prefix="chrome_remote_desktop_")
    with open(os.path.join(config_dir, "xorg.conf"), "wb") as config_file:
      config_file.write(gen_xorg_config().encode())

    self.server_supports_randr = True
    self.randr_add_sizes = True
    self.xorg_conf = config_file.name

    xorg_binary = "/usr/lib/xorg/Xorg";
    if not os.access(xorg_binary, os.X_OK):
      xorg_binary = "Xorg";

    logging.info("Starting %s on display :%d" % (xorg_binary, display))
    # We use the child environment so the Xorg server picks up the Mesa libGL
    # instead of any proprietary versions that may be installed, thanks to
    # LD_LIBRARY_PATH.
    # Note: This prevents any environment variable the user has set from
    # affecting the Xorg server.
    self.server_proc = subprocess.Popen(
        [xorg_binary, ":%d" % display,
         "-auth", x_auth_file,
         "-nolisten", "tcp",
         "-noreset",
         # Disable logging to a file and instead bump up the stderr verbosity
         # so the equivalent information gets logged in our main log file.
         "-logfile", "/dev/null",
         "-verbose", "3",
         "-configdir", config_dir,
         # Pass a non-existent file, to prevent Xorg from reading the default
         # config file: /etc/X11/xorg.conf
         "-config", os.path.join(config_dir, "none")
        ] + extra_x_args, env=self._x_env())
    if not self.server_proc.pid:
      raise Exception("Could not start Xorg.")
    self._wait_for_x()

  def _launch_server(self, extra_x_args):
    x_auth_file = os.path.expanduser("~/.Xauthority")
    self.child_env["XAUTHORITY"] = x_auth_file
    display = self.get_unused_display_number()

    # Run "xauth add" with |child_env| so that it modifies the same XAUTHORITY
    # file which will be used for the X session.
    exit_code = subprocess.call("xauth add :%d . `mcookie`" % display,
                                env=self.child_env, shell=True)
    if exit_code != 0:
      raise Exception("xauth failed with code %d" % exit_code)

    # Disable the Composite extension iff the X session is the default
    # Unity-2D, since it uses Metacity which fails to generate DAMAGE
    # notifications correctly. See crbug.com/166468.
    x_session = choose_x_session()
    if (len(x_session) == 2 and
        x_session[1] == "/usr/bin/gnome-session --session=ubuntu-2d"):
      extra_x_args.extend(["-extension", "Composite"])

    self.child_env["DISPLAY"] = ":%d" % display

    if self.use_xvfb:
      self._launch_xvfb(display, x_auth_file, extra_x_args)
    else:
      self._launch_xorg(display, x_auth_file, extra_x_args)

    # The remoting host expects the server to use "evdev" keycodes, but Xvfb
    # starts configured to use the "base" ruleset, resulting in XKB configuring
    # for "xfree86" keycodes, and screwing up some keys. See crbug.com/119013.
    # Reconfigure the X server to use "evdev" keymap rules.  The X server must
    # be started with -noreset otherwise it'll reset as soon as the command
    # completes, since there are no other X clients running yet.
    exit_code = subprocess.call(["setxkbmap", "-rules", "evdev"],
                                env=self.child_env)
    if exit_code != 0:
      logging.error("Failed to set XKB to 'evdev'")

    if not self.server_supports_randr:
      return

    # Register the screen sizes with RandR, if needed.  Errors here are
    # non-fatal; the X server will continue to run with the dimensions from
    # the "-screen" option.
    if self.randr_add_sizes:
      refresh_rates = ["60"]
      try:
        proc_num = subprocess.check_output("nproc", universal_newlines=True)

        # Keep the proc_num logic in sync with desktop_resizer_x11.cc
        if (int(proc_num) > 16):
          refresh_rates.append("120")
      except (ValueError, OSError, subprocess.CalledProcessError) as e:
        logging.error("Failed to retrieve processor count: " + str(e))

      output_names = (
          ["screen"]
          if self.use_xvfb
          else ["DUMMY0","DUMMY1","DUMMY2","DUMMY3"])

      for output_name in output_names:
        for refresh_rate in refresh_rates:
          for width, height in self.sizes:
            # This sets dot-clock, vtotal and htotal such that the computed
            # refresh-rate will have a realistic value:
            # refresh rate = dot-clock / (vtotal * htotal).
            label = "%dx%d_%s" % (width, height, refresh_rate)
            args = ["xrandr", "--newmode", label, refresh_rate, str(width), "0",
                    "0", "1000", str(height), "0", "0", "1000"]
            subprocess.call(args, env=self.child_env, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
            args = ["xrandr", "--addmode", output_name, label]
            subprocess.call(args, env=self.child_env, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)

    # Set the initial mode to the first size specified, otherwise the X server
    # would default to (max_width, max_height), which might not even be in the
    # list.
    initial_size = self.sizes[0]
    label = "%dx%d" % initial_size
    args = ["xrandr", "-s", label]
    subprocess.call(args, env=self.child_env, stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL)

    # Set the physical size of the display so that the initial mode is running
    # at approximately 96 DPI, since some desktops require the DPI to be set
    # to something realistic.
    args = ["xrandr", "--dpi", "96"]
    subprocess.call(args, env=self.child_env, stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL)

    if self.use_xvfb:
      # Monitor for any automatic resolution changes from the desktop
      # environment. This is needed only for Xvfb sessions because Xvfb sets
      # the first mode to be the maximum supported resolution, and some
      # desktop-environments would mistakenly set this as the preferred mode,
      # leading to a huge desktop with tiny text. With Xorg, the modes are
      # all reasonably sized, so the problem doesn't occur.
      args = [SCRIPT_PATH, "--watch-resolution", str(initial_size[0]),
              str(initial_size[1])]

      # It is not necessary to wait() on the process here, as this script's main
      # loop will reap the exit-codes of all child processes.
      subprocess.Popen(args, env=self.child_env, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)

  def launch_desktop_session(self):
    # Start desktop session.
    # The /dev/null input redirection is necessary to prevent the X session
    # reading from stdin.  If this code runs as a shell background job in a
    # terminal, any reading from stdin causes the job to be suspended.
    # Daemonization would solve this problem by separating the process from the
    # controlling terminal.
    xsession_command = choose_x_session()
    if xsession_command is None:
      raise Exception("Unable to choose suitable X session command.")

    logging.info("Launching X session: %s" % xsession_command)
    self.session_proc = subprocess.Popen(xsession_command,
                                         stdin=subprocess.DEVNULL,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT,
                                         cwd=HOME_DIR,
                                         env=self.child_env)

    if not self.session_proc.pid:
      raise Exception("Could not start X session")

    output_filter_thread = SessionOutputFilterThread(self.session_proc.stdout,
        "Session output: ", SESSION_OUTPUT_TIME_LIMIT_SECONDS)
    output_filter_thread.start()


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


def bash_invocation_for_script(script):
  """Chooses the appropriate bash command to run the provided script."""
  if os.path.exists(script):
    if os.access(script, os.X_OK):
      # "/bin/sh -c" is smart about how to execute the session script and
      # works in cases where plain exec() fails (for example, if the file is
      # marked executable, but is a plain script with no shebang line).
      return ["/bin/sh", "-c", shlex.quote(script)]
    else:
      # If this is a system-wide session script, it should be run using the
      # system shell, ignoring any login shell that might be set for the
      # current user.
      return ["/bin/sh", script]

def choose_x_session():
  """Chooses the most appropriate X session command for this system.

  Returns:
    A string containing the command to run, or a list of strings containing
    the executable program and its arguments, which is suitable for passing as
    the first parameter of subprocess.Popen().  If a suitable session cannot
    be found, returns None.
  """
  XSESSION_FILES = [
    SESSION_FILE_PATH,
    SYSTEM_SESSION_FILE_PATH ]
  for startup_file in XSESSION_FILES:
    startup_file = os.path.expanduser(startup_file)
    if os.path.exists(startup_file):
      return bash_invocation_for_script(startup_file)

  # If there's no configuration, show the user a session chooser.
  return [HOST_BINARY_PATH, "--type=xsession_chooser"]

class ParentProcessLogger(object):
  """Redirects logs to the parent process, until the host is ready or quits.

  This class creates a pipe to allow logging from the daemon process to be
  copied to the parent process. The daemon process adds a log-handler that
  directs logging output to the pipe. The parent process reads from this pipe
  and writes the content to stderr. When the pipe is no longer needed (for
  example, the host signals successful launch or permanent failure), the daemon
  removes the log-handler and closes the pipe, causing the the parent process
  to reach end-of-file while reading the pipe and exit.

  The file descriptor for the pipe to the parent process should be passed to
  the constructor. The (grand-)child process should call start_logging() when
  it starts, and then use logging.* to issue log statements, as usual. When the
  child has either succesfully started the host or terminated, it must call
  release_parent() to allow the parent to exit.
  """

  __instance = None

  def __init__(self, write_fd):
    """Constructor.

    Constructs the singleton instance of ParentProcessLogger. This should be
    called at most once.

    write_fd: The write end of the pipe created by the parent process. If
              write_fd is not a valid file descriptor, the constructor will
              throw either IOError or OSError.
    """
    # Ensure write_pipe is closed on exec, otherwise it will be kept open by
    # child processes (X, host), preventing the read pipe from EOF'ing.
    old_flags = fcntl.fcntl(write_fd, fcntl.F_GETFD)
    fcntl.fcntl(write_fd, fcntl.F_SETFD, old_flags | fcntl.FD_CLOEXEC)
    self._write_file = os.fdopen(write_fd, 'w')
    self._logging_handler = None
    ParentProcessLogger.__instance = self

  def _start_logging(self):
    """Installs a logging handler that sends log entries to a pipe, prefixed
    with the string 'MSG:'. This allows them to be distinguished by the parent
    process from commands sent over the same pipe.

    Must be called by the child process.
    """
    self._logging_handler = logging.StreamHandler(self._write_file)
    self._logging_handler.setFormatter(logging.Formatter(fmt='MSG:%(message)s'))
    logging.getLogger().addHandler(self._logging_handler)

  def _release_parent(self, success):
    """Uninstalls logging handler and closes the pipe, releasing the parent.

    Must be called by the child process.

    success: If true, write a "host ready" message to the parent process before
             closing the pipe.
    """
    if self._logging_handler:
      logging.getLogger().removeHandler(self._logging_handler)
      self._logging_handler = None
    if not self._write_file.closed:
      if success:
        try:
          self._write_file.write("READY\n")
          self._write_file.flush()
        except IOError:
          # A "broken pipe" IOError can happen if the receiving process
          # (remoting_user_session) has exited (probably due to timeout waiting
          # for the host to start).
          # Trapping the error here means the host can continue running.
          logging.info("Caught IOError writing READY message.")
      try:
        self._write_file.close()
      except IOError:
        pass

  @staticmethod
  def try_start_logging(write_fd):
    """Attempt to initialize ParentProcessLogger and start forwarding log
    messages.

    Returns False if the file descriptor was invalid (safe to ignore).
    """
    try:
      ParentProcessLogger(USER_SESSION_MESSAGE_FD)._start_logging()
      return True
    except (IOError, OSError):
      # One of these will be thrown if the file descriptor is invalid, such as
      # if the the fd got closed by the login shell. In that case, just continue
      # without sending log messages.
      return False

  @staticmethod
  def release_parent_if_connected(success):
    """If ParentProcessLogger is active, stop logging and release the parent.

    success: If true, signal to the parent that the script was successful.
    """
    instance = ParentProcessLogger.__instance
    if instance is not None:
      ParentProcessLogger.__instance = None
      instance._release_parent(success)


def run_command_with_group(command, group):
  """Run a command with a different primary group."""

  # This is implemented using sg, which is an odd character and will try to
  # prompt for a password if it can't verify the user is a member of the given
  # group, along with in a few other corner cases. (It will prompt in the
  # non-member case even if the group doesn't have a password set.)
  #
  # To prevent sg from prompting the user for a password that doesn't exist,
  # redirect stdin and detach sg from the TTY. It will still print something
  # like "Password: crypt: Invalid argument", so redirect stdout and stderr, as
  # well. Finally, have the shell unredirect them when executing user-session.
  #
  # It is also desirable to have some way to tell whether any errors are
  # from sg or the command, which is done using a pipe.

  def pre_exec(read_fd, write_fd):
    os.close(read_fd)

    # /bin/sh may be dash, which only allows redirecting file descriptors 0-9,
    # the minimum required by POSIX. Since there may be files open elsewhere,
    # move the relevant file descriptors to specific numbers under that limit.
    # Because this runs in the child process, it doesn't matter if existing file
    # descriptors are closed in the process. After, stdio will be redirected to
    # /dev/null, write_fd will be moved to 6, and the old stdio will be moved
    # to 7, 8, and 9.
    if (write_fd != 6):
      os.dup2(write_fd, 6)
      os.close(write_fd)
    os.dup2(0, 7)
    os.dup2(1, 8)
    os.dup2(2, 9)
    devnull = os.open(os.devnull, os.O_RDWR)
    os.dup2(devnull, 0)
    os.dup2(devnull, 1)
    os.dup2(devnull, 2)
    os.close(devnull)

    # os.setsid will detach subprocess from the TTY
    os.setsid()

  # Pipe to check whether sg successfully ran our command.
  read_fd, write_fd = os.pipe()
  try:
    # sg invokes the provided argument using /bin/sh. In that shell, first write
    # "success\n" to the pipe, which is checked later to determine whether sg
    # itself succeeded, and then restore stdio, close the extra file
    # descriptors, and exec the provided command.
    process = subprocess.Popen(
        ["sg", group,
         "echo success >&6; exec {command} "
           # Restore original stdio
           "0<&7 1>&8 2>&9 "
           # Close no-longer-needed file descriptors
           "6>&- 7<&- 8>&- 9>&-"
           .format(command=" ".join(map(shlex.quote, command)))],
        # It'd be nice to use pass_fds instead close_fds=False. Unfortunately,
        # pass_fds doesn't seem usable with remapping. It runs after preexec_fn,
        # which does the remapping, but complains if the specified fds don't
        # exist ahead of time.
        close_fds=False, preexec_fn=lambda: pre_exec(read_fd, write_fd))
    result = process.wait()
  except OSError as e:
    logging.error("Failed to execute sg: {}".format(e.strerror))
    if e.errno == errno.ENOENT:
      result = COMMAND_NOT_FOUND_EXIT_CODE
    else:
      result = COMMAND_NOT_EXECUTABLE_EXIT_CODE
    # Skip pipe check, since sg was never executed.
    os.close(read_fd)
    return result
  except KeyboardInterrupt:
    # Because sg is in its own session, it won't have gotten the interrupt.
    try:
      os.killpg(os.getpgid(process.pid), signal.SIGINT)
      result = process.wait()
    except OSError:
      logging.warning("Command may still be running")
      result = 1
  finally:
    os.close(write_fd)

  with os.fdopen(read_fd) as read_file:
    contents = read_file.read()
  if contents != "success\n":
    # No success message means sg didn't execute the command. (Maybe the user
    # is not a member of the group?)
    logging.error("Failed to access {} group. Is the user a member?"
                  .format(group))
    result = COMMAND_NOT_EXECUTABLE_EXIT_CODE

  return result


def run_command_as_root(command):
  if os.getenv("DISPLAY"):
    # TODO(rickyz): Add a Polkit policy that includes a more friendly
    # message about what this command does.
    command = ["/usr/bin/pkexec"] + command
  else:
    command = ["/usr/bin/sudo", "-k", "--"] + command

  return subprocess.call(command)


def exec_self_via_login_shell():
  """Attempt to run the user's login shell and run this script under it. This
  will allow the user's ~/.profile or similar to be processed, which may set
  environment variables to configure Chrome Remote Desktop."""
  args = [sys.argv[0], "--child-process"] + [arg for arg in sys.argv[1:]
                                             if arg != "--new-session"]
  try:
    shell = os.getenv("SHELL")

    if shell is not None:
      # Shells consider themselves a login shell if arg0 starts with a '-'.
      shell_arg0 = "-" + os.path.basename(shell)

      # First, ensure we can execute commands via the user's login shell. Some
      # users have an incorrect .profile or similar that breaks this.
      output = subprocess.check_output(
          [shell_arg0], executable=shell,
          input=b"exec echo CRD_SHELL_TEST_OUTPUT",
          timeout=15)

      if b"CRD_SHELL_TEST_OUTPUT" in output:
        # subprocess doesn't support calling exec without fork, so we need to
        # set up our pipe manually.
        read_fd, write_fd = os.pipe()
        # The command line should easily fit in the 16KiB pipe buffer.
        os.write(
            write_fd,
            b"exec " + os.fsencode(" ".join(map(shlex.quote, args))))
        os.close(write_fd)
        os.dup2(read_fd, 0)
        os.close(read_fd)
        os.execv(shell, [shell_arg0])
      else:
        logging.warning("Login shell doesn't execute standard input.")
    else:
      logging.warning("SHELL envirionment variable not set.")
  except Exception as e:
    logging.warning(str(e))

  logging.warning(
      "Failed to run via login shell; continuing without. Environment "
      "variables set via ~/.profile or similar won't be processed.")
  os.execv(args[0], args)


def start_via_user_session(foreground):
  # We need to invoke user-session
  command = [USER_SESSION_PATH, "start"]
  if foreground:
    command += ["--foreground"]
  command += ["--"] + sys.argv[1:]
  try:
    process = subprocess.Popen(command)
    result = process.wait()
  except OSError as e:
    if e.errno == errno.EACCES:
      # User may have just been added to the CRD group, in which case they
      # won't be able to execute user-session directly until they log out and
      # back in. In the mean time, we can try to switch to the CRD group and
      # execute user-session.
      result = run_command_with_group(command, CHROME_REMOTING_GROUP_NAME)
    else:
      logging.error("Could not execute {}: {}"
                    .format(USER_SESSION_PATH, e.strerror))
      if e.errno == errno.ENOENT:
        result = COMMAND_NOT_FOUND_EXIT_CODE
      else:
        result = COMMAND_NOT_EXECUTABLE_EXIT_CODE
  except KeyboardInterrupt:
    # Child will have also gotten the interrupt. Wait for it to exit.
    result = process.wait()

  return result


def cleanup():
  logging.info("Cleanup.")

  global g_desktop
  if g_desktop is not None:
    g_desktop.cleanup()
    if getattr(g_desktop, 'xorg_conf', None) is not None:
      os.remove(g_desktop.xorg_conf)
      os.rmdir(os.path.dirname(g_desktop.xorg_conf))

  g_desktop = None
  ParentProcessLogger.release_parent_if_connected(False)


class SignalHandler:
  """Reload the config file on SIGHUP. Since we pass the configuration to the
  host processes via stdin, they can't reload it, so terminate them. They will
  be relaunched automatically with the new config."""

  def __init__(self, host_config):
    self.host_config = host_config

  def __call__(self, signum, _stackframe):
    logging.info("Caught signal: " + str(signum))
    if signum == signal.SIGHUP:
      logging.info("SIGHUP caught, restarting host.")
      try:
        self.host_config.load()
      except (IOError, ValueError) as e:
        logging.error("Failed to load config: " + str(e))
      if g_desktop is not None and g_desktop.host_proc:
        g_desktop.host_proc.send_signal(signal.SIGTERM)
    else:
      # Exit cleanly so the atexit handler, cleanup(), gets called.
      raise SystemExit


class RelaunchInhibitor:
  """Helper class for inhibiting launch of a child process before a timeout has
  elapsed.

  A managed process can be in one of these states:
    running, not inhibited (running == True)
    stopped and inhibited (running == False and is_inhibited() == True)
    stopped but not inhibited (running == False and is_inhibited() == False)

  Attributes:
    label: Name of the tracked process. Only used for logging.
    running: Whether the process is currently running.
    earliest_relaunch_time: Time before which the process should not be
      relaunched, or 0 if there is no limit.
    failures: The number of times that the process ran for less than a
      specified timeout, and had to be inhibited.  This count is reset to 0
      whenever the process has run for longer than the timeout.
  """

  def __init__(self, label):
    self.label = label
    self.running = False
    self.disabled = False
    self.earliest_relaunch_time = 0
    self.earliest_successful_termination = 0
    self.failures = 0

  def is_inhibited(self):
    return (not self.running) and (time.time() < self.earliest_relaunch_time)

  def record_started(self, minimum_lifetime, relaunch_delay):
    """Record that the process was launched, and set the inhibit time to
    |timeout| seconds in the future."""
    self.earliest_relaunch_time = time.time() + relaunch_delay
    self.earliest_successful_termination = time.time() + minimum_lifetime
    self.running = True

  def record_stopped(self, expected):
    """Record that the process was stopped, and adjust the failure count
    depending on whether the process ran long enough. If the process was
    intentionally stopped (expected is True), the failure count will not be
    incremented."""
    self.running = False
    if time.time() >= self.earliest_successful_termination:
      self.failures = 0
    elif not expected:
      self.failures += 1
    logging.info("Failure count for '%s' is now %d", self.label, self.failures)

  def disable(self):
    """Disable launching this process, such as if the needed components are
    missing and launching it is never expected to succeed. Only makes sense for
    non-critical processes. (Otherwise, the script should just bail.)"""
    self.disabled = True


def relaunch_self():
  """Relaunches the session to pick up any changes to the session logic in case
  Chrome Remote Desktop has been upgraded. We return a special exit code to
  inform user-session that it should relaunch.
  """

  # cleanup run via atexit
  sys.exit(RELAUNCH_EXIT_CODE)


def waitpid_with_timeout(pid, deadline):
  """Wrapper around os.waitpid() which waits until either a child process dies
  or the deadline elapses.

  Args:
    pid: Process ID to wait for, or -1 to wait for any child process.
    deadline: Waiting stops when time.time() exceeds this value.

  Returns:
    (pid, status): Same as for os.waitpid(), except that |pid| is 0 if no child
    changed state within the timeout.

  Raises:
    Same as for os.waitpid().
  """
  while time.time() < deadline:
    pid, status = os.waitpid(pid, os.WNOHANG)
    if pid != 0:
      return (pid, status)
    time.sleep(1)
  return (0, 0)


def waitpid_handle_exceptions(pid, deadline):
  """Wrapper around os.waitpid()/waitpid_with_timeout(), which waits until
  either a child process exits or the deadline elapses, and retries if certain
  exceptions occur.

  Args:
    pid: Process ID to wait for, or -1 to wait for any child process.
    deadline: If non-zero, waiting stops when time.time() exceeds this value.
      If zero, waiting stops when a child process exits.

  Returns:
    (pid, status): Same as for waitpid_with_timeout(). |pid| is non-zero if and
    only if a child exited during the wait.

  Raises:
    Same as for os.waitpid(), except:
      OSError with errno==EINTR causes the wait to be retried (this can happen,
      for example, if this parent process receives SIGHUP).
      OSError with errno==ECHILD means there are no child processes, and so
      this function sleeps until |deadline|. If |deadline| is zero, this is an
      error and the OSError exception is raised in this case.
  """
  while True:
    try:
      if deadline == 0:
        pid_result, status = os.waitpid(pid, 0)
      else:
        pid_result, status = waitpid_with_timeout(pid, deadline)
      return (pid_result, status)
    except OSError as e:
      if e.errno == errno.EINTR:
        continue
      elif e.errno == errno.ECHILD:
        now = time.time()
        if deadline == 0:
          # No time-limit and no child processes. This is treated as an error
          # (see docstring).
          raise
        elif deadline > now:
          time.sleep(deadline - now)
        return (0, 0)
      else:
        # Anything else is an unexpected error.
        raise


def watch_for_resolution_changes(initial_size):
  """Watches for any resolution-changes which set the maximum screen resolution,
  and resets the initial size if this happens.

  The Ubuntu desktop has a component (the 'xrandr' plugin of
  unity-settings-daemon) which often changes the screen resolution to the
  first listed mode. This is the built-in mode for the maximum screen size,
  which can trigger excessive CPU usage in some situations. So this is a hack
  which waits for any such events, and undoes the change if it occurs.

  Sometimes, the user might legitimately want to use the maximum available
  resolution, so this monitoring is limited to a short time-period.
  """
  for _ in range(30):
    time.sleep(1)

    xrandr_output = subprocess.Popen(["xrandr"],
                                     stdout=subprocess.PIPE).communicate()[0]
    match = re.search(br'current (\d+) x (\d+), maximum (\d+) x (\d+)',
                      xrandr_output)

    # No need to handle ValueError. If xrandr fails to give valid output,
    # there's no point in continuing to monitor.
    current_size = (int(match.group(1)), int(match.group(2)))
    maximum_size = (int(match.group(3)), int(match.group(4)))

    if current_size != initial_size:
      # Resolution change detected.
      if current_size == maximum_size:
        # This was probably an automated change from unity-settings-daemon, so
        # undo it.
        label = "%dx%d" % initial_size
        args = ["xrandr", "-s", label]
        subprocess.call(args)
        args = ["xrandr", "--dpi", "96"]
        subprocess.call(args)

      # Stop monitoring after any change was detected.
      break


def setup_argument_parser():
  EPILOG = """This script is not intended for use by end-users. To configure
Chrome Remote Desktop, please install the app from the Chrome
Web Store: https://chrome.google.com/remotedesktop"""
  parser = argparse.ArgumentParser(
      usage="Usage: %(prog)s [options] [ -- [ X server options ] ]",
      epilog=EPILOG)
  parser.add_argument("-s", "--size", dest="size", action="append",
                      help="Dimensions of virtual desktop. This can be "
                      "specified multiple times to make multiple screen "
                      "resolutions available (if the X server supports this).")
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
  parser.add_argument("--add-user-as-root", dest="add_user_as_root",
                      action="store", metavar="USER",
                      help="Adds the specified user to the "
                      "chrome-remote-desktop group (must be run as root).")
  # The script is being run as a child process under the user-session binary.
  # Don't daemonize and use the inherited environment.
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
      print("The daemon is not currently running")
    else:
      print("Killing process %s" % proc.pid)
      proc.terminate()
      try:
        proc.wait(timeout=30)
      except psutil.TimeoutExpired:
        print("Timed out trying to kill daemon process")
        return 1
    return 0

  if options.reload:
    proc = get_daemon_proc(config_file)
    if proc is None:
      return 1
    proc.send_signal(signal.SIGHUP)
    return 0

  if options.enable_and_start:
    user = getpass.getuser()

    if os.path.isdir("/run/systemd/system"):
      # While systemd will generally prompt for a password via polkit if run by
      # a normal user, it won't properly fall back to prompting on the TTY if
      # stdin is redirected, such as is done by the start-host binary.
      # Additionally, some configurations can result in systemctl prompting the
      # user for their password multiple times, which can be confusing and
      # annoying. Running it as root avoids both issues.
      return run_command_as_root(["systemctl", "enable", "--now",
                                  "chrome-remote-desktop@" + user])
    else:
      try:
        if user in grp.getgrnam(CHROME_REMOTING_GROUP_NAME).gr_mem:
          logging.info("User '%s' is already a member of '%s'." %
                       (user, CHROME_REMOTING_GROUP_NAME))
          return 0
      except KeyError:
        logging.info("Group '%s' not found." % CHROME_REMOTING_GROUP_NAME)

      if run_command_as_root([SCRIPT_PATH, '--add-user-as-root', user]) != 0:
        logging.error("Failed to add user to group")
        return 1

      # Replace --enable-and-start with --start in the command-line arguments,
      # which are used later to reinvoke the script as a child of user-session.
      sys.argv = [arg if arg != "--enable-and-start" else "--start"
                  for arg in sys.argv]
      options.start = True

  if options.add_user_as_root is not None:
    if os.getuid() != 0:
      logging.error("--add-user-as-root can only be specified as root.")
      return 1;

    user = options.add_user_as_root
    try:
      pwd.getpwnam(user)
    except KeyError:
      logging.error("user '%s' does not exist." % user)
      return 1

    try:
      subprocess.check_call(["/usr/sbin/groupadd", "-f",
                             CHROME_REMOTING_GROUP_NAME])
      subprocess.check_call(["/usr/bin/gpasswd", "--add", user,
                             CHROME_REMOTING_GROUP_NAME])
    except (ValueError, OSError, subprocess.CalledProcessError) as e:
      logging.error("Command failed: " + str(e))
      return 1

    return 0

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
    if options.child_process:
      # If the script is running under user-session, try to relay the message.
      ParentProcessLogger.try_start_logging(USER_SESSION_MESSAGE_FD)
    logging.info("Service already running.")
    ParentProcessLogger.release_parent_if_connected(True)
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
    if os.path.isdir("/run/systemd/system"):
      return run_command_as_root(["systemctl", "start",
                                  "chrome-remote-desktop@" + getpass.getuser()])
    else:
      return start_via_user_session(options.foreground)

  # Start logging to user-session messaging pipe if it exists.
  ParentProcessLogger.try_start_logging(USER_SESSION_MESSAGE_FD)

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

  default_sizes = DEFAULT_SIZES

  # Collate the list of sizes that XRANDR should support.
  if not options.size:
    if DEFAULT_SIZES_ENV_VAR in os.environ:
      default_sizes = os.environ[DEFAULT_SIZES_ENV_VAR]
    options.size = default_sizes.split(",")

  sizes = []
  for size in options.size:
    size_components = size.split("x")
    if len(size_components) != 2:
      parser.error("Incorrect size format '%s', should be WIDTHxHEIGHT" % size)

    try:
      width = int(size_components[0])
      height = int(size_components[1])

      # Enforce minimum desktop size, as a sanity-check.  The limit of 100 will
      # detect typos of 2 instead of 3 digits.
      if width < 100 or height < 100:
        raise ValueError
    except ValueError:
      parser.error("Width and height should be 100 pixels or greater")

    sizes.append((width, height))

  # Register an exit handler to clean up session process and the PID file.
  atexit.register(cleanup)

  # Load the initial host configuration.
  host_config = Config(config_file)
  try:
    host_config.load()
  except (IOError, ValueError) as e:
    print("Failed to load config: " + str(e), file=sys.stderr)
    return 1

  # Register handler to re-load the configuration in response to signals.
  for s in [signal.SIGHUP, signal.SIGINT, signal.SIGTERM]:
    signal.signal(s, SignalHandler(host_config))

  # Verify that the initial host configuration has the necessary fields.
  auth = Authentication()
  auth_config_valid = auth.copy_from(host_config)
  host = Host()
  host_config_valid = host.copy_from(host_config)
  if not host_config_valid or not auth_config_valid:
    logging.error("Failed to load host configuration.")
    return 1

  if host.host_id:
    logging.info("Using host_id: " + host.host_id)

  extra_start_host_args = []
  if HOST_EXTRA_PARAMS_ENV_VAR in os.environ:
      extra_start_host_args = \
          re.split(r"\s+", os.environ[HOST_EXTRA_PARAMS_ENV_VAR].strip())
  is_wayland = any([opt == '--enable-wayland' for opt in extra_start_host_args])
  if is_wayland:
    desktop = WaylandDesktop(sizes, host_config)
  else:
    desktop = XDesktop(sizes, host_config)

  if is_crash_reporting_enabled(host_config):
    desktop.enable_crash_reporting()

  # Whether we are tearing down because the display server and/or session
  # exited. This keeps us from counting processes exiting because we've
  # terminated them as errors.
  tear_down = False

  while True:
    # If the session process or display server stops running (e.g. because the
    # user logged out), terminate all processes. The session will be restarted
    # once everything has exited.
    if tear_down:
      desktop.cleanup()

      failure_count = desktop.aggregate_failure_count()
      tear_down = False

      if (failure_count == 0):
        # Since the user's desktop is already gone at this point, there's no
        # state to lose and now is a good time to pick up any updates to this
        # script that might have been installed.
        logging.info("Relaunching self")
        relaunch_self()
      else:
        # If there is a non-zero |failures| count, restarting the whole script
        # would lose this information, so just launch the session as normal,
        # below.
        pass

    relaunch_times = []

    # Set the backoff interval and exit if a process failed too many times.
    backoff_time = SHORT_BACKOFF_TIME
    for inhibitor, offline_reason in desktop.inhibitors.items():
      if inhibitor.disabled:
        continue
      if inhibitor.failures >= MAX_LAUNCH_FAILURES:
        if offline_reason is None:
          logging.error("Too many launch failures of '%s', not retrying."
                        % inhibitor.label)
        else:
          logging.error("Too many launch failures of '%s', exiting."
                        % inhibitor.label)
          desktop.report_offline_reason(offline_reason)
          sys.exit(1)
      elif inhibitor.failures >= SHORT_BACKOFF_THRESHOLD:
        backoff_time = LONG_BACKOFF_TIME

      if inhibitor.is_inhibited():
        relaunch_times.append(inhibitor.earliest_relaunch_time)

    if relaunch_times:
      # We want to wait until everything is ready to start so we don't end up
      # launching things in the wrong order due to differing relaunch times.
      logging.info("Waiting before relaunching")
    else:
      if (desktop.pipewire_proc is None and desktop.pipewire_pulse_proc is None
          and desktop.pipewire_session_manager_proc is None
          and not desktop.pipewire_inhibitor.disabled
          and desktop.pipewire_inhibitor.failures < MAX_LAUNCH_FAILURES):
        desktop.setup_audio(host.host_id, backoff_time)
      if (desktop.server_proc is None and desktop.pre_session_proc is None and
          desktop.session_proc is None):
        desktop.launch_session(options.args, backoff_time)
      if desktop.host_proc is None:
        desktop.launch_host(extra_start_host_args, backoff_time)
      if desktop.crash_uploader_proc is None:
        desktop.launch_crash_uploader(backoff_time)

    deadline = max(relaunch_times) if relaunch_times else 0
    pid, status = waitpid_handle_exceptions(-1, deadline)
    if pid == 0:
      continue

    logging.info("wait() returned (%s,%s)" % (pid, status))

    # When a process has terminated, and we've reaped its exit-code, any Popen
    # instance for that process is no longer valid. Reset any affected instance
    # to None.
    tear_down = desktop.on_process_exit(pid, status)

if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG,
                      format="%(asctime)s:%(levelname)s:%(message)s")
  sys.exit(main())
