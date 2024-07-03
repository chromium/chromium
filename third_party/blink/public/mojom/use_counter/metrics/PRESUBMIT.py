# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Blink presubmit script for use counter metrics enums.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
for more details about the presubmit API built into gcl.
"""

PRESUBMIT_VERSION = '2.0.0'


def CheckHistograms(input_api, output_api):  # pylint: disable=C0103
    import sys

    original_sys_path = sys.path.copy()
    try:
        sys.path.append(
            input_api.os_path.join(input_api.change.RepositoryRoot(), 'tools',
                                   'metrics', 'histograms'))
        import update_histogram_enum  # pylint: disable=F0401
        import update_use_counter_css  # pylint: disable=F0401
    finally:
        sys.path = original_sys_path

    def _CssPropertyValueReader(source_path, start_marker, end_marker,
                                strip_k_prefix):
        return update_use_counter_css.ReadCssProperties(source_path)

    _VALIDATE_HISTOGRAM_ARGS = {
        'third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom':
        {
            'update_script_name': 'update_use_counter_feature_enum.py',
            'histogram_enum_name': 'WebDXFeatureObserver',
            'start_marker': '^enum WebDXFeature {',
            'end_marker': '^kNumberOfFeatures',
            'strip_k_prefix': True,
        },
        'third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom':
        {
            'update_script_name': 'update_use_counter_feature_enum.py',
            'histogram_enum_name': 'FeatureObserver',
            'start_marker': '^enum WebFeature {',
            'end_marker': '^kNumberOfFeatures',
            'strip_k_prefix': True,
        },
        'third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom':
        {
            'update_script_name': 'update_use_counter_css.py',
            'histogram_enum_name': 'MappedCSSProperties',
            'start_marker': '',
            'end_marker': '',
            'histogram_value_reader': _CssPropertyValueReader,
        },
    }

    results = []
    for f in input_api.AffectedFiles():
        if f.LocalPath() not in _VALIDATE_HISTOGRAM_ARGS:
            continue
        presubmit_error = update_histogram_enum.CheckPresubmitErrors(
            'tools/metrics/histograms/enums.xml',
            source_enum_path=f.LocalPath(),
            **_VALIDATE_HISTOGRAM_ARGS[f.LocalPath()])
        if presubmit_error:
            results.append(
                output_api.PresubmitError(presubmit_error,
                                          items=[f.LocalPath()]))
    return results
