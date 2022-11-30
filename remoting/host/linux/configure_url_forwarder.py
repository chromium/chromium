#!/usr/bin/python3
# Copyright 2021 The Chromium Authors
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

# XFCE4's default settings app changes default browser setting on Cinnamon/GNOME
# to xfce4-web-browser, which redirects GNOME apps to use the XFCE default
# browser setting.
XFCE4_WEB_BROWSER_DESKTOP_ENTRY = 'xfce4-web-browser.desktop'

HOST_SETTINGS_PATH = os.path.join(
    os.environ['HOME'],
    '.config/chrome-remote-desktop/host#{}.settings.json'.format(HOST_HASH))

X_SESSIONS_DIR = '/usr/share/xsessions/'

XDG_SETTING_DEFAULT_WEB_BROWSER = 'default-web-browser'


def env_with_current_desktop(desktop_env: str) -> dict[str, str]:
  """Returns an environment variable dictionary with XDG_CURRENT_DESKTOP set to
  |desktop_env|."""

  env = os.environ.copy()
  env['XDG_CURRENT_DESKTOP'] = desktop_env
  return env


def get_default_browser(desktop_env: str) -> str:
  """Returns the XDG default-web-browser setting for the given desktop
  environment."""

  args = ['xdg-settings', 'get', XDG_SETTING_DEFAULT_WEB_BROWSER]
  env = env_with_current_desktop(desktop_env)

  try:
    return subprocess.check_output(args, env=env).decode('utf-8').strip()
  except CalledProcessError as e:
    print('Failed to execute', args, ':', e, file=sys.stderr)
    return ''


def set_default_browser(desktop_env: str, desktop_entry: str) -> None:
  """Sets the XDG default-web-browser setting for the given desktop
  environment."""

  args = [
      'xdg-settings', 'set', XDG_SETTING_DEFAULT_WEB_BROWSER, desktop_entry]
  env = env_with_current_desktop(desktop_env)

  subprocess.run(args, env=env)


def set_default_browser_to_url_forwarder(
    desktop_env: str,
    backup_dict: dict[str, str],
    backup_key: str) -> None:
  """Sets default browser on the given desktop environment to the remote URL
  forwarder and backs up the previous default browser.

  Args:
    desktop_env: The desktop environment to be configured.
    backup_dict: The dictionary to backup the previous default browser.
    backup_key: The key that the previous default browser will be stored in
      backup_dict with.
  """

  current_entry = get_default_browser(desktop_env)
  if not current_entry:
    print('Cannot get default browser for', desktop_env, file=sys.stderr)
    return
  if current_entry == URL_FORWARDER_DESKTOP_ENTRY:
    print('Default browser for', desktop_env, 'is already',
          URL_FORWARDER_DESKTOP_ENTRY, file=sys.stderr)
    return
  backup_dict[backup_key] = current_entry
  if current_entry == XFCE4_WEB_BROWSER_DESKTOP_ENTRY:
    print('Default browser for', desktop_env, 'has been set to',
          XFCE4_WEB_BROWSER_DESKTOP_ENTRY, ', which effectively forces',
          desktop_env, 'to use XFCE\'s default browser setting.')
    # We can't back up XFCE's default browser here for local fallback, since it
    # might have been changed to the URL forwarder already.
    return
  set_default_browser(desktop_env, URL_FORWARDER_DESKTOP_ENTRY)
  print('Default browser for', desktop_env, 'has been successfully set to',
        URL_FORWARDER_DESKTOP_ENTRY)


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


def get_supported_desktop_envs_and_settings_key() -> dict[str, str]:
  desktop_envs_and_settings_keys = dict()

  for x_session_desktop_path in Path(X_SESSIONS_DIR).glob('*.desktop'):
    desktop_name = os.path.basename(x_session_desktop_path)
    if desktop_name.startswith('xfce'):
      desktop_envs_and_settings_keys['XFCE'] = 'previous_default_browser_xfce'
    elif desktop_name.startswith('cinnamon'):
      desktop_envs_and_settings_keys['X-Cinnamon'] = \
          'previous_default_browser_cinnamon'
    elif desktop_name.startswith('gnome'):
      desktop_envs_and_settings_keys['GNOME'] = 'previous_default_browser_gnome'

  if not desktop_envs_and_settings_keys:
    # Add X-Generic for generic fallback.
    desktop_envs_and_settings_keys['X-Generic'] = \
        'previous_default_browser_generic'

  return desktop_envs_and_settings_keys

