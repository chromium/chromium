# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining targets that the chromium family of recipes can build/test."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./nodes.star", "nodes")

_TARGET = nodes.create_unscoped_node_type("target")

def _create_target(
        *,
        name,
        type,
        label,
        label_type = None,
        executable = None,
        executable_suffix = None,
        script = None,
        skip_usage_check = False,
        args = None):
    target_key = _TARGET.add(name, props = dict(
        type = type,
        label = label,
        label_type = label_type,
        executable = executable,
        executable_suffix = executable_suffix,
        script = script,
        skip_usage_check = skip_usage_check,
        args = args,
    ))
    graph.add_edge(keys.project(), target_key)

def _compile_target(*, name, label, skip_usage_check = False):
    """Define a compile target to use in targets specs.

    A compile target provides a mapping to any ninja target that will
    only be built, not executed.

    Args:
        name: The name that can be used to refer to the target.
        label: The GN label for the ninja target.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
    """
    _create_target(
        name = name,
        type = "additional_compile_target",
        label = label,
        skip_usage_check = skip_usage_check,
    )

def _console_test_launcher(
        *,
        name,
        label,
        label_type = None,
        skip_usage_check = False,
        args = None):
    """Define a console test launcher target to use in targets specs.

    A console test launcher is a gtest-based test that uses the
    parallelizing TestLauncher from //base/test:test_support but does
    not need Xvfb.

    Args:
        name: The name that can be used to refer to the target.
        label: The GN label for the ninja target.
        label_type: The type of the label. This is used by MB to find
            the generated runtime files in the correct place if the
            target uses the test_launcher command-line conventions but
            the label refers to a different type of target.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
        args: The arguments to the test. These arguments will be
            included when the test is run using "mb try"
    """
    _create_target(
        name = name,
        type = "console_test_launcher",
        label = label,
        label_type = label_type,
        skip_usage_check = skip_usage_check,
        args = args,
    )

def _generated_script(*, name, label, skip_usage_check = False, args = None):
    """Define a generated script target to use in targets specs.

    A generated script target is a test that is executed via a script
    generated at build time. The script must be in
    output_dir/bin/run_$target (or output_dir\bin\run_$target.bat on
    Windows).

    Args:
        name: The name that can be used to refer to the target.
        label: The GN label for the ninja target.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
        args: The arguments to the test. These arguments will be
            included when the test is run using "mb try"
    """
    _create_target(
        name = name,
        type = "generated_script",
        label = label,
        skip_usage_check = skip_usage_check,
        args = args,
    )

def _junit_test(*, name, label, skip_usage_check = False):
    """Define a junit test target to use in targets specs.

    A junit test target is a test using the JUnit test framework.

    Args:
        name: The name that can be used to refer to the target.
        label: The GN label for the ninja target.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
    """
    _create_target(
        name = name,
        type = "junit_test",
        label = label,
        skip_usage_check = skip_usage_check,
    )

def _script(*, name, label, script, skip_usage_check = False, args = None):
    """Define a script target to use in targets specs.

    A script target is a test that is executed via a python script.

    Args:
        name: The name that can be used to refer to the target.
        label: The GN label for the ninja target.
        script: The GN path (e.g. //testing/scripts/foo.py" to the python
            script to run.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
        args: The arguments to the test. These arguments will be
            included when the test is run using "mb try"
    """
    _create_target(
        name = name,
        type = "script",
        label = label,
        script = script,
        skip_usage_check = skip_usage_check,
        args = args,
    )

def _windowed_test_launcher(
        *,
        name,
        label,
        label_type = None,
        executable = None,
        executable_suffix = None,
        skip_usage_check = False,
        args = None):
    """Define a windowed test launcher target to use in targets specs.

    A windowed test launcher is a gtest-based test that uses the
    parallelizing TestLauncher from //base/test:test_support and needs
    to run under Xvfb if run on some platforms (eg. Linux Desktop and
    Ozone CrOS).

    Args:
        name: The name that can be used to refer to the target.
        label: The GN label for the ninja target.
        label_type: The type of the label. This is used by MB to find
            the generated runtime files in the correct place if the
            target uses the test_launcher command-line conventions but
            the label refers to a different type of target.
        executable: The binary to run. By default, the ninja target name
            will be used. On Windows, .exe will be appended, so it
            should not appear in the executable name.
        executable_suffix: The suffix to append to the executable name.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
        args: The arguments to the test. These arguments will be
            included when the test is run using "mb try"
    """
    _create_target(
        name = name,
        type = "windowed_test_launcher",
        label = label,
        label_type = label_type,
        executable = executable,
        executable_suffix = executable_suffix,
        skip_usage_check = skip_usage_check,
        args = args,
    )

targets = struct(
    compile_target = _compile_target,
    console_test_launcher = _console_test_launcher,
    generated_script = _generated_script,
    junit_test = _junit_test,
    script = _script,
    windowed_test_launcher = _windowed_test_launcher,
)

GN_ISOLATE_MAP_PYL = """\
# THIS IS A GENERATED FILE DO NOT EDIT!!!
# Instead:
# 1. Modify //infra/config/targets/targets.star
# 2. Run //infra/config/main.star
# 3. Run //infra/config/scripts/sync-isolate-map.py

{{
{entries}
}}
"""

def _generate_gn_isolate_map_pyl(ctx):
    entries = []
    for n in graph.children(keys.project(), _TARGET.kind, graph.DEFINITION_ORDER):
        entries.append('  "{}": {{'.format(n.key.id))
        entries.append('    "label": "{}",'.format(n.props.label))
        if n.props.label_type != None:
            entries.append('    "label_type": "{}",'.format(n.props.label_type))
        entries.append('    "type": "{}",'.format(n.props.type))
        if n.props.executable != None:
            entries.append('    "executable": "{}",'.format(n.props.executable))
        if n.props.executable_suffix != None:
            entries.append('    "executable_suffix": "{}",'.format(n.props.executable_suffix))
        if n.props.script != None:
            entries.append('    "script": "{}",'.format(n.props.script))
        if n.props.skip_usage_check:
            entries.append('    "skip_usage_check": {},'.format(n.props.skip_usage_check))
        if n.props.args:
            entries.append('    "args": [')
            for a in n.props.args:
                entries.append('      "{}",'.format(a))
            entries.append("    ],")
        entries.append("  },")
    ctx.output["testing/gn_isolate_map.pyl"] = GN_ISOLATE_MAP_PYL.format(entries = "\n".join(entries))

lucicfg.generator(_generate_gn_isolate_map_pyl)
