#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import requests

_PACKAGE_ID = "microsoft.direct3d.d3d12"

def get_versions():
    """Fetches available versions of _PACKAGE_ID from NuGet API."""
    # Registration API index for the package
    url = f"https://api.nuget.org/v3/registration5-semver1/{_PACKAGE_ID}/index.json"

    response = requests.get(url)
    response.raise_for_status()
    data = response.json()

    versions = []
    # Items are grouped into 'pages' in the NuGet API
    for page in data.get('items', []):
        for item in page.get('items', []):
            versions.append(item['catalogEntry']['version'])

    # NuGet versions usually come in order, but we ensure they are readable
    return versions[::-1]  # Reverse to show newest first

def do_latest():
    print(get_versions()[0])


def get_download_url(version):
    # Package content download URL template
    download_url = f"https://api.nuget.org/v3-flatcontainer/{_PACKAGE_ID}/{version}/{_PACKAGE_ID}.{version}.nupkg"
    partial_manifest = {
        'url': [download_url],
        'ext': '.zip', # .nupkg is a zip file
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()

    latest = sub.add_parser("latest")
    latest.set_defaults(func=do_latest)

    download = sub.add_parser("get_url")
    download.set_defaults(
        func=lambda : get_download_url(os.environ['_3PP_VERSION']))

    opts = ap.parse_args()
    opts.func()


if __name__ == '__main__':
    main()
