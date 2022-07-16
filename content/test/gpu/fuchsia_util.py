# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

from gpu_tests import path_util

sys.path.insert(0,
                os.path.join(path_util.GetChromiumSrcDir(), 'build', 'fuchsia'))
from common_args import (AddCommonArgs, AddTargetSpecificArgs, ConfigureLogging,
                         GetDeploymentTargetForArgs)
from symbolizer import BuildIdsPaths


def RunTestOnFuchsiaDevice(script_cmd):
  """Preps Fuchsia device with pave and package update, then runs script."""

  parser = argparse.ArgumentParser()
  AddCommonArgs(parser)
  AddTargetSpecificArgs(parser)
  runner_script_args, test_args = parser.parse_known_args()
  ConfigureLogging(runner_script_args)

  # If out_dir is not set, assume the script is being launched
  # from the output directory.
  if not runner_script_args.out_dir:
    runner_script_args.out_dir = os.getcwd()

  # Create a temporary log file that Telemetry will look to use to build
  # an artifact when tests fail.
  clean_up_logs_on_exit = False
  if not runner_script_args.logs_dir:
    runner_script_args.logs_dir = tempfile.mkdtemp()

  package_names = ['web_engine_with_webui', 'web_engine_shell']
  web_engine_dir = os.path.join(runner_script_args.out_dir, 'gen', 'fuchsia',
                                'engine')
  package_paths = map(
      lambda package_name: os.path.join(web_engine_dir, package_name),
      package_names)

  # Pass all other arguments to the gpu integration tests.
  script_cmd.extend(test_args)
  try:
    with GetDeploymentTargetForArgs(runner_script_args) as target:
      target.Start()
      target.StartSystemLog(package_paths)
      fuchsia_device_address, fuchsia_ssh_port = target._GetEndpoint()
      script_cmd.extend(
          ['--chromium-output-directory', runner_script_args.out_dir])
      script_cmd.extend(['--fuchsia-device-address', fuchsia_device_address])
      script_cmd.extend(['--fuchsia-ssh-config', target._GetSshConfigPath()])
      if fuchsia_ssh_port:
        script_cmd.extend(['--fuchsia-ssh-port', str(fuchsia_ssh_port)])
      script_cmd.extend([
          '--fuchsia-system-log-file',
          os.path.join(runner_script_args.logs_dir, 'system_log')
      ])
      # Add to the script
      if runner_script_args.verbose:
        script_cmd.append('-v')

      # Keep the package repository live while the test runs.
      with target.GetPkgRepo():
        # Install necessary packages on the device.
        far_files = map(
            lambda package_name: os.path.join(web_engine_dir, package_name,
                                              package_name + '.far'),
            package_names)
        target.InstallPackage(far_files)
        return subprocess.call(script_cmd)
  finally:
    if clean_up_logs_on_exit:
      shutil.rmtree(runner_script_args.logs_dir)
