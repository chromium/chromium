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
--shard-index : -- this-chunk
"""

# TODO(aluo): Combine or factor out commons parts with run_wpt_tests.py script.

import argparse
import contextlib
import json
import logging
import os
import sys

import common

from wpt_android_lib import (
    WPTWeblayerAdapter, WPTWebviewAdapter, WPTClankAdapter,
    add_emulator_args, get_devices)

logger = logging.getLogger(__name__)

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
BLINK_TOOLS_DIR = os.path.join(
    SRC_DIR, 'third_party', 'blink', 'tools')
CATAPULT_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult')
DEFAULT_WPT = os.path.join(
    SRC_DIR, 'third_party', 'wpt_tools', 'wpt', 'wpt')
PYUTILS = os.path.join(CATAPULT_DIR, 'common', 'py_utils')
TOMBSTONE_PARSER = os.path.join(SRC_DIR, 'build', 'android', 'tombstones.py')

if BLINK_TOOLS_DIR not in sys.path:
  sys.path.append(BLINK_TOOLS_DIR)

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

import devil_chromium

from blinkpy.web_tests.port.android import (
    PRODUCTS, ANDROID_WEBLAYER,
    ANDROID_WEBVIEW, CHROME_ANDROID)

from devil import devil_env

def _get_adapter(product, devices):
  if product == ANDROID_WEBLAYER:
    return WPTWeblayerAdapter(devices)
  elif product == ANDROID_WEBVIEW:
    return WPTWebviewAdapter(devices)
  else:
    return WPTClankAdapter(devices)


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)


def main():
  devil_chromium.Initialize()

  usage = '%(prog)s --product={' + ','.join(PRODUCTS) + '} ...'
  product_parser = argparse.ArgumentParser(
      add_help=False, prog='run_android_wpt.py', usage=usage)
  product_parser.add_argument(
      '--product', action='store', required=True, choices=PRODUCTS)
  add_emulator_args(product_parser)
  args, _ = product_parser.parse_known_args()
  product = args.product

  with get_devices(args) as devices:
    if not devices:
      logger.error('There are no devices attached to this host. Exiting...')
      return

    adapter = _get_adapter(product, devices)
    if adapter.options.verbose:
      if adapter.options.verbose == 1:
        logger.setLevel(logging.INFO)
      else:
        logger.setLevel(logging.DEBUG)

    # WPT setup for chrome and webview requires that PATH contains adb.
    platform_tools_path = os.path.dirname(devil_env.config.FetchPath('adb'))
    os.environ['PATH'] = ':'.join([platform_tools_path] +
                                  os.environ['PATH'].split(':'))

    return adapter.run_test()


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  logging.basicConfig(level=logging.WARNING)
  logger = logging.getLogger()
  sys.exit(main())
