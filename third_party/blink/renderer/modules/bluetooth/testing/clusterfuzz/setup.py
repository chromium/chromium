# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script that helps run the fuzzer locally and in ClusterFuzz.

   To prepare to run the fuzzer locally, this script copies the necessary
   resources.

   To prepare to run the fuzzer in ClusterFuzz this script generates two zip
   files: web_bluetooth_fuzzer.tar.bz2. This zip file can then be directly
   uploaded to ClusterFuzz.
"""

import argparse
import glob
import os
import shutil
import sys

# src path from this file's path.
SRC_PATH = os.path.join(os.pardir, os.pardir, os.pardir, os.pardir, os.pardir,
                        os.pardir, os.pardir)
WEB_TESTS_RESOURCES_PATH = os.path.join(SRC_PATH, 'third_party', 'blink',
                                        'web_tests', 'resources')
WEB_PLATFORM_TESTS_RESOURCES_PATH = os.path.join(
    SRC_PATH, 'third_party', 'blink', 'web_tests', 'external', 'wpt',
    'bluetooth', 'resources')
COMMON_FUZZER_RESOURCES_PATH = os.path.join(SRC_PATH, 'testing', 'clusterfuzz',
                                            'common')
RESOURCES = [
    os.path.join(WEB_TESTS_RESOURCES_PATH, 'testharness.js'),
    os.path.join(WEB_TESTS_RESOURCES_PATH, 'testharnessreport.js'),
    os.path.join(WEB_PLATFORM_TESTS_RESOURCES_PATH, 'bluetooth-test.js'),
    os.path.join(WEB_PLATFORM_TESTS_RESOURCES_PATH,
                 'bluetooth-fake-devices.js'),
    os.path.join(COMMON_FUZZER_RESOURCES_PATH, 'fuzzy_types.py'),
    os.path.join(COMMON_FUZZER_RESOURCES_PATH, 'utils.py'),
    os.path.join(COMMON_FUZZER_RESOURCES_PATH, '__init__.py'),
]


def _GetCurrentPath():
    """Returns this file's directory path"""
    return os.path.dirname(os.path.realpath(__file__))


def RetrieveResources():
    # Create resources directory if it doesn't exist. Clear it otherwise.
    current_path = _GetCurrentPath()
    resources_path = os.path.join(current_path, 'resources')
    if not os.path.exists(resources_path):
        print('\'resources\' folder doesn\'t exist. Creating one...')
        os.makedirs(resources_path)
    else:
        print('\'resources\' folder already exists. Clearing it...')
        filelist = glob.glob(os.path.join(resources_path, '*'))
        for f in filelist:
            os.remove(f)

    # Copy necessary files.
    for r in RESOURCES:
        print('Copying: ' + os.path.abspath(os.path.join(current_path, r)))
        shutil.copy(os.path.join(current_path, r), resources_path)

    return resources_path


def main():

    # Get arguments.
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-c',
        '--cluster_fuzz',
        action='store_true',
        help='If present, this script generates tar.bz2 file '
        'containing the fuzzer. This file can be uploaded '
        'and run on ClusterFuzz.')
    parser.add_argument(
        '-l',
        '--local',
        action='store_true',
        help='If present, this script retrieves the files '
        'necessary to run the fuzzer locally.')

    args = parser.parse_args()

    if not (args.cluster_fuzz or args.local):
        print('No action requested.')
        return

    RetrieveResources()

    # To run locally we only need to copy the files.
    if args.local:
        print('Ready to run locally!')

    # To run on ClusterFuzz we create a tar.bz2 file.
    if args.cluster_fuzz:
        # Clean directory
        current_path = _GetCurrentPath()
        filelist = glob.glob(os.path.join(current_path, '*.pyc'))
        for f in filelist:
            os.remove(f)

        # Compress folder to upload
        compressed_file_path = os.path.join(current_path,
                                            'web_bluetooth_fuzzer')
        shutil.make_archive(
            compressed_file_path,
            format='bztar',
            root_dir=os.path.join(current_path, os.pardir),
            base_dir='clusterfuzz')
        print('File written to: ' + compressed_file_path + '.tar.bz2')


if __name__ == '__main__':
    sys.exit(main())
