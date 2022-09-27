#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Process WPT results for upload to ResultDB."""

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.common.path_finder import add_depot_tools_dir_to_os_path
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor


def main():
    add_depot_tools_dir_to_os_path()
    host = Host()
    options = WPTResultsProcessor.parse_args()
    processor = WPTResultsProcessor.from_options(host, options)
    try:
        host.exit(processor.main(options))
    except KeyboardInterrupt:
        host.print_('Interrupted, exiting')
        host.exit(exit_codes.INTERRUPTED_EXIT_STATUS)


if __name__ == '__main__':
    main()
