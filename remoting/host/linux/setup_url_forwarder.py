#!/usr/bin/python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import hashlib
import json
import logging
import os
import shutil
import socket
import subprocess
import sys

from pathlib import Path
from typing import Callable

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

HOST_HASH = hashlib.md5(socket.gethostname().encode()).hexdigest()

URL_FORWARDER_DESKTOP_ENTRY = 'crd-url-forwarder.desktop'

HOST_SETTINGS_PATH = os.path.join(
    os.environ['HOME'],
    '.config/chrome-remote-desktop/host#{}.settings.json'.format(HOST_HASH))

XDG_DATA_HOME = (os.environ['XDG_DATA_HOME']
    if 'XDG_DATA_HOME' in os.environ
    else os.path.join(os.environ['HOME'], '.local/share'))

XDG_USER_APP_DIR = os.path.join(XDG_DATA_HOME, 'applications')

SRC_URL_FORWARDER_DESKTOP_ENTRY_PATH = os.path.join(
    SCRIPT_DIR, URL_FORWARDER_DESKTOP_ENTRY)

DEST_URL_FORWARDER_DESKTOP_ENTRY_PATH = os.path.join(
    XDG_USER_APP_DIR, URL_FORWARDER_DESKTOP_ENTRY)

X_SCHEME_HANDLER_HTTP = 'x-scheme-handler/http'
X_SCHEME_HANDLER_HTTPS = 'x-scheme-handler/https'
X_SCHEME_HANDLER_MAILTO = 'x-scheme-handler/mailto'
XDG_SETTING_DEFAULT_WEB_BROWSER = 'default-web-browser'

HOST_SETTING_KEY_PREVIOUS_DEFAULT_WEB_BROWSER = 'previous_default_web_browser'
HOST_SETTING_KEY_PREVIOUS_HTTP_HANDLER = 'previous_http_handler'
HOST_SETTING_KEY_PREVIOUS_HTTPS_HANDLER = 'previous_https_handler'
HOST_SETTING_KEY_PREVIOUS_MAILTO_HANDLER = 'previous_mailto_handler'


def install_url_forwarder_desktop_entry() -> None:
  """Installs the URL forwarder desktop entry into the user's XDG app
  directory."""

  Path(XDG_USER_APP_DIR).mkdir(mode=0o755, parents=True, exist_ok=True)
  shutil.copyfile(SRC_URL_FORWARDER_DESKTOP_ENTRY_PATH,
                  DEST_URL_FORWARDER_DESKTOP_ENTRY_PATH)


def remove_url_forwarder_desktop_entry() -> None:
  """Removes the URL forwarder desktop entry from the user's XDG app
  directory."""

  if not os.path.isfile(DEST_URL_FORWARDER_DESKTOP_ENTRY_PATH):
    print('File', DEST_URL_FORWARDER_DESKTOP_ENTRY_PATH, 'doesn\'t exist.',
          file=sys.stderr)
    return
  os.remove(DEST_URL_FORWARDER_DESKTOP_ENTRY_PATH)


def get_output_or_empty_string(args: list[str]) -> str:
  """Executes |args| and returns the output. Returns an empty string if the
  command fails to execute."""

  try:
    return subprocess.check_output(args).decode('utf-8').strip()
  except CalledProcessError as e:
    print('Failed to execute', args, ':', e, file=sys.stderr)
    return ''


def get_default_browser(unused_entry_name: str) -> str:
  """Returns the XDG default-web-browser setting."""

  return get_output_or_empty_string(
      ['xdg-settings', 'get', XDG_SETTING_DEFAULT_WEB_BROWSER])


def set_default_browser(unused_entry_name: str, desktop_entry: str) -> None:
  """Sets the XDG default-web-browser setting."""

  subprocess.run([
      'xdg-settings', 'set', XDG_SETTING_DEFAULT_WEB_BROWSER, desktop_entry])


def get_mime_default(mime: str) -> str:
  """Gets the default desktop entry for |mime|."""

  return get_output_or_empty_string(['xdg-mime', 'query', 'default', mime])


def set_mime_default(mime: str, desktop_entry: str) -> None:
  """Sets the default desktop entry for |mime|."""

  subprocess.run(['xdg-mime', 'default', desktop_entry, mime])


