#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pathlib
import sys

_3PP_DIR = pathlib.Path(__file__).resolve().parent
_SRC_ROOT = _3PP_DIR.parents[3]
sys.path.insert(0, str(_SRC_ROOT / 'build' / '3pp_common'))
import fetch_github_release


def _get_artifact_regex(platform_3pp):
    if platform_3pp == 'linux-amd64':
        return r'protoc-.*-linux-x86_64\.zip$'
    if platform_3pp == 'mac-arm64':
        return r'protoc-.*-osx-aarch_64\.zip$'
    raise RuntimeError('Unsupported 3pp platform %s' % platform_3pp)


if __name__ == '__main__':
    fetch_github_release.main(
        project='protocolbuffers/protobuf',
        install_scripts=[_3PP_DIR / 'install.sh'],
        artifact_extension='.zip',
        artifact_regex=_get_artifact_regex(os.environ['_3PP_PLATFORM']))
