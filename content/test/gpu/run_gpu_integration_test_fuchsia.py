#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Wrapper for running gpu integration tests on Fuchsia devices."""

from __future__ import print_function

import os
import sys

import fuchsia_util
import gpu_path_util


def main():
  gpu_script = [
      os.path.join(gpu_path_util.GPU_DIR, 'run_gpu_integration_test.py')
  ]
  return fuchsia_util.RunTestOnFuchsiaDevice(gpu_script)


if __name__ == '__main__':
  sys.exit(main())
