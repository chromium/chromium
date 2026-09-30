#!/usr/bin/python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Launches and supervises a standalone Chrome Remote Desktop X11 session for
# the multi-process host. Unlike linux_me2me_host.py, this script does not
# launch or manage any host process.

import argparse
import atexit
import datetime
import dbus
import logging
import os
import pwd
import signal
import socket
import sys
import syslog
import time

from shared_lib import (
    XDesktop,
    cleanup,
    display_manager_is_gdm,
    exec_self_via_login_shell,
    watch_for_resolution_changes,
)

# PAM service name used by chrome-remote-desktop-x11-session@.service.
CRD_PAM_SERVICE = "chrome-remote-desktop"

# Timeout for each D-Bus call to logind, so that a hung logind can't delay
# the exit past the unit's TimeoutStopSec, which would lose the exit code.
LOGIND_DBUS_TIMEOUT_SECONDS = 5


def terminate_logind_session():
  """Terminates the logind session that this script runs in.

  Processes that daemonize (e.g. XDG autostart apps) are not killed by
  cleanup(). With the default `KillUserProcesses=no`, logind would leave them
  running and keep the session in the "closing" state indefinitely after
  pam_close_session(). Terminating the session makes logind stop its scope
  unit, which kills all remaining processes. This must happen while the
  session is still active, i.e. before this process exits.
  """
  try:
    bus = dbus.SystemBus()
    manager = dbus.Interface(
        bus.get_object("org.freedesktop.login1", "/org/freedesktop/login1",
                       introspect=False),
        "org.freedesktop.login1.Manager")
    session_path = manager.GetSessionByPID(
        dbus.UInt32(os.getpid()), timeout=LOGIND_DBUS_TIMEOUT_SECONDS)
    session_object = bus.get_object("org.freedesktop.login1", session_path,
                                    introspect=False)
    properties = session_object.GetAll(
        "org.freedesktop.login1.Session",
        dbus_interface=dbus.PROPERTIES_IFACE,
        timeout=LOGIND_DBUS_TIMEOUT_SECONDS)
    service = properties.get("Service")
    leader = properties.get("Leader")
    # Only terminate the session that the systemd unit created for this
    # process. Otherwise, e.g. if the script was run manually from an SSH or
    # desktop session, leave that session alone.
    if service != CRD_PAM_SERVICE or leader != os.getpid():
      logging.info(
          "Not terminating logind session %s with PAM service %s and leader "
          "%s", session_path, service, leader)
      return

    # Terminating the session also sends SIGTERM and SIGHUP to this process.
    # Ignore them so that they don't interrupt the atexit handlers or change
    # the exit code (e.g. RELAUNCH_EXIT_CODE).
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    logging.info("Terminating logind session %s", session_path)
    dbus.Interface(session_object, "org.freedesktop.login1.Session").Terminate(
        timeout=LOGIND_DBUS_TIMEOUT_SECONDS)
  except dbus.exceptions.DBusException as e:
    logging.warning("Failed to terminate logind session: %s", e)


class SignalHandler:
  """Exits on SIGINT/SIGTERM so that the atexit cleanup() handler runs.

  SIGHUP is only logged, so that it doesn't terminate the session without
  running cleanup().
  """

  def __call__(self, signum, _stackframe):
    logging.info("Caught signal: %s", signum)
    if signum in (signal.SIGINT, signal.SIGTERM):
      raise SystemExit


def setup_argument_parser():
  parser = argparse.ArgumentParser(
      usage="Usage: %(prog)s --type=x11 [ -- [ X server options ] ]")
  # Not `required=True`, since XDesktop re-invokes this script with only
  # --watch-resolution.
  parser.add_argument("--type", dest="session_type", choices=["x11"],
                      help="Type of desktop session to launch.")
  # Appended by exec_self_via_login_shell() when re-executing this script.
  parser.add_argument("--child-process", dest="child_process", default=False,
                      action="store_true", help=argparse.SUPPRESS)
  # The script is being run in a new PAM session. Attempt to exec a login shell
  # to allow the user's ~/.profile or similar to run.
  parser.add_argument("--new-session", dest="new_session", default=False,
                      action="store_true", help=argparse.SUPPRESS)
  parser.add_argument("--watch-resolution", dest="watch_resolution",
                      type=int, nargs=2, default=False, action="store",
                      help=argparse.SUPPRESS)
  parser.add_argument(dest="args", nargs="*", help=argparse.SUPPRESS)
  return parser


def main():
  parser = setup_argument_parser()
  options = parser.parse_args()

  if options.watch_resolution:
    watch_for_resolution_changes(tuple(options.watch_resolution))
    return 0

  # TODO: crbug.com/493980875 - Add support for kde-wayland.
  if options.session_type != "x11":
    parser.error("--type=x11 is required.")

  if options.new_session:
    exec_self_via_login_shell()

  logging.info("CRD desktop session (%s) is starting", options.session_type)
  logging.info("Machine hostname: %s", socket.getfqdn())
  uptime = datetime.timedelta(
      seconds=int(time.clock_gettime(time.CLOCK_BOOTTIME)))
  logging.info("Machine uptime: %s", uptime)

  if display_manager_is_gdm():
    # See https://gitlab.gnome.org/GNOME/gdm/-/issues/580 for details.
    gdm_message = (
        "WARNING: This system uses GDM. Some GDM versions have a bug that "
        "prevents local login while a Chrome Remote Desktop session is "
        "running. If you run into this issue, you can stop the Chrome Remote "
        "Desktop session from an SSH session or a text console (e.g. "
        "Ctrl+Alt+F3) by running `sudo systemctl stop "
        "chrome-remote-desktop-x11-session@%s`." %
        pwd.getpwuid(os.getuid()).pw_name)
    logging.warning(gdm_message)
    syslog.syslog(syslog.LOG_WARNING | syslog.LOG_DAEMON, gdm_message)

  # atexit handlers run in reverse order, so the logind session is terminated
  # after cleanup() has torn down the session processes and unset CRD systemd
  # env vars.
  atexit.register(terminate_logind_session)
  atexit.register(cleanup)
  signal_handler = SignalHandler()
  for s in [signal.SIGHUP, signal.SIGINT, signal.SIGTERM]:
    signal.signal(s, signal_handler)

  XDesktop().run(options.args)
  return 0


if __name__ == "__main__":
  logging.basicConfig(level=logging.DEBUG,
                      format="%(asctime)s:%(levelname)s:%(message)s")
  sys.exit(main())
