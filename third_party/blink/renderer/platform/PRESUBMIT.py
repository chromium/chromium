# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting Source/platform.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import difflib
import os
import re

USE_PYTHON3 = True
RUNTIMEENABLED_NAME = re.compile(r'\s*name\s*:\s*"([^"]*)"')


def RuntimeEnabledFeatureNames(filename):
    """Reads the 'name' of each feature in runtime_enabled_features.json5."""
    # Note: We don't have a JSON5 parser available, so just use a regex.
    with open(filename) as f:
        for line in f:
            match = RUNTIMEENABLED_NAME.match(line)
            if match:
                yield match.group(1)


def _CheckRuntimeEnabledFeaturesSorted(input_api, output_api):
    """Check: runtime_enabled_features.json5 feature list sorted alphabetically.
    """
    # Read runtime_enabled_features.json5 using the JSON5 parser.
    filename = os.path.join(input_api.PresubmitLocalPath(),
                            'runtime_enabled_features.json5')
    features = list(RuntimeEnabledFeatureNames(filename))

    # Sort the 'data' section by name.
    features_sorted = sorted(features, key=lambda s: s.lower())

    if features == features_sorted:
        return []

    # Diff the sorted/unsorted versions.
    differ = difflib.Differ()
    diff = differ.compare(features, features_sorted)
    return [
        output_api.PresubmitError(
            'runtime_enabled_features.json5 features must be sorted alphabetically. '
            'Diff of feature order follows:',
            long_text='\n'.join(diff))
    ]


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(_CheckRuntimeEnabledFeaturesSorted(input_api, output_api))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
