#!/usr/bin/env vpython
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from blinkpy.common.host import Host
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.w3c.android_wpt_expectations_updater import (
    AndroidWPTExpectationsUpdater)


def get_updater(host=None, args=None):
    host = host or Host()
    args = args or []
    if ('--update-android-expectations-only' in args or
            any(arg.startswith('--android-product') for arg in args)):
        return AndroidWPTExpectationsUpdater(host, args)
    else:
        return WPTExpectationsUpdater(host, args)


def main(args):
    host = Host()
    updater = get_updater(host, args)
    return updater.run()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
