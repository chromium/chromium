#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pushes changes to web-platform-tests inside Chromium to the upstream repo."""

from blinkpy.common.host import Host
from blinkpy.w3c.pr_cleanup_tool import PrCleanupTool


def main():
    host = Host()
    exporter = PrCleanupTool(host)
    success = exporter.main()
    host.exit(0 if success else 1)


if __name__ == '__main__':
    main()
