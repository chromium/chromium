#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import urllib.request

_RELEASES_URL = 'https://api.github.com/repos/google/bundletool/releases/latest'
_RELEASE_URL = 'https://api.github.com/repos/google/bundletool/releases/tags/{}'


def fetch_json(url):
    return json.load(urllib.request.urlopen(url))


def do_latest():
    json_dict = fetch_json(_RELEASES_URL)
    print(json_dict['tag_name'])


def get_download_url():
    json_dict = fetch_json(_RELEASE_URL.format(os.environ['_3PP_VERSION']))
    # There is currently just one asset, but do a quick filter to guard against
    # possible future assets.
    urls = [x['browser_download_url'] for x in json_dict['assets']]
    urls = [x for x in urls if 'bundletool-all' in x and x.endswith('.jar')]
    if len(urls) != 1:
        raise Exception('len(urls) != 1: ' + '\n'.join(urls))

    partial_manifest = {
        'url': urls,
        # Used only if recipe needs to extract.
        'ext': '',
        # Use constant filename, so as to not need to update filepaths when
        # bundletool is autorolled.
        'name': ['bundletool.jar'],
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()

    latest = sub.add_parser("latest")
    latest.set_defaults(func=do_latest)

    download = sub.add_parser("get_url")
    download.set_defaults(func=get_download_url)

    ap.parse_args().func()


if __name__ == '__main__':
    main()
