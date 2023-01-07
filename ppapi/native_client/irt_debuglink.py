#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run objcopy --add-gnu-debuglink for the NaCl IRT.
"""

import subprocess
import sys


def Main(args):
  objcopy, debug_file, stripped_file, output_file = args
  return subprocess.call([
      objcopy, '--add-gnu-debuglink', debug_file, stripped_file, output_file
      ])


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
