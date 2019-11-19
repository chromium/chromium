#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for use in test_env unittests."""

import os
import signal
import sys
import time


def print_signal(sig, *_):
  print 'Signal :{}'.format(sig)


if __name__ == '__main__':
  signal.signal(signal.SIGTERM, print_signal)
  signal.signal(signal.SIGINT, print_signal)
  if sys.platform == 'win32':
    signal.signal(signal.SIGBREAK, print_signal)
  time.sleep(2)  # gives process time to receive signal.
