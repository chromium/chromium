# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os

import gpu_path_util

conformance_relcomps = ('third_party', 'webgl', 'src', 'sdk', 'tests')

extensions_relcomps = ('content', 'test', 'data', 'gpu')

conformance_relpath = os.path.join(*conformance_relcomps)
extensions_relpath = os.path.join(*extensions_relcomps)
conformance_path = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR,
                                conformance_relpath)

# These URL prefixes are needed because having more than one static
# server dir is causing the base server directory to be moved up the
# directory hierarchy.
url_prefixes_to_trim = [
    '/'.join(conformance_relcomps) + '/',
    '/'.join(extensions_relcomps) + '/',
]
