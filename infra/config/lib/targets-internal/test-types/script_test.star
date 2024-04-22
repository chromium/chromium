# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for script tests."""

load("../common.star", _targets_common = "common")

def _script_test_spec_init(node, settings):
    settings = settings  # Shut linter up
    return dict(
        name = node.key.id,
        script = node.props.details.script,
    )

_script_test_spec_handler = _targets_common.spec_handler(
    type_name = "script test",
    init = _script_test_spec_init,
    finalize = (lambda name, settings, spec_value: ("scripts", name, spec_value)),
)

def script_test(*, name, script):
    """Define a script test.

    A script test is a test that runs a python script wihin the
    //testing/scripts directory.

    Args:
        name: The name that can be used to refer to the test in other
            starlark declarations. The step name of the test will be
            based on this name (additional components may be added by
            the recipe or when generating a test with a variant).
        script: The name of the file within the //testing/scripts
            directory to run as the test.
    """
    _targets_common.create_legacy_test(
        name = name,
        basic_suite_test_config = _targets_common.basic_suite_test_config(
            script = script,
        ),
    )

    _targets_common.create_test(
        name = name,
        spec_handler = _script_test_spec_handler,
        details = struct(
            script = script,
        ),
    )
