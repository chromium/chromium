#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys
import time

def main():
  description = 'Invokes build-webkit with the given options.'
  parser = argparse.ArgumentParser(description=description)
  parser.add_argument('--output_dir',
                    help='Output directory for build products.')
  parser.add_argument('--ios_simulator', action='store_true', default=False,
                      help='Use "iphoneos" SDK instead of "macos".')
  parser.add_argument('--asan', action='store_true', default=False,
                      help='Make Asan build.')
  parser.add_argument('--clean', action='store_true', default=False,
                      help='Clean output directory before building.')
  parser.add_argument('--debug', action='store_true', default=False,
                      help='Make debug build.')
  parser.add_argument('-j',
                    help='Number of parallel jobs to run.')
  (opts, extra_args) = parser.parse_known_args()

  output_dir = opts.output_dir
  if not output_dir:
    # Use a default that matches what ninja uses.
    platform_dir = 'iOS' if opts.ios_simulator else 'macOS'
    output_dir = os.path.realpath(os.path.join(
      os.path.dirname(__file__),
      '../../..',
      'out', 'Debug-iphonesimulator', 'obj',
      'ios', 'third_party', 'webkit', platform_dir));

  command = ['src/Tools/Scripts/build-webkit']

  if opts.ios_simulator:
    command.append('--ios-simulator')

  if opts.debug:
    command.append('--debug')

  # ClusterFuzz macOS bots are Intel machines. Remove this once they have
  # moved to ARM.
  if opts.asan:
    command.append('--architecture=x86_64 arm64')

  if opts.j:
    command.extend(['-jobs', opts.j])
  command.extend(extra_args)

  env = {
    'WEBKIT_OUTPUTDIR': output_dir,
     # Needed for /bin/mkdir, /usr/bin/copypng, and /usr/sbin/sysctl.
    'PATH': '/bin:/usr/bin:/usr/sbin',
  }
  cwd = os.path.dirname(os.path.realpath(__file__))

  if opts.clean:
     clean_command = ['src/Tools/Scripts/clean-webkit']
     proc = subprocess.Popen(clean_command, cwd=cwd, env=env)
     proc.communicate()

  if opts.asan:
     config_command = ['src/Tools/Scripts/set-webkit-configuration', '--asan']
     if opts.debug:
       config_command.append('--debug')
     proc = subprocess.Popen(config_command, cwd=cwd, env=env)
     proc.communicate()
     if proc.returncode:
       return proc.returncode

  # Enable rewriting WK_API_AVAILABLE() -> API_AVAILABLE().
  if opts.ios_simulator:
    command.append('WK_FRAMEWORK_HEADER_POSTPROCESSING_DISABLED=NO')

  proc = subprocess.Popen(command, cwd=cwd, env=env)

  # Building WebKit can take multiple hours, so produce output periodically to
  # to avoid appearing to be hung.
  build_finished = False
  start_time = time.time()
  while not build_finished:
    build_finished = True
    try:
      proc.communicate(timeout=600)
    except subprocess.TimeoutExpired:
      elapsed = time.time() - start_time
      print(f'WebKit is still building, {elapsed:.0f} seconds elapsed')
      build_finished = False

  return proc.returncode

if __name__ == '__main__':
  sys.exit(main())
