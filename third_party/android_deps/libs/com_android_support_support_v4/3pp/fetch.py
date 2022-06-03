#!/usr/bin/env python
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is generated, do not edit. Update BuildConfigGenerator.groovy and
# 3ppFetch.template instead.

from __future__ import print_function

import argparse
import json
import os

_FILE_URL = 'https://dl.google.com/dl/android/maven2/com/android/support/support-v4/28.0.0/support-v4-28.0.0.aar'
_FILE_NAME = 'support-v4-28.0.0.aar'
_FILE_VERSION = '28.0.0'


def do_latest():
    print(_FILE_VERSION)


def get_download_url(version):
    if _FILE_URL.endswith('.jar'):
        ext = '.jar'
    elif _FILE_URL.endswith('.aar'):
        ext = '.aar'
    else:
        raise Exception('Unsupported extension for %s' % _FILE_URL)

    partial_manifest = {
        'url': [_FILE_URL],
        'name': [_FILE_NAME],
        'ext': ext,
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
