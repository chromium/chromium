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
        import update_scheduler_enums  # pylint: disable=F0401
    finally:
        sys.path = original_sys_path

    source_path = ''
    for f in input_api.AffectedFiles():
        if f.LocalPath().endswith(
                input_api.os_path.basename(
                    update_scheduler_enums.SOUCRE_FILE)):
            source_path = f.LocalPath()
            break
    else:
        return []

    presubmit_error = update_histogram_enum.CheckPresubmitErrors(
        update_scheduler_enums.XML_FILE,
        histogram_enum_name=update_scheduler_enums.ENUM_NAME,
        update_script_name=update_scheduler_enums.SCRIPT,
        source_enum_path=update_scheduler_enums.SOUCRE_FILE,
        start_marker=update_scheduler_enums.START_MARKER,
        end_marker=update_scheduler_enums.END_MARKER,
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
