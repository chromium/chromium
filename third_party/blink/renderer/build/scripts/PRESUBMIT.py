# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _GenerateTestCommand(input_api, output_api, file_name, affected_list):
    if not input_api.AffectedFiles(
            file_filter=lambda x: input_api.FilterSourceFile(
                x, files_to_check=affected_list)):
        return None

    if input_api.is_committing:
        message_type = output_api.PresubmitError
    else:
        message_type = output_api.PresubmitPromptWarning

    test_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                       file_name)
    cmd = [input_api.python3_executable, test_path]

    # Adds paths for jinja2 and pyjson5
    env = input_api.environ.copy()
    import_path = [
        input_api.os_path.join(input_api.change.RepositoryRoot(),
                               'third_party'),
        input_api.os_path.join(input_api.change.RepositoryRoot(),
                               'third_party', 'pyjson5', 'src')
    ]
    if env.get('PYTHONPATH'):
        import_path.append(env.get('PYTHONPATH'))
    env['PYTHONPATH'] = input_api.os_path.pathsep.join(import_path)

    test_cmd = input_api.Command(
        name=file_name, cmd=cmd, kwargs={'env': env}, message=message_type)
    return test_cmd


def _RunTests(input_api, output_api):
    tests = [{
        'file_name': 'json5_generator_unittest.py',
        'affected_list': [r'.*json5_generator.*', r'.*\btests[\\\/].*']
    }, {
        'file_name': 'make_runtime_features_utilities_unittest.py',
        'affected_list': [r'.*make_runtime_features_utilities.*']
    }, {
        'file_name': 'make_document_policy_features_unittest.py',
        'affected_list': [r'.*make_document_policy_features.*']
    }, {
        'file_name': 'make_document_policy_features_tests.py',
        'affected_list': [r'.*make_document_policy_features.*']
    }, {
        'file_name':
        'make_permissions_policy_features_tests.py',
        'affected_list': [
            r'.*make_permissions_policy_features.*',
            '.*/templates/permissions_policy_features_generated.cc.tmpl',
        ]
    }]
    test_commands = []
    for test in tests:
        test_commands.append(
            _GenerateTestCommand(input_api, output_api, test['file_name'],
                                 test['affected_list']))
    return input_api.RunTests(
        [command for command in test_commands if command])


def CheckChangeOnUpload(input_api, output_api):
    return _RunTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunTests(input_api, output_api)