def set_entry_to_url_forwarder(
    entry_name: str,
    entry_getter: Callable[[str], str],
    entry_setter: Callable[[str, str], None],
    backup_dict: dict[str, str],
    backup_key: str) -> None:
  """Sets an XDG entry to the remote URL forwarder.

  Args:
    entry_name: The name of the XDG entry.
    entry_getter: Called with |entry_name| to get back the current configured
      XDG entry.
    entry_setter: Called with |entry_name| and the new desktop entry value to
      set an XDG entry.
    backup_dict: The dictionary to backup the previous desktop entry.
    backup_key: The key that the previous desktop entry will be stored in
      backup_dict with.
  """

  current_entry = entry_getter(entry_name)
  if not current_entry:
    print('No value is associated with', entry_name, file=sys.stderr)
    return
  if current_entry == URL_FORWARDER_DESKTOP_ENTRY:
    print(entry_name, 'is already', URL_FORWARDER_DESKTOP_ENTRY,
          file=sys.stderr)
    return
  backup_dict[backup_key] = current_entry
  entry_setter(entry_name, URL_FORWARDER_DESKTOP_ENTRY)


def set_default_browser_to_url_forwarder(backup_dict: dict[str, str],
                                         backup_key: str) -> None:
  """Sets the XDG default browser to the remote URL forwarder and back up the
  previous default browser."""

  set_entry_to_url_forwarder(XDG_SETTING_DEFAULT_WEB_BROWSER,
                             get_default_browser, set_default_browser,
                             backup_dict, backup_key)


def set_mime_default_to_url_forwarder(
    mime: str, backup_dict: dict[str, str], backup_key: str) -> None:
  """Sets the XDG MIME default to the remote URL forwarder and back up the
  previous MIME default."""

  set_entry_to_url_forwarder(mime, get_mime_default, set_mime_default,
                             backup_dict, backup_key)


def load_host_settings_file() -> dict[str, str]:
  """Loads and returns the host settings JSON."""

  if not os.path.isfile(HOST_SETTINGS_PATH):
    return {}
  with open(HOST_SETTINGS_PATH, 'r') as settings_file:
    try:
      return json.load(settings_file)
    except JSONDecodeError as e:
      print('Failed to load JSON file:', e, file=sys.stderr)
      return {}


def save_host_settings_file(settings: dict[str, str]) -> None:
  """Saves the host settings JSON to the file."""

  with open(HOST_SETTINGS_PATH, 'w') as settings_file:
    json.dump(settings, settings_file)


def setup() -> None:
  install_url_forwarder_desktop_entry()
  settings = load_host_settings_file()

  # Apps that use xdg-open to open the URL obey the default browser settings.
  # Example: IntelliJ
  set_default_browser_to_url_forwarder(
      settings, HOST_SETTING_KEY_PREVIOUS_DEFAULT_WEB_BROWSER)

  # Apps that use gvfs-open will always look at the URL scheme handler settings.
  # Example: GNOME Terminal
  # Note: In most desktop environments (an exception would be XFCE), setting
  # default-web-browser is equivalent to setting the HTTP and HTTPS scheme
  # handlers. This is still fine as the calls below will be no-op if the desktop
  # entry is already the URL forwarder.
  set_mime_default_to_url_forwarder(
      X_SCHEME_HANDLER_HTTP, settings, HOST_SETTING_KEY_PREVIOUS_HTTP_HANDLER)
  set_mime_default_to_url_forwarder(
      X_SCHEME_HANDLER_HTTPS, settings, HOST_SETTING_KEY_PREVIOUS_HTTPS_HANDLER)
  set_mime_default_to_url_forwarder(
      X_SCHEME_HANDLER_MAILTO, settings,
      HOST_SETTING_KEY_PREVIOUS_MAILTO_HANDLER)
  save_host_settings_file(settings)

  # There are also x-www-browser and gnome-www-browser in the Debian Alternative
  # system. Most apps don't use them directly. xdg-open uses them if the session
  # does not have a display (i.e. interactive shell). Configuring them requires
  # sudo permission, and we always have a desktop environment, so we are not
  # changing them for now.
  # There is also a BROWSER environment variable. xdg-open may also use it when
  # the session does not have a display. We can't export a environment variable
  # back to the parent process anyway, so we don't change it here.


