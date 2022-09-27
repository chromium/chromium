#!/usr/bin/env vpython3
#
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

BLINK_TOOLS_PATH = os.path.abspath(os.path.dirname(__file__))

def main():
    path_to_merge_script = os.path.join(BLINK_TOOLS_PATH,
                                        'blinkpy',
                                        'web_tests',
                                        'merge_results.py')
    command = ['python3', path_to_merge_script] + sys.argv[1:]
    subprocess.check_call(command)

if __name__ == '__main__':
    main()
