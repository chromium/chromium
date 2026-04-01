#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import urllib.request


def do_latest():
    data = json.load(
        urllib.request.urlopen(
            'https://ci.android.com/builds/branches/aosp-master-ndk/status.json'
        ))
    for t in data['targets']:
        if t['name'] == 'linux':
            print(t['last_known_good_build'])
            return


def get_url():
    v = os.environ['_3PP_VERSION']
    html_url = (f'https://ci.android.com/builds/submitted/{v}/linux/latest/'
                f'android-ndk-{v}-linux-x86_64.zip')
    with urllib.request.urlopen(html_url) as response:
        html = response.read().decode('utf-8')

    # Extract the artifactUrl from the JSVariables in the HTML.
    match = re.search(r'"artifactUrl":("[^"]+")', html)
    if not match:
        raise Exception(f'Could not find artifactUrl in HTML at {html_url}')

    # Using json.loads on the quoted string will handle Unicode escapes like
    # \u0026 automatically.
    artifact_url = json.loads(match.group(1))

    partial_manifest = {'url': [artifact_url], 'ext': '.zip'}
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(required=True)

    sub.add_parser('latest').set_defaults(func=do_latest)
    sub.add_parser('get_url').set_defaults(func=get_url)

    opts = ap.parse_args()
    opts.func()


if __name__ == '__main__':
    main()
