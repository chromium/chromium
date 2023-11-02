#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script runs unit tests of the code in the gpu_tests/ directory.

This script DOES NOT run tests. run_gpu_test does that.
"""

import sys

import gpu_project_config
from gpu_path_util import setup_telemetry_paths  # pylint: disable=unused-import

from telemetry.testing import unittest_runner


def main():
  args = sys.argv[1:]
  return unittest_runner.Run(gpu_project_config.CONFIG,
                             no_browser=True,
                             passed_args=args)


if __name__ == '__main__':
  sys.exit(main())
