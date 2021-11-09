#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple script for xvfb_unittest to launch.

This script outputs formatted data to stdout for the xvfb unit tests
to read and compare with expected output.
"""

from __future__ import print_function

import os
import signal
import sys
import time


def print_signal(sig, *_):
  # print_function does not guarantee its output won't be interleaved
  # with other logging elsewhere, but it does guarantee its output
  # will appear intact. Because the tests parse via starts_with, prefix
  # with a newline. These tests were previously flaky due to output like
  # > Signal: 1 <other messages>.
  print('\nSignal :{}'.format(sig))


if __name__ == '__main__':
  signal.signal(signal.SIGTERM, print_signal)
  signal.signal(signal.SIGINT, print_signal)

  # test the subprocess display number.
  print('\nDisplay :{}'.format(os.environ.get('DISPLAY', 'None')))

  if len(sys.argv) > 1 and sys.argv[1] == '--sleep':
    time.sleep(2)  # gives process time to receive signal.
