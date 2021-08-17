#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pre-collects web tests and save the result to AllTestsByDirectories.json"""

import sys

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.web_tests.web_tests_collector import WebTestsCollector


def main():
    host = Host()
    collector = WebTestsCollector(host)
    try:
        return collector.main(sys.argv[1:])
    except KeyboardInterrupt:
        print("Interrupted, exiting")
        return exit_codes.INTERRUPTED_EXIT_STATUS


if __name__ == '__main__':
    sys.exit(main())
