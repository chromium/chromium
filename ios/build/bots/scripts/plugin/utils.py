# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import platform
import shutil
import stat
import sys

# Path constants
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
# we are currently at src/ios/build/bots/scripts/plugin/
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../../..'))
# TODO(crbug.com/1349392): add the below generated protobuf's path to build/config/io/ios_test_runner_wrapper.gni, so the resources are included when building ios test runner script
PLUGIN_PROTOS_PATH = os.path.abspath(
    os.path.join(CHROMIUM_SRC_DIR, 'ios/testing/plugin'))

PLUGIN_SERVICE_WORKER_COUNT = 10
# just picking a random port
PLUGIN_SERVICE_ADDRESS = 'localhost:32279'
