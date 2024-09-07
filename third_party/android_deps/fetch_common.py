# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""3pp fetch.py shared logic for libs."""

import argparse
import dataclasses
import json
import os
import re
import typing
import urllib.request


@dataclasses.dataclass
class Spec:
    repo_url: str
    group_name: str
    module_name: str
    file_ext: str
    patch_version: str = ''
    version_override: typing.Optional[str] = None
    version_filter: typing.Optional[str] = None


def do_latest(spec):
    if spec.version_override is not None:
        print(spec.version_override + f'.{spec.patch_version}')
        return
    maven_metadata_url = '{}/{}/{}/maven-metadata.xml'.format(
        spec.repo_url, spec.group_name, spec.module_name)
    metadata = urllib.request.urlopen(maven_metadata_url).read().decode(
        'utf-8')
    # Do not parse xml with the python included parser since it is susceptible
    # to maliciously crafted xmls. Only use regular expression parsing to be
    # safe. RE should be enough to handle what we need to extract.
    versions = re.findall('<version>([^<]+)</version>', metadata)
    if m := re.search('<latest>([^<]+)</latest>', metadata):
        versions.append(m.group(1))

    # If no latest info was found just hope the versions are sorted and the
    # last one is the latest (as is commonly the case).
    if spec.version_filter is not None:
        r = re.compile(spec.version_filter)
        versions = [v for v in versions if r.search(v)]
    latest = versions[-1]
    print(latest + f'.{spec.patch_version}')


def get_download_url(version, spec):
    # Remove the patch version when getting the download url
    version_no_patch, patch = version.rsplit('.', 1)
    if patch.startswith('cr'):
        version = version_no_patch
    file_url = '{0}/{1}/{2}/{3}/{2}-{3}.{4}'.format(spec.repo_url,
                                                    spec.group_name,
                                                    spec.module_name, version,
                                                    spec.file_ext)
    file_name = file_url.rsplit('/', 1)[-1]

    partial_manifest = {
        'url': [file_url],
        'name': [file_name],
        'ext': '.' + spec.file_ext,
    }
    print(json.dumps(partial_manifest))


def main(spec):
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(required=True)

    latest = sub.add_parser('latest')
    latest.set_defaults(func=lambda _opts: do_latest(spec))

    download = sub.add_parser('get_url')
    download.set_defaults(
        func=lambda _opts: get_download_url(os.environ['_3PP_VERSION'], spec))

    opts = ap.parse_args()
    opts.func(opts)
