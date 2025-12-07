#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import re
import stat
import sys
import urllib.request
import zipfile

# Outside docker: //path/to/project/3pp/fetch.py
# Inside docker: //path/to/project/install.py
_THIS_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _THIS_DIR.parents[3 if _THIS_DIR.name == "3pp" else 2]

sys.path.insert(1, str(_SRC_ROOT / "build" / "3pp_common"))
import common

_FILE_URL = 'https://dl.google.com/dl/android/maven2/com/android/tools/build/aapt2/{0}/aapt2-{0}-linux.jar'
_GROUP_INDEX_URL = 'https://dl.google.com/dl/android/maven2/com/android/tools/build/group-index.xml'


def do_latest():
    """Fetches the latest version of aapt2."""
    response = urllib.request.urlopen(_GROUP_INDEX_URL)
    match = re.search(r'<aapt2 versions="([^"]+)"',
                      response.read().decode('utf-8'))
    assert match, "Failed to match aapt2 versions from URL."
    versions = match.group(1).split(',')
    # The versions appear to be sorted already, no need to deal with sorting
    # their weird version scheme ourselves. Just print the last one.
    return versions[-1]


def do_install(args):
    """Downloads and extracts the aapt2 binary."""
    version = args.version
    url = _FILE_URL.format(version)
    jar_path = 'aapt2.jar'
    common.download_file(url, jar_path)

    with zipfile.ZipFile(jar_path) as z:
        z.extract('aapt2', '.')

    aapt2_path = 'aapt2'
    st = os.stat(aapt2_path)
    os.chmod(aapt2_path, st.st_mode | stat.S_IXUSR | stat.S_IXGRP)

    os.rename(aapt2_path, os.path.join(args.output_prefix, 'aapt2'))


def main():
    common.main(do_latest=do_latest, do_install=do_install, runtime_deps=[])


if __name__ == '__main__':
    main()
