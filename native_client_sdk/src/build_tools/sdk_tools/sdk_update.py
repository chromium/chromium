#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shim script for the SDK updater, to allow automatic updating.

The purpose of this script is to be a shim which automatically updates
sdk_tools (the bundle containing the updater scripts) whenever this script is
run.

When the sdk_tools bundle has been updated to the most recent version, this
script forwards its arguments to sdk_updater_main.py.
"""

import os
import subprocess
from sdk_update_common import RenameDir, RemoveDir, Error
import sys
import tempfile


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_UPDATE_MAIN = os.path.join(SCRIPT_DIR, 'sdk_update_main.py')
SDK_ROOT_DIR = os.path.dirname(SCRIPT_DIR)
NACLSDK_SHELL_SCRIPT = os.path.join(SDK_ROOT_DIR, 'naclsdk')
if sys.platform.startswith('win'):
  NACLSDK_SHELL_SCRIPT += '.bat'
SDK_TOOLS_DIR = os.path.join(SDK_ROOT_DIR, 'sdk_tools')
SDK_TOOLS_UPDATE_DIR = os.path.join(SDK_ROOT_DIR, 'sdk_tools_update')


def MakeSdkUpdateMainCmd(args):
  """Returns a list of command line arguments to run sdk_update_main.

  Args:
    args: A list of arguments to pass to sdk_update_main.py
  Returns:
    A new list that can be passed to subprocess.call, subprocess.Popen, etc.
  """
  return [sys.executable, SDK_UPDATE_MAIN] + args


def UpdateSDKTools(args):
  """Run sdk_update_main to update sdk_tools bundle. Return True if it is
  updated.

  Args:
    args: The arguments to pass to sdk_update_main.py. We need to keep this to
        ensure sdk_update_main is called correctly; some parameters specify
        URLS or directories to use.
  Returns:
    True if the sdk_tools bundle was updated.
  """
  cmd = MakeSdkUpdateMainCmd(args + ['--update-sdk-tools'])
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  stdout, _ = process.communicate()
  if process.returncode == 0:
    return stdout.find('Updating bundle sdk_tools to version') != -1
  else:
    # Updating sdk_tools could fail for any number of reasons. Regardless, it
    # should be safe to try to run the user's command.
    return False


def RenameSdkToolsDirectory():
  """Rename sdk_tools_update to sdk_tools."""
  # If there is no update directory, bail.
  if not os.path.isdir(SDK_TOOLS_UPDATE_DIR):
    return False

  try:
    tempdir = tempfile.mkdtemp()
    temp_sdktools = os.path.join(tempdir, 'sdk_tools')
    try:
      RenameDir(SDK_TOOLS_DIR, temp_sdktools)
    except Error:
      # The user is probably on Windows, and the directory is locked.
      sys.stderr.write('Cannot rename directory "%s". Make sure no programs are'
          ' viewing or accessing this directory and try again.\n' % (
          SDK_TOOLS_DIR,))
      sys.exit(1)

    try:
      RenameDir(SDK_TOOLS_UPDATE_DIR, SDK_TOOLS_DIR)
    except Error:
      # Failed for some reason, move the old dir back.
      try:
        RenameDir(temp_sdktools, SDK_TOOLS_DIR)
      except Error:
        # Not much to do here. sdk_tools won't exist, but sdk_tools_update
        # should. Hopefully running the batch script again will move
        # sdk_tools_update -> sdk_tools and it will work this time...
        sys.stderr.write('Unable to restore directory "%s" while auto-updating.'
            'Make sure no programs are viewing or accessing this directory and'
            'try again.\n' % (SDK_TOOLS_DIR,))
        sys.exit(1)
  finally:
    RemoveDir(tempdir)

  return True


def CheckRuntimeRequirements():
  if sys.platform.startswith('linux'):
    if not os.path.exists('/lib/ld-linux.so.2'):
      sys.stderr.write("""32-bit runtime environment was not found on this
system.  Specifically the 32-bit dynamic loader which is needed by the NaCl
compilers was not found ('/lib/ld-linux.so.2').  On modern debian/ubuntu
systems this is included in the 'libc6:i386' package.\n""")
      sys.exit(1)


def main():
  args = sys.argv[1:]
  CheckRuntimeRequirements()
  if UpdateSDKTools(args) and RenameSdkToolsDirectory():
    # Call the shell script, just in case this script was updated in the next
    # version of sdk_tools
    return subprocess.call([NACLSDK_SHELL_SCRIPT] + args)
  else:
    return subprocess.call(MakeSdkUpdateMainCmd(args))


if __name__ == '__main__':
  try:
    sys.exit(main())
  except KeyboardInterrupt:
    sys.exit(1)
