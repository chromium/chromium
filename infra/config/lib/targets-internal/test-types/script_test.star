# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implementation of tests for script tests."""

load("../common.star", _targets_common = "common")

def _script_test_spec_init(node, settings):
    if not settings.allow_script_tests:
        fail("script test being included by builder with allow_script_tests=False")
    return dict(
        name = node.key.id,
        script = node.props.details.script,
        args = list(node.props.details.args or []),
        precommit_args = list(node.props.details.precommit_args or []),
        non_precommit_args = list(node.props.details.non_precommit_args or []),
        resultdb = None,
    )

def _script_test_spec_finalize(_builder_name, test_name, _settings, spec_value):
    spec_value["resultdb"] = _targets_common.finalize_resultdb(spec_value["resultdb"])
    return "scripts", test_name, spec_value

_script_test_spec_handler = _targets_common.spec_handler(
    type_name = "script test",
    init = _script_test_spec_init,
    finalize = _script_test_spec_finalize,
)

def script_test(*, name, script, args = None, precommit_args = None, non_precommit_args = None):
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
            args = args,
            precommit_args = precommit_args,
            non_precommit_args = non_precommit_args,
        ),
    )

    _targets_common.create_test(
        name = name,
        spec_handler = _script_test_spec_handler,
        details = struct(
            script = script,
            args = args,
            precommit_args = precommit_args,
            non_precommit_args = non_precommit_args,
        ),
    )
