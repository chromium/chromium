#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Web Platform Tests (WPT) on Android browsers.

This script supports running tests on the Chromium Waterfall by mapping isolated
script flags to WPT flags.

It is also useful for local reproduction by performing APK installation and
configuring the browser to resolve test hosts.  Be sure to invoke this
executable directly rather than using python run_android_wpt.py so that
WPT dependencies in Chromium vpython are found.

If you need more advanced test control, please use the runner located at
//third_party/wpt_tools/wpt/wpt.

Here's the mapping [isolate script flag] : [wpt flag]
--isolated-script-test-output : --log-chromium
--total-shards : --total-chunks
--shard-index : --this-chunk
"""

# TODO(crbug/1274933): Combine or factor out commons parts with
# run_wpt_tests.py script.

import json
import os
import sys

import common

from wpt_android_lib import WPTAndroidAdapter

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
BLINK_TOOLS_DIR = os.path.join(
    SRC_DIR, 'third_party', 'blink', 'tools')

if BLINK_TOOLS_DIR not in sys.path:
  sys.path.append(BLINK_TOOLS_DIR)

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)


def main():
    adapter = WPTAndroidAdapter()
    return adapter.run_test()


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main())
