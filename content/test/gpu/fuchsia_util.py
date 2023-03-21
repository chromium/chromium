# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess

# pylint: disable=unused-import
from gpu_path_util import CHROMIUM_SRC_DIR, setup_fuchsia_paths
# pylint: enable=unused-import

from common import register_common_args  # pylint: disable=import-error


def RunTestOnFuchsiaDevice(script_type):
  """Helper method that runs Telemetry based tests on Fuchsia."""

  parser = argparse.ArgumentParser(add_help=False)
  register_common_args(parser)
  script_args, rest_args = parser.parse_known_args()

  # If out_dir is not set, assume the script is being launched
  # from the output directory.
  if not script_args.out_dir:
    script_args.out_dir = os.getcwd()

  script = os.path.join(CHROMIUM_SRC_DIR, 'build', 'fuchsia', 'test',
                        'run_test.py')
  script_cmd = [script, script_type, '--out-dir', script_args.out_dir]
  script_cmd.extend(rest_args)

  return subprocess.run(script_cmd, check=False).returncode
