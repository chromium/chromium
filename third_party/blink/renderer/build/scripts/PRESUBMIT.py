# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def _GenerateTestCommand(input_api, output_api, file_name, white_list):
    if not input_api.AffectedFiles(file_filter=lambda x: input_api.FilterSourceFile(x, white_list=white_list)):
        return None

    if input_api.is_committing:
        message_type = output_api.PresubmitError
    else:
        message_type = output_api.PresubmitPromptWarning

    test_path = input_api.os_path.join(
        input_api.PresubmitLocalPath(), file_name)
    if input_api.is_windows:
        cmd = [input_api.python_executable, test_path]
    else:
        cmd = [test_path]

    test_cmd = input_api.Command(
        name=file_name,
        cmd=cmd,
        kwargs={},
        message=message_type)
    return test_cmd


def _RunTests(input_api, output_api):
    tests = [
        {'file_name': 'json5_generator_unittest.py', 'white_list': [r'.*json5_generator.*', r'.*\btests[\\\/].*']},
        {'file_name': 'make_runtime_features_utilities_unittest.py', 'white_list': [r'.*make_runtime_features_utilities.*']},
    ]
    test_commands = []
    for test in tests:
        test_commands.append(_GenerateTestCommand(input_api, output_api, test['file_name'], test['white_list']))
    return input_api.RunTests([command for command in test_commands if command])


def CheckChangeOnUpload(input_api, output_api):
    return _RunTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunTests(input_api, output_api)