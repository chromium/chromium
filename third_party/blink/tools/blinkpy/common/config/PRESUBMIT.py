# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""config/ presubmit script for Blink.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

PRESUBMIT_VERSION = '2.0.0'

def CheckEnsureSpecifier(input_api, output_api):
    """Ensure the first specifiers for the builders are valid.
    """
    this_dir = input_api.PresubmitLocalPath()
    builders_json_file = input_api.os_path.join(this_dir,
                                                "builders.json")
    generic_test_expectation = input_api.os_path.join(this_dir,
        '..', '..', '..', '..', 'web_tests', 'TestExpectations')
    if builders_json_file in input_api.AbsoluteLocalPaths():
        with open(generic_test_expectation, encoding='utf-8') as f:
            tags = f.readline().rstrip()
        with open(builders_json_file, encoding='utf-8') as f:
            builders = input_api.json.load(f)
            for key, value in builders.items():
                tag = value["specifiers"][0]
                if tag == "Android":
                    continue
                if tag not in tags:
                    error_message = (
                        'This CL updates builders.json, but the specifier %s '
                        'is not a valid tag in TestExpectations' % tag)
                    if input_api.is_committing:
                        return [output_api.PresubmitError(error_message)]
                    else:
                        return [output_api.PresubmitPromptWarning(error_message)]
    return []
