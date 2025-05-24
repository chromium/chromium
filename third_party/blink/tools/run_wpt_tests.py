#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import multiprocessing
import sys

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s.%(msecs)03d %(levelname)s %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S')
logger = logging.getLogger('run_wpt_tests')


if __name__ == '__main__':
    logger.info('Running web platform tests')
    multiprocessing.set_start_method('spawn')
    from blinkpy.wpt_tests import wpt_adapter
    sys.exit(wpt_adapter.main(sys.argv[1:]))
