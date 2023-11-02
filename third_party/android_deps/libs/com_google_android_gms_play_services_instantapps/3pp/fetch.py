#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is generated, do not edit. Update BuildConfigGenerator.groovy and
# 3ppFetch.template instead.

import argparse
import json
import os
import re
import urllib.request

_REPO_URL = 'https://dl.google.com/dl/android/maven2'
_GROUP_NAME = 'com/google/android/gms'
_MODULE_NAME = 'play-services-instantapps'
_FILE_EXT = 'aar'
_OVERRIDE_LATEST = None
_PATCH_VERSION = 'cr1'


def do_latest():
    if _OVERRIDE_LATEST is not None:
        print(_OVERRIDE_LATEST + f'.{_PATCH_VERSION}')
        return
    maven_metadata_url = '{}/{}/{}/maven-metadata.xml'.format(
        _REPO_URL, _GROUP_NAME, _MODULE_NAME)
    metadata = urllib.request.urlopen(maven_metadata_url).read().decode(
        'utf-8')
    # Do not parse xml with the python included parser since it is susceptible
    # to maliciously crafted xmls. Only use regular expression parsing to be
    # safe. RE should be enough to handle what we need to extract.
    match = re.search('<latest>([^<]+)</latest>', metadata)
    if match:
        latest = match.group(1)
    else:
        # if no latest info was found just hope the versions are sorted and the
        # last one is the latest (as is commonly the case).
        latest = re.findall('<version>([^<]+)</version>', metadata)[-1]
    print(latest + f'.{_PATCH_VERSION}')


def get_download_url(version):
    # Remove the patch version when getting the download url
    version_no_patch, patch = version.rsplit('.', 1)
    if patch.startswith('cr'):
        version = version_no_patch
    file_url = '{0}/{1}/{2}/{3}/{2}-{3}.{4}'.format(_REPO_URL, _GROUP_NAME,
                                                    _MODULE_NAME, version,
                                                    _FILE_EXT)
    file_name = file_url.rsplit('/', 1)[-1]

    partial_manifest = {
        'url': [file_url],
        'name': [file_name],
        'ext': '.' + _FILE_EXT,
    }
    print(json.dumps(partial_manifest))


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers()

    latest = sub.add_parser('latest')
    latest.set_defaults(func=lambda _opts: do_latest())

    download = sub.add_parser('get_url')
    download.set_defaults(
        func=lambda _opts: get_download_url(os.environ['_3PP_VERSION']))

    opts = ap.parse_args()
    opts.func(opts)


if __name__ == '__main__':
    main()
