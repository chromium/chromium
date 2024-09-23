# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/extensions/browser.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import sys

PRESUBMIT_VERSION = '2.0.0'


def _CreateHistogramValueChecker(input_api, output_api, path):
  original_sys_path = sys.path

  try:
    sys.path.append(
        input_api.os_path.join(
            input_api.PresubmitLocalPath(),
            '..',
            '..',
            'tools',
            'strict_enum_value_checker',
        )
    )
    from strict_enum_value_checker import StrictEnumValueChecker
  finally:
    sys.path = original_sys_path

  return StrictEnumValueChecker(
      input_api,
      output_api,
      start_marker='enum HistogramValue {',
      end_marker='  // Last entry:',
      path=path,
  )


def CheckHistogramValuesUpload(input_api, output_api):
  results = []
  histogram_paths = (
      'extensions/browser/extension_event_histogram_value.h',
      'extensions/browser/extension_function_histogram_value.h',
  )
  for path in histogram_paths:
    results += _CreateHistogramValueChecker(input_api, output_api, path).Run()
  return results


def CheckHistogramsUpload(input_api, output_api):
  try:
    # Setup sys.path so that we can call histograms code.
    import sys

    original_sys_path = sys.path
    sys.path = sys.path + [
        input_api.os_path.join(
            input_api.change.RepositoryRoot(), 'tools', 'metrics', 'histograms'
        )
    ]

    import presubmit_bad_message_reasons

    return presubmit_bad_message_reasons.PrecheckBadMessage(
        input_api, output_api, 'BadMessageReasonExtensions'
    )
  except:
    return [output_api.PresubmitError('Could not verify histogram!')]
  finally:
    sys.path = original_sys_path
