# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit checks for ios/testing

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

def CheckChange(input_api, output_api):
    import sys
    old_sys_path = sys.path[:]
    results = []
    try:
        sys.path.append(input_api.change.RepositoryRoot())
        from build.ios import presubmit_support
        results += presubmit_support.CheckBundleData(input_api,
                                                     output_api,
                                                     'http_server_bundle_data',
                                                     globroot='.')
    finally:
        sys.path = old_sys_path
    return results
