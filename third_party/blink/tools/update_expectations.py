#!/usr/bin/env vpython
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from blinkpy.common import host
from blinkpy.web_tests import update_expectations
from blinkpy.web_tests.layout_package.bot_test_expectations import BotTestExpectationsFactory

if __name__ == "__main__":
    HOST = host.Host()
    RETURN_CODE = update_expectations.main(
        HOST, BotTestExpectationsFactory(HOST.builders), sys.argv[1:])
    sys.exit(RETURN_CODE)
