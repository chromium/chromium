# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting Source/platform.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import difflib
import sys


# pyright: reportMissingImports=false
def RuntimeEnabledFeatures(input_api, filename):
    """Returns the features present in the specified features JSON5 file."""

    # We need to wait until we have an input_api object and use this
    # roundabout construct to import json5 because this file is
    # eval-ed and thus doesn't have __file__.
    json5_path = input_api.os_path.normpath(
        input_api.os_path.join(
            input_api.PresubmitLocalPath(), '..', '..', '..', 'pyjson5', 'src'
        )
    )
    path_added = False
    try:
        if json5_path not in sys.path:
            sys.path.insert(0, json5_path)
            path_added = True
        import json5

        with open(filename, encoding='utf-8') as f:
            return json5.load(f)['data']
    finally:
        # Restore sys.path to what it was before.
        if path_added and json5_path in sys.path:
            sys.path.remove(json5_path)


def _CheckRuntimeEnabledFeaturesSorted(features, features_filename, output_api):
    """Check: runtime_enabled_features.json5 feature list sorted alphabetically."""
    names = [feature['name'] for feature in features]

    # Sort the 'data' section by name.
    names_sorted = sorted(names, key=lambda s: s.lower())

    if names == names_sorted:
        return []

    # Diff the sorted/unsorted versions.
    differ = difflib.Differ()
    diff = differ.compare(names, names_sorted)
    return [
        output_api.PresubmitError(
            features_filename + ' features must be sorted alphabetically. '
            'Diff of feature order follows:',
            long_text='\n'.join(diff),
        )
    ]


def _IsFileAffected(file_name, input_api):
    """Returns True if the specified file was modified in this change."""
    target_path = input_api.os_path.normpath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), file_name)
    )
    return any(
        input_api.os_path.normpath(f.AbsoluteLocalPath()) == target_path
        for f in input_api.AffectedFiles(include_deletes=False)
    )


def _CheckRuntimeEnabledFile(file_name, input_api, output_api):
    """Checks that features in file_name are sorted alphabetically."""
    if not _IsFileAffected(file_name, input_api):
        return []

    features_filename = input_api.os_path.join(
        input_api.PresubmitLocalPath(), file_name
    )
    try:
        features = RuntimeEnabledFeatures(input_api, features_filename)
        return _CheckRuntimeEnabledFeaturesSorted(
            features, features_filename, output_api
        )
    except Exception as e:
        return [
            output_api.PresubmitError(
                f'Failed to parse or validate {features_filename} for checks: '
                f'{e}'
            )
        ]


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(
        _CheckRuntimeEnabledFile(
            'runtime_enabled_features.json5', input_api, output_api
        )
    )
    results.extend(
        _CheckRuntimeEnabledFile(
            'runtime_enabled_features.override.json5', input_api, output_api
        )
    )

    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
