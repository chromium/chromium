# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import subprocess
import tempfile

from gpu_path_util import setup_fuchsia_paths  # pylint: disable=unused-import

from common_args import (AddCommonArgs, AddTargetSpecificArgs, ConfigureLogging,
                         GetDeploymentTargetForArgs)


def RunTestOnFuchsiaDevice(script_cmd):
  """Preps Fuchsia device with pave and package update, then runs script."""

  parser = argparse.ArgumentParser()
  AddCommonArgs(parser)
  AddTargetSpecificArgs(parser)
  parser.add_argument('--browser',
                      choices=['web-engine-shell', 'fuchsia-chrome'])
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

  if runner_script_args.browser == 'web-engine-shell':
    package_names = ['web_engine_with_webui', 'web_engine_shell']
    package_dir = os.path.join(runner_script_args.out_dir, 'gen', 'fuchsia_web',
                               'webengine')
  else:
    package_names = ['chrome']
    package_dir = os.path.join(runner_script_args.out_dir, 'gen', 'chrome',
                               'app')

  package_paths = list(
      map(lambda package_name: os.path.join(package_dir, package_name),
          package_names))

  # Pass all other arguments to the gpu integration tests.
  script_cmd.extend(test_args)
  try:
    with GetDeploymentTargetForArgs(runner_script_args) as target:
      target.Start()
      target.StartSystemLog(package_paths)
      # pylint: disable=protected-access
      fuchsia_device_address, fuchsia_ssh_port = target._GetEndpoint()
      # pylint: enable=protected-access
      script_cmd.extend(
          ['--chromium-output-directory', runner_script_args.out_dir])
      script_cmd.extend(['--fuchsia-device-address', fuchsia_device_address])
      # pylint: disable=protected-access
      script_cmd.extend(['--fuchsia-ssh-config', target._GetSshConfigPath()])
      # pylint: enable=protected-access
      if fuchsia_ssh_port:
        script_cmd.extend(['--fuchsia-ssh-port', str(fuchsia_ssh_port)])
      script_cmd.extend([
          '--fuchsia-system-log-file',
          os.path.join(runner_script_args.logs_dir, 'system_log')
      ])
      script_cmd.extend(['--browser', runner_script_args.browser])
      # Add to the script
      if runner_script_args.verbose:
        script_cmd.append('-v')

      # Keep the package repository live while the test runs.
      with target.GetPkgRepo():
        # Install necessary packages on the device.
        far_files = list(
            map(
                lambda package_name: os.path.join(package_dir, package_name,
                                                  package_name + '.far'),
                package_names))
        target.InstallPackage(far_files)
        return subprocess.call(script_cmd)
  finally:
    if clean_up_logs_on_exit:
      shutil.rmtree(runner_script_args.logs_dir)