def setup_url_forwarder() -> None:
  settings = load_host_settings_file()

  desktop_envs_and_setting_keys = get_supported_desktop_envs_and_settings_key()

  for desktop_env, setting_key in desktop_envs_and_setting_keys.items():
    set_default_browser_to_url_forwarder(desktop_env, settings, setting_key)

  # For previous default browsers that have been set to the XFCE4 web browser,
  # replace them with the actual previous default browser configured for XFCE4
  # so that the URL forwarder doesn't launch itself recursively in case of local
  # fallback.
  for desktop_env, setting_key in desktop_envs_and_setting_keys.items():
    if (setting_key in settings and
        settings[setting_key] == XFCE4_WEB_BROWSER_DESKTOP_ENTRY):
      if 'XFCE' not in desktop_envs_and_setting_keys:
        print('Default browser for', desktop_env, 'is set to',
              XFCE4_WEB_BROWSER_DESKTOP_ENTRY, 'but XFCE is not found',
              file=sys.stderr)
        break
      xfce_setting_key = desktop_envs_and_setting_keys['XFCE']
      if xfce_setting_key not in settings:
        print('Cannot find', xfce_setting_key, 'in host settings.')
        break
      settings[setting_key] = settings[xfce_setting_key]

  save_host_settings_file(settings)

  # There are also x-www-browser and gnome-www-browser in the Debian Alternative
  # system. Most apps don't use them directly. xdg-open uses them if the session
  # does not have a display (i.e. interactive shell). Configuring them requires
  # sudo permission, and we always have a desktop environment, so we are not
  # changing them for now.
  # There is also a BROWSER environment variable. xdg-open may also use it when
  # the session does not have a display. We can't export a environment variable
  # back to the parent process anyway, so we don't change it here.


def restore_default_browser(
    desktop_env: str,
    backup_dict: dict[str, str],
    backup_key: str) -> None:
  """Restores XDG default-web-browser to backup_dict[backup_key] on the given
  desktop environment.

  Args:
    desktop_env: The desktop environment to be configured.
    backup_dict: The dictionary where the previous configuration can be found.
    backup_key: The dictionary key to find the previous configuration.
  """

  if (backup_key not in backup_dict) or not backup_dict[backup_key]:
    print("No setting to restore from", backup_key, file=sys.stderr)
    return
  previous_setting = backup_dict[backup_key]
  if (previous_setting == URL_FORWARDER_DESKTOP_ENTRY or
      previous_setting == XFCE4_WEB_BROWSER_DESKTOP_ENTRY):
    print('Setting to restore from', backup_key, 'is', previous_setting,
          '. Ignored.', file=sys.stderr)
    return
  current_entry = get_default_browser(desktop_env)
  if current_entry == XFCE4_WEB_BROWSER_DESKTOP_ENTRY:
    print('Default browser for', desktop_env, 'is',
          XFCE4_WEB_BROWSER_DESKTOP_ENTRY, '. Ignored.', file=sys.stderr)
    return
  if current_entry != URL_FORWARDER_DESKTOP_ENTRY:
    print('Default browser for', desktop_env, 'is no longer',
          URL_FORWARDER_DESKTOP_ENTRY,
          '. Previously stored setting will not be restored.', file=sys.stderr)
    return
  set_default_browser(desktop_env, previous_setting)
  print('Default browser for', desktop_env, 'has been successfully restored to',
        previous_setting)


def restore_previous_settings() -> None:
  settings = load_host_settings_file()

  for desktop_env, setting_key in (
      get_supported_desktop_envs_and_settings_key().items()):
    restore_default_browser(desktop_env, settings, setting_key)


def check_default_browser_is_url_forwarder(desktop_env: str) -> None:
  """Checks if the default browser is set to the remote URL forwarder on the
  given desktop environment. Exits with 1 if it's not the case."""

  if (desktop_env != 'XFCE' and
      get_default_browser(desktop_env) == XFCE4_WEB_BROWSER_DESKTOP_ENTRY):
    print('Default browser for', desktop_env, 'is',
          XFCE4_WEB_BROWSER_DESKTOP_ENTRY,
          '. Checking default browser for XFCE instead...')
    check_default_browser_is_url_forwarder('XFCE')
    return
  if get_default_browser(desktop_env) != URL_FORWARDER_DESKTOP_ENTRY:
    print('Default browser for', desktop_env, 'is not',
          URL_FORWARDER_DESKTOP_ENTRY)
    sys.exit(1)


def check_url_forwarder_setup() -> None:
  for desktop_env in get_supported_desktop_envs_and_settings_key().keys():
    check_default_browser_is_url_forwarder(desktop_env)

  print('Chrome Remote Desktop URL forwarder is properly set up.')
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
    setup_url_forwarder()
  elif options.restore:
    restore_previous_settings()
  elif options.check_setup:
    check_url_forwarder_setup()
  else:
    parser.print_usage()

if __name__ == '__main__':
  logging.basicConfig(level=logging.DEBUG,
                      format='%(asctime)s:%(levelname)s:%(message)s')
  sys.exit(main())
