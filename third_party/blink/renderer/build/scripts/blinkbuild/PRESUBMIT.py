# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

def _RunBindingsTests(input_api, output_api):
    # The presubmit tests are all run together with the module scheme set to
    # "flat" at the recipe level. But these presubmit tests run through
    # a test runner that parses the test as a "pyunit" test. The environment
    # variable forces the test runner to parse the tests as a flat test,
    # otherwise resultdb will give an error saying a field in the "flat" type
    # is unexpectedly not empty.
    os.environ['RESULTDB_MODULE_SCHEME'] = 'flat'

    pardir = input_api.os_path.pardir
    cmd_name = 'run_bindings_tests.py'
    run_bindings_tests_path = input_api.os_path.join(
        input_api.PresubmitLocalPath(), *([pardir] * 4 + ['tools', cmd_name]))
    cmd = [input_api.python3_executable, run_bindings_tests_path]
    if input_api.verbose:
        print('Running ' + cmd_name)
    test_cmd = input_api.Command(
        name=cmd_name, cmd=cmd, kwargs={}, message=output_api.PresubmitError)
    return input_api.RunTests([test_cmd])


def CheckChangeOnUpload(input_api, output_api):
    return _RunBindingsTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _RunBindingsTests(input_api, output_api)
