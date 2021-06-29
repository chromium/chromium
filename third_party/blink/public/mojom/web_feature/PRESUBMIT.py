# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Blink frame presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""


def _RunUmaHistogramChecks(input_api, output_api):  # pylint: disable=C0103
    import sys

    original_sys_path = sys.path
    try:
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..',
                                   '..', '..', '..', 'tools', 'metrics',
                                   'histograms')
        ]
        import update_histogram_enum  # pylint: disable=F0401
    finally:
        sys.path = original_sys_path

    for f in input_api.AffectedFiles():
        if f.LocalPath().endswith('web_feature.mojom'):
            break
    else:
        return []

    source_path = 'third_party/blink/public/mojom/web_feature/web_feature.mojom'
    start_marker = '^enum WebFeature {'
    end_marker = '^kNumberOfFeatures'
    presubmit_error = update_histogram_enum.CheckPresubmitErrors(
        histogram_enum_name='FeatureObserver',
        update_script_name='update_use_counter_feature_enum.py',
        source_enum_path=source_path,
        start_marker=start_marker,
        end_marker=end_marker,
        strip_k_prefix=True)
    if presubmit_error:
        return [
            output_api.PresubmitPromptWarning(
                presubmit_error, items=[source_path])
        ]
    return []


def CheckChangeOnUpload(input_api, output_api):  # pylint: disable=C0103
    results = []
    results.extend(_RunUmaHistogramChecks(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):  # pylint: disable=C0103
    results = []
    results.extend(_RunUmaHistogramChecks(input_api, output_api))
    return results
