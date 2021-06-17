#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script runs unit tests of the code in the gpu_tests/ directory.

This script DOES NOT run tests. run_gpu_test does that.
"""

from __future__ import print_function

import sys

from gpu_tests import path_util

path_util.SetupTelemetryPaths()

import gpu_project_config

from telemetry.testing import unittest_runner


def main():
  args = sys.argv[1:]
  return unittest_runner.Run(gpu_project_config.CONFIG,
                             no_browser=True,
                             passed_args=args)


if __name__ == '__main__':
  sys.exit(main())
