# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting Source/platform.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import difflib
import os
import sys

# pyright: reportMissingImports=false
def RuntimeEnabledFeatures(input_api, filename):
    """Returns the features present in the specified features JSON5 file."""

    # We need to wait until we have an input_api object and use this
    # roundabout construct to import json5 because this file is
    # eval-ed and thus doesn't have __file__.
    try:
        json5_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                            '..', '..', '..', 'pyjson5', 'src')
        sys.path.append(json5_path)
        import json5
        return json5.load(open(filename, encoding='utf-8'))['data']
    finally:
        # Restore sys.path to what it was before.
        sys.path.remove(json5_path)


def _CheckRuntimeEnabledFeaturesSorted(features, features_filename,
                                       output_api):
    """Check: runtime_enabled_features.json5 feature list sorted alphabetically.
    """
    names = [feature['name'] for feature in features]

    # Sort the 'data' section by name.
    names_sorted = sorted(names, key=lambda s: s.lower())

    if names == names_sorted:
        return []

    # Diff the sorted/unsorted versions.
    differ = difflib.Differ()
    diff = differ.compare(names, names_sorted)
    return [
        output_api.PresubmitError(features_filename +
                                  ' features must be sorted alphabetically. '
                                  'Diff of feature order follows:',
                                  long_text='\n'.join(diff))
    ]


def _CheckRuntimeEnabledFile(file_name, input_api, output_api):
    """Checks common to both upload and commit."""
    # Read json5 using the JSON5 parser.
    features_filename = os.path.join(input_api.PresubmitLocalPath(), file_name)
    try:
        features = RuntimeEnabledFeatures(input_api, features_filename)
    except:
        return [
            output_api.PresubmitError(
                'Failed to parse {} for checks'.format(features_filename))
        ]

    results = []
    results.extend(
        _CheckRuntimeEnabledFeaturesSorted(features, features_filename,
                                           output_api))

    return results


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(
        _CheckRuntimeEnabledFile('runtime_enabled_features.json5', input_api,
                                 output_api))
    results.extend(
        _CheckRuntimeEnabledFile('runtime_enabled_features.override.json5',
                                 input_api, output_api))

    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
