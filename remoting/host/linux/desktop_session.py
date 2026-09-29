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

  # Tears down the session processes and unsets CRD systemd env vars.
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
