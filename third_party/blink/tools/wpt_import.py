#!/usr/bin/env vpython3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pulls the latest revisions of the web-platform-tests."""

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.common.path_finder import add_depot_tools_dir_to_os_path
from blinkpy.w3c.test_importer import TestImporter


def main():
    add_depot_tools_dir_to_os_path()
    host = Host()
    importer = TestImporter(host)
    try:
        with importer:
            host.exit(importer.main())
    except KeyboardInterrupt:
        host.print_("Interrupted, exiting")
        host.exit(exit_codes.INTERRUPTED_EXIT_STATUS)


if __name__ == '__main__':
    main()
