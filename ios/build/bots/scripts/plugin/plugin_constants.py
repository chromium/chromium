# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import platform
import shutil
import stat
import sys

from enum import Enum

# Path constants
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
# we are currently at src/ios/build/bots/scripts/plugin/, unless we are in pwd
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../../..'))
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))

PLUGIN_PROTOS_PATH = os.path.abspath(
    os.path.join(CHROMIUM_SRC_DIR, 'ios/testing/plugin'))

PLUGIN_SERVICE_WORKER_COUNT = 10
# just picking a random port
PLUGIN_SERVICE_ADDRESS = 'localhost:32279'
PLUGIN_PROXY_SERVICE_PORT = '20000'
REMOTE_PLUGIN_PROXY_PORT = '40000'
# Max number of times a test case can be video recorded and saved to disk
MAX_RECORDED_COUNT = 3
# Options for enabling video recording on EG tests
VIDEO_RECORDER_PLUGIN_OPTIONS = Enum('video_recorder_plugin_options',
                                     {'failed_only': 1})
SIMULATOR_FOLDERS = [
    os.path.expanduser('~/Library/Developer/CoreSimulator/Devices'),
    os.path.expanduser('~/Library/Developer/XCTestDevices')
]
