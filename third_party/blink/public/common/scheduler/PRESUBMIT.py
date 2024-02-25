# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Blink feature-policy presubmit script.

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

    source_path = ''
    for f in input_api.AffectedFiles():
        if f.LocalPath().endswith('web_scheduler_tracked_feature.h'):
            source_path = f.LocalPath()
            break
    else:
        return []

    start_marker = '^enum class WebSchedulerTrackedFeature {'
    end_marker = '^kMaxValue'
    presubmit_error = update_histogram_enum.CheckPresubmitErrors(
        'tools/metrics/histograms/metadata/navigation/enums.xml',
        histogram_enum_name='WebSchedulerTrackedFeature',
        update_script_name='update_scheduler_enums.py',
        source_enum_path=source_path,
        start_marker=start_marker,
        end_marker=end_marker,
        strip_k_prefix=True)
    if presubmit_error:
        return [
            output_api.PresubmitError(presubmit_error, items=[source_path])
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
