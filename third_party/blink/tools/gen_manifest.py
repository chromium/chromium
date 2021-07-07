#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import os
import shutil
import subprocess
import sys

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 os.pardir, os.pardir, os.pardir))
WPT_EXEC_PATH = os.path.join(SRC_DIR, 'third_party', 'wpt_tools', 'wpt', 'wpt')
BASE_MANIFEST_PATH = os.path.join(SRC_DIR, 'third_party', 'blink',
                                  'web_tests', 'external',
                                  'WPT_BASE_MANIFEST_8.json')
MANIFEST_NAME = 'MANIFEST.json'

def maybe_make_directory(path):
    try:
        os.makedirs(path)
    except OSError as error:
        if error.errno != errno.EEXIST:
            raise

def main() :
    usage = "Generate manifest for web test..."
    parser = argparse.ArgumentParser(
        add_help=False, prog='gen_manifest.py', usage=usage)
    parser.add_argument('--out', required=True,
                        help='Path of the output directory')
    options = parser.parse_args()
    if options.out.startswith('//'):
        options.out = os.path.join(SRC_DIR, options.out[2:])

    WPT_DIRS = ['wpt_internal', 'external/wpt']
    for path in WPT_DIRS:
        print('Generating MANIFEST.json for %s...' % path)
        wpt_path = os.path.join(SRC_DIR, 'third_party',
                                'blink', 'web_tests', path)
        manifest_path = os.path.join(options.out, path, MANIFEST_NAME)
        if 'external' in path:
            maybe_make_directory(os.path.dirname(manifest_path))
            shutil.copyfile(BASE_MANIFEST_PATH, manifest_path)
        cmd = [
            sys.executable, WPT_EXEC_PATH, 'manifest', '-v',
            '--no-download', '--tests-root', wpt_path, '--path', manifest_path
        ]
        subprocess.check_output(cmd)

if __name__ == '__main__':
    main()
