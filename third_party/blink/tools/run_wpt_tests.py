#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing
import sys

from blinkpy.wpt_tests import wpt_adapter


if __name__ == '__main__':
    multiprocessing.set_start_method('spawn')
    sys.exit(wpt_adapter.main(sys.argv[1:]))
