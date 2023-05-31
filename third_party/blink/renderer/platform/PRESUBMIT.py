# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting Source/platform.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import difflib
import os
import re
import sys

RUNTIMEENABLED_NAME = re.compile(r'\s*name\s*:\s*"([^"]*)"')
ASH_STATUS = "ChromeOS_Ash"
LACROS_STATUS = "ChromeOS_Lacros"

# The ignore list will be removed once existing features adopt parity across
# Lacros and ChromeOS.
# TODO(erikchen): This list doesn't match what in the .json5 file.
ASH_LACROS_FEATURE_STATUS_PARITY_IGNORE_LIST = [
    'DigitalGoods',  # crbug.com/1235859
    'DocumentPictureInPictureAPI',  # crbug.com/1373334
    'NetInfoDownlinkMax',  # crbug.com/1235864
    'WebBluetooth',  # crbug.com/1235867
]


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


def RuntimeEnabledFeatureNames(filename):
    """Reads the 'name' of each feature in runtime_enabled_features.json5."""
    # Note: We don't have a JSON5 parser available, so just use a regex.
    with open(filename, encoding='utf-8') as f:
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


def _CheckChromeOSAshLacrosFeatureStatusParity(input_api, output_api):
    """Check: runtime_enabled_features.json5 feature status parity across
     ChromeOS Ash and ChromeOS Lacros.
    """

    filename = os.path.join(input_api.PresubmitLocalPath(),
                            'runtime_enabled_features.json5')
    try:
        features = RuntimeEnabledFeatures(input_api, filename)
        # Check that all features with a status specified for ChromeOS Ash have
        # the same status specified for ChromeOS Lacros.
        for feature in features:
            feature_name = feature['name']
            if feature_name in ASH_LACROS_FEATURE_STATUS_PARITY_IGNORE_LIST:
                continue
            if 'status' in feature and type(feature['status']) is dict:
                status_dict = feature['status']
                if (ASH_STATUS in status_dict or LACROS_STATUS in status_dict
                    ) and (status_dict.get(LACROS_STATUS) !=
                           status_dict.get(ASH_STATUS)):
                    return [
                        output_api.PresubmitError(
                            f'Feature {feature_name} does not have status '
                            'parity across ChromeOS Ash and ChromeOS Lacros.')
                    ]
    except:
        return [
            output_api.PresubmitError(
                'Failed to parse {} for checks'.format(filename))
        ]

    return []


def _CommonChecks(input_api, output_api):
    """Checks common to both upload and commit."""
    results = []
    results.extend(_CheckRuntimeEnabledFeaturesSorted(input_api, output_api))
    results.extend(
        _CheckChromeOSAshLacrosFeatureStatusParity(input_api, output_api))

    return results


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
