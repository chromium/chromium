#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pushes changes to web-platform-tests inside Chromium to the upstream repo."""

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.common.path_finder import add_depot_tools_dir_to_os_path
from blinkpy.w3c.test_exporter import TestExporter


def main():
    add_depot_tools_dir_to_os_path()
    host = Host()
    exporter = TestExporter(host)
    try:
        success = exporter.main()
        host.exit(0 if success else 1)
    except KeyboardInterrupt:
        host.print_('Interrupted, exiting')
        host.exit(exit_codes.INTERRUPTED_EXIT_STATUS)


if __name__ == '__main__':
    main()
