#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

SRC_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(os.path.join(SRC_DIR, 'tools', 'perf'))

import process_perf_results

if __name__ == '__main__':
  sys.exit(process_perf_results.main())
