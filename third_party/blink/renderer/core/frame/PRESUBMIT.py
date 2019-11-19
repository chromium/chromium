# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Blink frame presubmit script

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

CSS_PROPERTY_ID_HEADER_PATH = (
    'third_party/blink/public/mojom/use_counter/css_property_id.mojom')

def _RunUseCounterChecks(input_api, output_api):
    for f in input_api.AffectedFiles():
        if f.LocalPath().endswith('use_counter.cc'):
            use_counter_cpp_file = f
            break
    else:
        return []

    largest_found_bucket = 0
    expected_max_bucket = 0

    # Looking for a line like "case CSSPropertyID::kGrid: return 453;"
    bucket_finder = input_api.re.compile(
        r'case CSSPropertyID::k\w+:\s*return\s*(\d+);')

    # Looking for a line like "const int32 kMaximumCSSSampleId = 452;"
    expected_max_finder = input_api.re.compile(
        r'const int32 kMaximumCSSSampleId = (\d+);')

    for f in input_api.change.AffectedFiles():
        if f.AbsoluteLocalPath().endswith(CSS_PROPERTY_ID_HEADER_PATH):
            expected_max_match = expected_max_finder.search(
                '\n'.join(f.NewContents()))
            break
    else:
        return []

    if expected_max_match:
        expected_max_bucket = int(expected_max_match.group(1))

    for bucket_match in bucket_finder.finditer(
            '\n'.join(use_counter_cpp_file.NewContents())):

        bucket = int(bucket_match.group(1))
        largest_found_bucket = max(largest_found_bucket, bucket)

    if largest_found_bucket != expected_max_bucket:
        if input_api.is_committing:
            message_type = output_api.PresubmitError
        else:
            message_type = output_api.PresubmitPromptWarning

        return [message_type(
            'Largest found CSSProperty bucket Id (%d) does not match '
            'maximumCSSSampleId (%d)' % (
                largest_found_bucket, expected_max_bucket),
            items=[use_counter_cpp_file.LocalPath(),
                   CSS_PROPERTY_ID_HEADER_PATH])]

    return []


def CheckChangeOnUpload(input_api, output_api):
    results = []
    results.extend(_RunUseCounterChecks(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    results = []
    results.extend(_RunUseCounterChecks(input_api, output_api))
    return results
