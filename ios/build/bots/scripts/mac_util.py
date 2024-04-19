# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import distutils.version
import json
import logging
import subprocess
from typing import Union, Tuple

LOGGER = logging.getLogger(__name__)


def version():
  """Invokes sw_vers -productVersion

  Raises:
    subprocess.CalledProcessError on exit codes non zero

  Returns:
    e.g. 13.2.1
  """
  cmd = [
      'sw_vers',
      '-productVersion',
  ]

  # output sample:
  # 13.2.1
  output = subprocess.check_output(
      cmd, stderr=subprocess.STDOUT).decode('utf-8')
  return output


def is_macos_13_or_higher():
  """Returns true if the current MacOS version is 13 or higher"""
  return distutils.version.LooseVersion(
      '13.0') <= distutils.version.LooseVersion(version())


def kill_usbmuxd():
  """kills the current usbmuxd process"""
  cmd = [
      'sudo',
      '/usr/bin/killall',
      '-v',
      'usbmuxd',
  ]
  subprocess.check_call(cmd)


def stop_usbmuxd():
  """stops the current usbmuxd process"""
  cmd = [
      'sudo',
      '/bin/launchctl',
      'stop',
      'com.apple.usbmuxd',
  ]
  subprocess.check_call(cmd)


def run_codesign_check(dir_path):
  """Runs codesign check on a directory

    Returns:
        success (boolean), error (subprocess.CalledProcessError)
    """
  try:
    cmd = [
        'codesign',
        '--verify',
        '--verbose=9',
        '--deep',
        '--strict=all',
        dir_path,
    ]
    subprocess.check_call(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
  except subprocess.CalledProcessError as e:
    return False, e

  return True, None


error_type = Union[subprocess.CalledProcessError, json.JSONDecodeError]
plist_as_dict_return_type = Union[Tuple[dict, None], Tuple[None, error_type]]


def plist_as_dict(abs_path: str) -> plist_as_dict_return_type:
  """Converts plist to python dictionary.

  Args:
      abs_path (str) absolute path of the string to convert.

  Returns:
      Plist (dictionary),
      error (subprocess.CalledProcessError, json.JSONDecodeError)
  """
  try:
    plist = json.loads(
        subprocess.check_output(
            ['plutil', '-convert', 'json', '-o', '-', abs_path]))
    return plist, None
  except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
    return None, e
