#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import urllib.request

_FILE_URL = 'https://dl.google.com/dl/android/maven2/com/android/tools/build/aapt2/{0}/aapt2-{0}-linux.jar'
_GROUP_INDEX_URL = 'https://dl.google.com/dl/android/maven2/com/android/tools/build/group-index.xml'
_FILE_NAME = 'aapt2-{0}-linux.jar'


def do_latest():
    response = urllib.request.urlopen(_GROUP_INDEX_URL)
    match = re.search(r'<aapt2 versions="([^"]+)"',
                      response.read().decode('utf-8'))
    versions = match.group(1).split(',')
    # The versions appear to be sorted already, no need to deal with sorting
    # their weird version scheme ourselves. Just print the last one.
    print(versions[-1])


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