def restore_entry(
    entry_name: str,
    entry_getter: Callable[[str], str],
    entry_setter: Callable[[str, str], None],
    backup_dict: dict[str, str],
    backup_key: str) -> None:
  """Restores an XDG entry back to the previous configuration.

  Args:
    entry_name: The name of the XDG entry.
    entry_getter: Called with |entry_name| to get back the current configured
      XDG entry.
    entry_setter: Called with |entry_name| and the new desktop entry value to
      set an XDG entry.
    backup_dict: The dictionary where the previous configuration can be found.
    backup_key: The dictionary key to find the previous configuration.
  """

  if (backup_key not in backup_dict) or not backup_dict[backup_key]:
    print("No setting to restore from", backup_key, file=sys.stderr)
    return
  previous_setting = backup_dict[backup_key]
  if previous_setting == URL_FORWARDER_DESKTOP_ENTRY:
    print('Setting to restore from', backup_key, 'is',
          URL_FORWARDER_DESKTOP_ENTRY, '. Ignored.', file=sys.stderr)
    return
  current_entry = entry_getter(entry_name)
  if current_entry != URL_FORWARDER_DESKTOP_ENTRY:
    print(entry_name, 'is no longer', URL_FORWARDER_DESKTOP_ENTRY,
          '. Previously stored setting will not be restored.', file=sys.stderr)
    return
  entry_setter(entry_name, previous_setting)


def restore_default_browser(backup_dict: dict[str, str],
                            backup_key: str) -> None:
  """Restores XDG default-web-browser to backup_dict[backup_key]."""

  restore_entry(XDG_SETTING_DEFAULT_WEB_BROWSER, get_default_browser,
                set_default_browser, backup_dict, backup_key)


def restore_mime_default(
    mime: str, backup_dict: dict[str, str], backup_key: str) -> None:
  """Restores an XDG mime default to backup_dict[backup_key]."""

  restore_entry(mime, get_mime_default, set_mime_default, backup_dict,
                backup_key)


def restore() -> None:
  settings = load_host_settings_file()
  restore_default_browser(
      settings, HOST_SETTING_KEY_PREVIOUS_DEFAULT_WEB_BROWSER)
  restore_mime_default(
      X_SCHEME_HANDLER_HTTP, settings, HOST_SETTING_KEY_PREVIOUS_HTTP_HANDLER)
  restore_mime_default(
      X_SCHEME_HANDLER_HTTPS, settings, HOST_SETTING_KEY_PREVIOUS_HTTPS_HANDLER)
  restore_mime_default(
      X_SCHEME_HANDLER_MAILTO, settings,
      HOST_SETTING_KEY_PREVIOUS_MAILTO_HANDLER)
  remove_url_forwarder_desktop_entry()


def check_entry_is_url_forwarder(
    entry_name: str, entry_getter: Callable[[str], str]) -> None:
  """Checks if an XDG entry is set to the remote URL forwarder. Exists with 1 if
  it's not the case."""

  if entry_getter(entry_name) != URL_FORWARDER_DESKTOP_ENTRY:
    print(entry_name, 'is not', URL_FORWARDER_DESKTOP_ENTRY)
    sys.exit(1)


def check_setup() -> None:
  check_entry_is_url_forwarder(XDG_SETTING_DEFAULT_WEB_BROWSER,
                               get_default_browser)
  check_entry_is_url_forwarder(X_SCHEME_HANDLER_HTTP, get_mime_default)
  check_entry_is_url_forwarder(X_SCHEME_HANDLER_HTTPS, get_mime_default)
  check_entry_is_url_forwarder(X_SCHEME_HANDLER_MAILTO, get_mime_default)
  sys.exit(0)


def main() -> None:
  parser = argparse.ArgumentParser(
      usage='Usage: %(prog)s [options]',
      description='Set up a URL forwarder so that URLs will be opened on the '
      'Chrome Remote Desktop client. This script must be run within the remote '
      'desktop\'s session.')
  parser.add_argument('--setup', dest='setup', default=False,
                      action='store_true',
                      help='Set up the URL forwarder.')
  parser.add_argument('--restore', dest='restore', default=False,
                      action='store_true',
                      help='Remove the URL forwarder and restore default '
                      'browser settings.')
  parser.add_argument('--check-setup', dest='check_setup', default=False,
                      action='store_true',
                      help='Exit with 0 if the URL forwarder is properly set '
                      'up, or 1 otherwise.')
  options = parser.parse_args()
  if options.setup:
    setup()
  elif options.restore:
    restore()
  elif options.check_setup:
    check_setup()
  else:
    parser.print_usage()

if __name__ == '__main__':
  logging.basicConfig(level=logging.DEBUG,
                      format='%(asctime)s:%(levelname)s:%(message)s')
  sys.exit(main())
