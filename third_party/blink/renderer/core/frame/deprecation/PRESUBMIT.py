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


# Deprecations in this list have no `WebFeature`s to generate code from as they
# are not dispatched within the renderer. If this list starts to grow we should
# add more formal support.
EXEMPTED_FROM_RENDERER_GENERATION = {
    "PrivacySandboxExtensionsAPI": True,
    "ThirdPartyCookieAccessWarning": True,
    "ThirdPartyCookieAccessError": True,
}

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
        if len(deprecation['message']) != len(deprecation['message'].encode()):
            return [
                output_api.PresubmitError(
                    'deprecation.json5 items must all contain fully ascii "message" values.'
                )
            ]
        if 'translation_note' not in deprecation or not deprecation[
                'translation_note']:
            return [
                output_api.PresubmitError(
                    'deprecation.json5 items must all contain a non-empty '
                    '"translation_note" value.'
                )
            ]
        if len(deprecation['translation_note']) != len(
                deprecation['translation_note'].encode()):
            return [
                output_api.PresubmitError(
                    'deprecation.json5 items must all contain fully ascii '
                    '"translation_note" values.'
                )
            ]
        if 'web_features' in deprecation and deprecation['web_features']:
            sorted_web_features = sorted(deprecation['web_features'],
                                         key=lambda s: s.lower())
            if deprecation['web_features'] != sorted_web_features:
                differ = difflib.Differ()
                diff = differ.compare(deprecation['web_features'],
                                      sorted_web_features)
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items web_features must be sorted alphabetically. '
                        'Diff of web_features data order follows:',
                        long_text='\n'.join(diff))
                ]
        else:
            if deprecation['name'] not in EXEMPTED_FROM_RENDERER_GENERATION:
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items must all contain a non-empty '
                        'list of "web_features".'
                    )
                ]
        if 'chrome_status_feature' in deprecation:
            if not deprecation['chrome_status_feature']:
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items can omit chrome_status_feature,'
                        'but if included it must have a value.'
                    )
                ]
            if deprecation[
                    'chrome_status_feature'] < 1000000000000000 or deprecation[
                        'chrome_status_feature'] > 9999999999999999:
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items with a chrome_status_feature '
                        'must have one in the valid range.'
                    )
                ]
        if 'milestone' in deprecation:
            if not deprecation['milestone']:
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items can omit milestone, but if '
                        'included it must have a value.'
                    )
                ]
            if deprecation['milestone'] < 1 or deprecation['milestone'] > 1000:
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items with a milestone must have '
                        'one in the valid range.'
                    )
                ]
        if 'obsolete_to_be_removed_after_milestone' in deprecation:
            if not deprecation['obsolete_to_be_removed_after_milestone']:
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items can omit milestone, but if '
                        'included it must have a value.'
                    )
                ]
            if deprecation[
                    'obsolete_to_be_removed_after_milestone'] < 1 or deprecation[
                        'obsolete_to_be_removed_after_milestone'] > 1000:
                return [
                    output_api.PresubmitError(
                        'deprecation.json5 items with an '
                        'obsolete_to_be_removed_after_milestone must have a '
                        'milestone in the valid range.'
                    )
                ]

    # Parse deprecations for ordering.
    deprecation_names_sorted = sorted(deprecation_names,
                                      key=lambda s: s.lower())
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
