#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Determine OS and various other system properties.

Determine the name of the platform used and other system properties such as
the location of Chrome.  This is used, for example, to determine the correct
Toolchain to invoke.
"""

from __future__ import print_function

import argparse
import os
import re
import subprocess
import sys

import oshelpers


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHROME_DEFAULT_PATH = {
  'win': r'c:\Program Files (x86)\Google\Chrome\Application\chrome.exe',
  'mac': '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  'linux': '/usr/bin/google-chrome',
}


if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)


class Error(Exception):
  pass


def GetSDKPath():
  return os.getenv('NACL_SDK_ROOT', os.path.dirname(SCRIPT_DIR))


def GetPlatform():
  if sys.platform.startswith('cygwin') or sys.platform.startswith('win'):
    return 'win'
  elif sys.platform.startswith('darwin'):
    return 'mac'
  elif sys.platform.startswith('linux'):
    return 'linux'
  else:
    raise Error("Unknown platform: %s" % sys.platform)


def UseWin64():
  arch32 = os.environ.get('PROCESSOR_ARCHITECTURE')
  arch64 = os.environ.get('PROCESSOR_ARCHITEW6432')

  if arch32 == 'AMD64' or arch64 == 'AMD64':
    return True
  return False


def GetSDKVersion():
  root = GetSDKPath()
  readme = os.path.join(root, "README")
  if not os.path.exists(readme):
    raise Error("README not found in SDK root: %s" % root)

  version = None
  revision = None
  commit_position = None
  for line in open(readme):
    if ':' in line:
      name, value = line.split(':', 1)
      if name == "Version":
        version = value.strip()
      if name == "Chrome Revision":
        revision = value.strip()
      if name == "Chrome Commit Position":
        commit_position = value.strip()

  if revision is None or version is None or commit_position is None:
    raise Error("error parsing SDK README: %s" % readme)

  try:
    version = int(version)
    revision = int(revision)
  except ValueError:
    raise Error("error parsing SDK README: %s" % readme)

  return (version, revision, commit_position)


def GetSystemArch(platform):
  if platform == 'win':
    if UseWin64():
      return 'x86_64'
    return 'x86_32'

  if platform in ['mac', 'linux']:
    try:
      pobj = subprocess.Popen(['uname', '-m'], stdout= subprocess.PIPE)
      arch = pobj.communicate()[0]
      arch = arch.split()[0]
      if arch.startswith('arm'):
        arch = 'arm'
    except Exception:
      arch = None
  return arch


def GetChromePath(platform):
  # If CHROME_PATH is defined and exists, use that.
  chrome_path = os.environ.get('CHROME_PATH')
  if chrome_path:
    if not os.path.exists(chrome_path):
      raise Error('Invalid CHROME_PATH: %s' % chrome_path)
    return os.path.realpath(chrome_path)

  # Otherwise look in the PATH environment variable.
  basename = os.path.basename(CHROME_DEFAULT_PATH[platform])
  chrome_path = oshelpers.FindExeInPath(basename)
  if chrome_path:
    return os.path.realpath(chrome_path)

  # Finally, try the default paths to Chrome.
  chrome_path = CHROME_DEFAULT_PATH[platform]
  if os.path.exists(chrome_path):
    return os.path.realpath(chrome_path)

  raise Error('CHROME_PATH is undefined, and %s not found in PATH, nor %s.' % (
              basename, chrome_path))


def GetNaClArch(platform):
  if platform == 'win':
    # On windows the nacl arch always matches to system arch
    return GetSystemArch(platform)
  elif platform == 'mac':
    # On Mac the nacl arch is currently always 32-bit.
    return 'x86_32'

  # On linux the nacl arch matches to chrome arch, so we inspect the chrome
  # binary using objdump
  chrome_path = GetChromePath(platform)

  # If CHROME_PATH is set to point to google-chrome or google-chrome
  # was found in the PATH and we are running on UNIX then google-chrome
  # is a bash script that points to 'chrome' in the same folder.
  #
  # When running beta or dev branch, the name is google-chrome-{beta,dev}.
  if os.path.basename(chrome_path).startswith('google-chrome'):
    chrome_path = os.path.join(os.path.dirname(chrome_path), 'chrome')

  if not os.path.exists(chrome_path):
    raise Error("File %s does not exist." % chrome_path)

  if not os.access(chrome_path, os.X_OK):
    raise Error("File %s is not executable" % chrome_path)

  try:
    pobj = subprocess.Popen(['objdump', '-f', chrome_path],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    output, stderr = pobj.communicate()
    # error out here if objdump failed
    if pobj.returncode:
      raise Error(output + stderr.strip())
  except OSError as e:
    # This will happen if objdump is not installed
    raise Error("Error running objdump: %s" % e)

  pattern = r'(file format) ([a-zA-Z0-9_\-]+)'
  match = re.search(pattern, output)
  if not match:
    raise Error("Error running objdump on: %s" % chrome_path)

  arch = match.group(2)
  if 'arm' in arch:
    return 'arm'
  if '64' in arch:
    return 'x86_64'
  return 'x86_32'


def ParseVersion(version):
  """Parses a version number of the form '<major>.<position>'.

  <position> is the Cr-Commit-Position number.
  """
  if '.' in version:
    version = version.split('.')
  else:
    version = (version, '0')

  try:
    return tuple(int(x) for x in version)
  except ValueError:
    raise Error('error parsing SDK version: %s' % version)


def CheckVersion(required_version):
  """Determines whether the current SDK version meets the required version.

  Args:
    required_version: (major, position) pair, where position is the
    Cr-Commit-Position number.

  Raises:
    Error: The SDK version is older than required_version.
  """
  version = GetSDKVersion()[:2]
  if version < required_version:
    raise Error("SDK version too old (current: %d.%d, required: %d.%d)"
           % (version[0], version[1], required_version[0], required_version[1]))


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--arch', action='store_true',
      help='Print architecture of current machine (x86_32, x86_64 or arm).')
  parser.add_argument('--chrome', action='store_true',
      help='Print the path chrome (by first looking in $CHROME_PATH and '
           'then $PATH).')
  parser.add_argument('--nacl-arch', action='store_true',
      help='Print architecture used by NaCl on the current machine.')
  parser.add_argument('--sdk-version', action='store_true',
      help='Print major version of the NaCl SDK.')
  parser.add_argument('--sdk-revision', action='store_true',
      help='Print revision number of the NaCl SDK.')
  parser.add_argument('--sdk-commit-position', action='store_true',
      help='Print commit position of the NaCl SDK.')
  parser.add_argument('--check-version',
      metavar='MAJOR.POSITION',
      help='Check that the SDK version is at least as great as the '
           'version passed in. MAJOR is the major version number and POSITION '
           'is the Cr-Commit-Position number.')

  if len(args) > 1:
    parser.error('Only one option can be specified at a time.')

  options = parser.parse_args(args)

  platform = GetPlatform()

  if options.arch:
    out = GetSystemArch(platform)
  elif options.nacl_arch:
    out = GetNaClArch(platform)
  elif options.chrome:
    out = GetChromePath(platform)
  elif options.sdk_version:
    out = GetSDKVersion()[0]
  elif options.sdk_revision:
    out = GetSDKVersion()[1]
  elif options.sdk_commit_position:
    out = GetSDKVersion()[2]
  elif options.check_version:
    required_version = ParseVersion(options.check_version)
    CheckVersion(required_version)
    out = None
  else:
    out = platform

  if out:
    print(out)
  return 0


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Error as e:
    sys.stderr.write(str(e) + '\n')
    sys.exit(1)
