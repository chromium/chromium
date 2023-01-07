#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is customized based on 3ppFetch.template.

import argparse
import re
import urllib.request

_REPO_URL = 'https://dl.google.com/android/maven2'
_GROUP_NAME = 'com/android/tools/apkparser'
_MODULE_NAME = 'apkanalyzer'
_OVERRIDE_LATEST = None
_PATCH_VERSION = 'cr0'


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


def do_checkout():
    # Everything is done in install.py. This method allows us to bypass having
    # to download an unnecessary file.
    pass


def main():
    argparser = argparse.ArgumentParser()
    subparser = argparser.add_subparsers()

    latest = subparser.add_parser('latest')
    latest.set_defaults(func=do_latest)

    checkout = subparser.add_parser('checkout')
    checkout.add_argument('checkout_path')  # Needed only to avoid parse error.
    checkout.set_defaults(func=do_checkout)

    args = argparser.parse_args()
    args.func()


if __name__ == '__main__':
    main()
