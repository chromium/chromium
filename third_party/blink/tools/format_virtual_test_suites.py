#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Script to format and sort the VirtualTestSuites JSON file.
"""

import json
import sys

from blinkpy.common.host import Host
from blinkpy.common.path_finder import PathFinder
from blinkpy.style.virtual_suites_formatter import format_json_with_comments


def main():
    """Formats the main VirtualTestSuites file in-place."""
    host = Host()
    path_finder = PathFinder(host.filesystem)
    vts_path = path_finder.path_from_web_tests('VirtualTestSuites')
    print(f"Formatting '{vts_path}' in-place.")
    with open(vts_path, 'r') as f:
        entries = json.load(f)
    sorted_json_content = format_json_with_comments(entries)
    with open(vts_path, 'w') as f:
        f.write(sorted_json_content)
    print(
        f"Successfully formatted '{vts_path}'")
    return 0


if __name__ == '__main__':
    sys.exit(main())
