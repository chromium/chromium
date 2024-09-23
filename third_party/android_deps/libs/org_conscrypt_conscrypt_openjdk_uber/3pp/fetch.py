#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is generated, do not edit. Update BuildConfigGenerator.groovy and
# 3ppFetch.template instead.

import pathlib
import sys

_3PP_DIR = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(_3PP_DIR.parents[2]))
import fetch_common

_REPO_URL = 'https://repo.maven.apache.org/maven2'
SPEC = fetch_common.Spec(repo_url=_REPO_URL,
                         group_name='org/conscrypt',
                         module_name='conscrypt-openjdk-uber',
                         file_ext='jar',
                         patch_version='cr1',
                         version_override=None,
                         version_filter=None)

if __name__ == '__main__':
    fetch_common.main(SPEC)
