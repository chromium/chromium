#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import json
import os

_FILE_URL = 'https://dl.google.com/dl/android/maven2/com/android/tools/build/aapt2/{0}/aapt2-{0}-linux.jar'
_FILE_NAME = 'aapt2-{0}-linux.jar'
# This is the version that the 3pp bot will be building, this is usually the
# same or newer than the one in README.chromium. This version is not actively
# used in Chromium. The authoratative version is the one in README.chromium.
_FILE_VERSION = '7.0.0-beta03-7147631'


def do_latest():
    print(_FILE_VERSION)


def get_download_url(version):
    partial_manifest = {
        'url': [_FILE_URL.format(version)],
        'name': [_FILE_NAME.format(version)],
        'ext': '.jar',
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()

    latest = sub.add_parser("latest")
    latest.set_defaults(func=lambda _opts: do_latest())

    download = sub.add_parser("get_url")
    download.set_defaults(
        func=lambda _opts: get_download_url(os.environ['_3PP_VERSION']))

    opts = ap.parse_args()
    opts.func(opts)


if __name__ == '__main__':
    main()
