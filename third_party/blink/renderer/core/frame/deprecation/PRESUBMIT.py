# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting deprecations.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import difflib
import os
import sys

USE_PYTHON3 = True


# pyright: reportMissingImports=false
def _LoadDeprecation(input_api, filename):
    """Returns the deprecations present in the specified JSON5 file."""

    # We need to wait until we have an input_api object and use this
    # roundabout construct to import json5 because this file is
    # eval-ed and thus doesn't have __file__.
    try:
        json5_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                            '..', '..', '..', '..', '..',
                                            'pyjson5', 'src')
        sys.path.append(json5_path)
        import json5
        return json5.load(open(filename, encoding='utf-8'))['data']
    finally:
        # Restore sys.path to what it was before.
        sys.path.remove(json5_path)


def _CheckDeprecation(input_api, output_api):
    """Check: deprecation.json5 is well formed.
    """
    # Read deprecation.json5 using the JSON5 parser.
    filename = os.path.join(input_api.PresubmitLocalPath(),
                            'deprecation.json5')
    deprecations = _LoadDeprecation(input_api, filename)

    # Parse deprecations for correctness.
    deprecation_names = []
    for deprecation in deprecations:
        if 'name' not in deprecation or not deprecation['name']:
            return [
                output_api.PresubmitError(
                    'deprecation.json5 items must all contain a non-empty "name" value.'
                )
            ]
        deprecation_names.append(deprecation['name'])
        if 'message' not in deprecation or not deprecation['message']:
            return [
                output_api.PresubmitError(
                    'deprecation.json5 items must all contain a non-empty "message" value.'
                )
            ]
        if 'translation_note' not in deprecation or not deprecation[
                'translation_note']:
            return [
                output_api.PresubmitError(
                    'deprecation.json5 items must all contain a non-empty "translation_note" value.'
                )
            ]
        if 'web_features' not in deprecation or not deprecation['web_features']:
            return [
                output_api.PresubmitError(
                    'deprecation.json5 items must all contain a non-empty list of "web_features".'
                )
            ]

    # Parse deprecations for ordering.
    deprecation_names_sorted = sorted(deprecation_names)
    if deprecation_names == deprecation_names_sorted:
        return []
    differ = difflib.Differ()
    diff = differ.compare(deprecation_names, deprecation_names_sorted)
    return [
        output_api.PresubmitError(
            'deprecation.json5 items must be sorted alphabetically. '
            'Diff of deprecation data order follows:',
            long_text='\n'.join(diff))
    ]


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(_CheckDeprecation(input_api, output_api))
    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
