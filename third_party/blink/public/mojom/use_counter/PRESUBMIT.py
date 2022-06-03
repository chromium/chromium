# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Blink frame presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.

This script generates enums.xml based on the css_property_id.mojom and verifies
that it is equivalent with the expected file in the patch otherwise warns the
user.
"""

import sys


USE_PYTHON3 = True


def _RunUmaHistogramChecks(input_api, output_api):
    original_sys_path = sys.path
    try:
        sys.path = sys.path + [
            input_api.os_path.join(input_api.PresubmitLocalPath(), '..', '..',
                                   '..', '..', '..', 'tools', 'metrics',
                                   'histograms')
        ]
        import update_histogram_enum  # pylint: disable=F0401
        import update_use_counter_css  # pylint: disable=F0401

    finally:
        sys.path = original_sys_path

    source_path = 'third_party/blink/public/mojom/use_counter/'\
                  'css_property_id.mojom'

    if not source_path in [f.LocalPath() for f in input_api.AffectedFiles()]:
        return []

    # Note: This is similar to update_histogram_enum.CheckPresubmitErrors except
    # that we use the logic in update_use_counter_css to produce the enum
    # values.

    histogram_enum_name = 'MappedCSSProperties'
    update_script_name = 'update_use_counter_css.py'

    def read_values(source_path, start_marker, end_marker, strip_k_prefix):
        return update_use_counter_css.ReadCssProperties(source_path)

    error = update_histogram_enum.CheckPresubmitErrors(
        histogram_enum_name,
        update_script_name,
        source_path,
        '',
        '',
        histogram_value_reader=read_values)

    if error:
        return [output_api.PresubmitPromptWarning(error, items=[source_path])]
    return []


def CheckChangeOnUpload(input_api, output_api):
    return _RunUmaHistogramChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunUmaHistogramChecks(input_api, output_api)
