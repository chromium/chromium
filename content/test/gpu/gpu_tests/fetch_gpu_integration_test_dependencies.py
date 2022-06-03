#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.append(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
import gpu_path_util  # pylint: disable=wrong-import-position

gpu_path_util.AddDirToPathIfNeeded(gpu_path_util.CATAPULT_DIR, 'devil')
from devil import devil_env  # pylint: disable=wrong-import-position


def main():
  if len(sys.argv) != 1:
    print('Usage: {}'.format(sys.argv[0]))
    return
  devil_env.config.FetchPath('adb')


if __name__ == '__main__':
  main()
