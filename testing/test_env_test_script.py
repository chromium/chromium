#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for use in test_env unittests."""

from __future__ import print_function
import signal
import sys
import time


def print_signal(sig, *_args):
  print('Signal :{}'.format(sig))


if __name__ == '__main__':
  signal.signal(signal.SIGTERM, print_signal)
  signal.signal(signal.SIGINT, print_signal)
  if sys.platform == 'win32':
    signal.signal(signal.SIGBREAK, print_signal)  # pylint: disable=no-member
  time.sleep(2)  # gives process time to receive signal.
