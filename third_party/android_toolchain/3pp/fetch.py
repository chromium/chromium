#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import urllib.request


def get_md():
    # We read the raw wiki markdown as it is the easiest to parse.
    with urllib.request.urlopen(
            'https://raw.githubusercontent.com/wiki/android/ndk/Home.md'
    ) as response:
        return response.read().decode('utf-8')


def version_key(v):
    # Eg. "29.0.14206865"
    return [int(x) for x in v.split('.')]


def do_latest():
    md = get_md()
    versions = re.findall(r'ndkVersion\s+"([^"]+)"', md)
    if not versions:
        raise Exception('Could not find any ndkVersion in Home.md')
    print(max(versions, key=version_key))


def get_url():
    v = os.environ['_3PP_VERSION']
    md = get_md()

    # Split by sections starting with ### as these headers seperate the LTS,
    # current release, and beta/RC.
    sections = md.split('###')
    for section in sections:
        if f'ndkVersion "{v}"' in section:
            # Find linux download url in this section.
            match = re.search(r'href="(http\S+android-ndk-[^"]+-linux\.zip)"',
                              section)
            if match:
                artifact_url = match.group(1)
                partial_manifest = {'url': [artifact_url], 'ext': '.zip'}
                print(json.dumps(partial_manifest))
                return

    raise Exception(f'Could not find URL for version {v} in github wiki.')


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(required=True)

    latest = sub.add_parser("latest")
    latest.set_defaults(func=do_latest)

    download = sub.add_parser("get_url")
    download.set_defaults(func=get_url)

    opts = ap.parse_args()
    opts.func()


if __name__ == '__main__':
    main()
