#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper for running gpu integration tests on Fuchsia devices."""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import tempfile

from gpu_tests import path_util

sys.path.insert(0,
                os.path.join(path_util.GetChromiumSrcDir(), 'build', 'fuchsia'))
from common_args import (AddCommonArgs, ConfigureLogging,
                         GetDeploymentTargetForArgs)
from symbolizer import RunSymbolizer


def main():
  parser = argparse.ArgumentParser()
  AddCommonArgs(parser)
  args, gpu_test_args = parser.parse_known_args()
  ConfigureLogging(args)

  additional_target_args = {}

  # If output_dir is not set, assume the script is being launched
  # from the output directory.
  if not args.out_dir:
    args.out_dir = os.getcwd()
    additional_target_args['out_dir'] = args.out_dir

  # Create a temporary log file that Telemetry will look to use to build
  # an artifact when tests fail.
  temp_log_file = False
  if not args.system_log_file:
    args.system_log_file = os.path.join(tempfile.mkdtemp(), 'system-log')
    temp_log_file = True
    additional_target_args['system_log_file'] = args.system_log_file

  package_names = ['web_engine', 'web_engine_shell']
  web_engine_dir = os.path.join(args.out_dir, 'gen', 'fuchsia', 'engine')
  gpu_script = [
      os.path.join(path_util.GetChromiumSrcDir(), 'content', 'test', 'gpu',
                   'run_gpu_integration_test.py')
  ]

  # Pass all other arguments to the gpu integration tests.
  gpu_script.extend(gpu_test_args)
  try:
    with GetDeploymentTargetForArgs(additional_target_args) as target:
      target.Start()
      _, fuchsia_ssh_port = target._GetEndpoint()
      gpu_script.extend(['--fuchsia-ssh-config-dir', args.out_dir])
      gpu_script.extend(['--fuchsia-ssh-port', str(fuchsia_ssh_port)])
      gpu_script.extend(['--fuchsia-system-log-file', args.system_log_file])
      if args.verbose:
        gpu_script.append('-v')

      # Set up logging of WebEngine
      listener = target.RunCommandPiped(['log_listener'],
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)
      build_ids_paths = map(
          lambda package_name: os.path.join(
              web_engine_dir, package_name, 'ids.txt'),
          package_names)
      symbolizer = RunSymbolizer(listener.stdout, open(args.system_log_file,
                                                       'w'), build_ids_paths)

      # Keep the Amber repository live while the test runs.
      with target.GetAmberRepo():
        # Install necessary packages on the device.
        far_files = map(
            lambda package_name: os.path.join(
                web_engine_dir, package_name, package_name + '.far'),
            package_names)
        target.InstallPackage(far_files)
        return subprocess.call(gpu_script)
  finally:
    if temp_log_file:
      shutil.rmtree(os.path.dirname(args.system_log_file))


if __name__ == '__main__':
  sys.exit(main())
