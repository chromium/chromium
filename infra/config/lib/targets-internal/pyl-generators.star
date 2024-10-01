# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generators that generate the testing/buildbot .pyl files.

The gn_isolate_map.pyl, test_suites.pyl, mixins.pyl and variants.pyl that are
used by //testing/buildbot/generate_buildbot_json.py by default are generated
from definitions in starlark. This allows those definitions to be used by both
builders that set their tests in //testing/buildbot/waterfalls.pyl and builders
that set their tests in starlark.

The remaining pyl files (waterfalls.pyl and test_suite_exceptions.pyl) define
the tests per builder and don't contain any declarations that can be reused, so
they are still purely hand-written. As builders are migrated to setting their
tests in starlark, entries for them should be removed from waterfalls.pyl and
test_suite_exceptions.pyl.

The angle repo reuses the definitions in those .pyl files by exporting
//testing/buildbot to a subtree repo that the angle repo DEPS in. This means
that the generated files actually have to exist in //testing/buildbot, which
requires a manual sync step (see //infra/config/scripts/sync-pyl-files.py).
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./common.star", _targets_common = "common")
load("./nodes.star", _targets_nodes = "nodes")

_PYL_HEADER_FMT = """\
# THIS IS A GENERATED FILE DO NOT EDIT!!!
# Instead:
# 1. Modify {star_file}
# 2. Run //infra/config/main.star
{extra_comments}
{{
{entries}
}}
"""

def _generate_gn_isolate_map_pyl(ctx):
    entries = []
    for n in graph.children(keys.project(), _targets_nodes.LABEL_MAPPING.kind, graph.KEY_ORDER):
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
    ctx.output["testing/gn_isolate_map.pyl"] = _PYL_HEADER_FMT.format(
        star_file = "//infra/config/targets/binaries.star and/or //infra/config/targets/tests.star (for tests defined using targets.tests.junit_test)",
        extra_comments = "",
        entries = "\n".join(entries),
    )

def _formatter(*, indent_level = 1, indent_size = 2):
    state = dict(
        lines = [],
        indent = indent_level * indent_size,
    )

    def add_line(s):
        if s:
            state["lines"].append(" " * state["indent"] + s)
        else:
            state["lines"].append("")

    def open_scope(s):
        add_line(s)
        state["indent"] += indent_size

    def close_scope(s):
        state["indent"] -= indent_size
        add_line(s)

    def lines():
        return list(state["lines"])

    def output():
        return "\n".join(state["lines"])

    return struct(
        add_line = add_line,
        open_scope = open_scope,
        close_scope = close_scope,
        lines = lines,
        output = output,
    )

def _generate_swarming_values(formatter, swarming):
    """Generate the pyl definitions for swarming fields.

    Swarming fields are the fields contained in values for the swarming,
    android_swarming and chromeos_swarming fields in mixins/variants/tests.

    Args:
        formatter: The formatter object used for generating indented output.
        swarming: The swarming value to generate the fields for.
    """

    def dimension_value(x):
        if x == None:
            return x
        return "'{}'".format(x)

    if swarming.enable != None:
        formatter.add_line("'can_use_on_swarming_builders': {},".format(swarming.enable))
    if swarming.shards:
        formatter.add_line("'shards': {},".format(swarming.shards))
    if swarming.dimensions:
        formatter.open_scope("'dimensions': {")
        for dim, value in swarming.dimensions.items():
            formatter.add_line("'{}': {},".format(dim, dimension_value(value)))
        formatter.close_scope("},")
    if swarming.optional_dimensions:
        formatter.open_scope("'optional_dimensions': {")
        for timeout, dimensions in swarming.optional_dimensions.items():
            formatter.open_scope("'{}': {{".format(timeout))
            for dim, value in dimensions.items():
                formatter.add_line("'{}': {},".format(dim, dimension_value(value)))
            formatter.close_scope("},")
        formatter.close_scope("},")
    if swarming.containment_type:
        formatter.add_line("'containment_type': '{}',".format(swarming.containment_type))
    if swarming.cipd_packages:
        formatter.open_scope("'cipd_packages': [")
        for package in swarming.cipd_packages:
            formatter.open_scope("{")
            formatter.add_line("'cipd_package': '{}',".format(package.package))
            formatter.add_line("'location': '{}',".format(package.location))
            formatter.add_line("'revision': '{}',".format(package.revision))
            formatter.close_scope("},")
        formatter.close_scope("],")
    if swarming.expiration_sec:
        formatter.add_line("'expiration': {},".format(swarming.expiration_sec))
    if swarming.hard_timeout_sec:
        formatter.add_line("'hard_timeout': {},".format(swarming.hard_timeout_sec))
    if swarming.io_timeout_sec:
        formatter.add_line("'io_timeout': {},".format(swarming.io_timeout_sec))
    if swarming.idempotent != None:
        formatter.add_line("'idempotent': {},".format(swarming.idempotent))
    if swarming.named_caches:
        formatter.open_scope("'named_caches': [")
        for cache in swarming.named_caches:
            formatter.open_scope("{")
            formatter.add_line("'name': '{}',".format(cache.name))
            formatter.add_line("'path': '{}',".format(cache.path))
            formatter.close_scope("},")
        formatter.close_scope("],")
    if swarming.service_account:
        formatter.add_line("'service_account': '{}',".format(swarming.service_account))

def _generate_mixin_values(formatter, mixin, generate_skylab_container = False):
    """Generate the pyl definitions for mixin fields.

    Mixin fields are fields that are common to mixins, variants and test
    definitions within basic suites.

    Args:
        formatter: The formatter object used for generating indented output.
        mixin: Dict containing the mixin values to output.
        generate_skylab_container: Whether or not to generate the skylab key to
            contain the fields of the skylab value. Mixins and the generated
            test have those fields at top-level, but variants have them under a
            skylab key.
    """
    if "description" in mixin:
        formatter.add_line("'description': '{}',".format(mixin["description"]))

    for args_attr in (
        "args",
        "precommit_args",
        "non_precommit_args",
        "android_args",
        "chromeos_args",
        "desktop_args",
        "lacros_args",
        "linux_args",
        "mac_args",
        "win_args",
        "win64_args",
    ):
        if args_attr in mixin:
            formatter.open_scope("'{}': [".format(args_attr))
            for a in mixin[args_attr]:
                if type(a) == type(struct()):
                    a = a.pyl_arg_value
                formatter.add_line("'{}',".format(a))
            formatter.close_scope("],")

    if "check_flakiness_for_new_tests" in mixin:
        formatter.add_line("'check_flakiness_for_new_tests': {},".format(mixin["check_flakiness_for_new_tests"]))

    if "ci_only" in mixin:
        formatter.add_line("'ci_only': {},".format(mixin["ci_only"]))

    if "isolate_profile_data" in mixin:
        formatter.add_line("'isolate_profile_data': {},".format(mixin["isolate_profile_data"]))

    if "timeout_sec" in mixin:
        formatter.add_line("'timeout_sec': {},".format(mixin["timeout_sec"]))

    for swarming_attr in ("swarming", "android_swarming", "chromeos_swarming"):
        if swarming_attr in mixin:
            swarming = mixin[swarming_attr]
            formatter.open_scope("'{}': {{".format(swarming_attr))
            _generate_swarming_values(formatter, swarming)
            formatter.close_scope("},")

    if "merge" in mixin:
        merge = mixin["merge"]
        formatter.open_scope("'merge': {")
        formatter.add_line("'script': '{}',".format(merge.script))
        if merge.args:
            formatter.open_scope("'args': [")
            for a in merge.args:
                formatter.add_line("'{}',".format(a))
            formatter.close_scope("],")
        formatter.close_scope("},")

    if "skylab" in mixin:
        skylab = mixin["skylab"]
        if generate_skylab_container:
            formatter.open_scope("'skylab': {")
        if skylab.cros_board:
            formatter.add_line("'cros_board': '{}',".format(skylab.cros_board))
        if skylab.cros_build_target:
            formatter.add_line("'cros_build_target': '{}',".format(skylab.cros_build_target))
        if skylab.cros_model:
            formatter.add_line("'cros_model': '{}',".format(skylab.cros_model))
        if skylab.cros_cbx:
            formatter.add_line("'cros_cbx': True,")
        if skylab.cros_img:
            formatter.add_line("'cros_img': '{}',".format(skylab.cros_img))
        if skylab.use_lkgm:
            formatter.add_line("'use_lkgm': True,")
        if skylab.autotest_name:
            formatter.add_line("'autotest_name': '{}',".format(skylab.autotest_name))
        if skylab.bucket:
            formatter.add_line("'bucket': '{}',".format(skylab.bucket))
        if skylab.dut_pool:
            formatter.add_line("'dut_pool': '{}',".format(skylab.dut_pool))
        if skylab.public_builder:
            formatter.add_line("'public_builder': '{}',".format(skylab.public_builder))
        if skylab.public_builder_bucket:
            formatter.add_line("'public_builder_bucket': '{}',".format(skylab.public_builder_bucket))
        if skylab.shards:
            formatter.add_line("'shards': {},".format(skylab.shards))
        if skylab.run_cft:
            formatter.add_line("'run_cft': {},".format(skylab.run_cft))
        if skylab.args:
            formatter.add_line("'args': {},".format(skylab.args))
        if generate_skylab_container:
            formatter.close_scope("},")

    if "resultdb" in mixin:
        resultdb = mixin["resultdb"]
        formatter.open_scope("'resultdb': {")
        if resultdb.enable:
            formatter.add_line("'enable': True,")
        if resultdb.has_native_resultdb_integration:
            formatter.add_line("'has_native_resultdb_integration': True,")
        if resultdb.result_format != None:
            formatter.add_line("'result_format': '{}',".format(resultdb.result_format))
        if resultdb.result_file != None:
            formatter.add_line("'result_file': '{}',".format(resultdb.result_file))
        if resultdb.inv_extended_properties_dir != None:
            formatter.add_line("'inv_extended_properties_dir': '{}',".format(resultdb.inv_extended_properties_dir))
        formatter.close_scope("},")

    if "use_isolated_scripts_api" in mixin:
        formatter.add_line("'use_isolated_scripts_api': {},".format(mixin["use_isolated_scripts_api"]))

    if "shards" in mixin:
        formatter.add_line("'shards': {},".format(mixin["shards"]))

    if "experiment_percentage" in mixin:
        formatter.add_line("'experiment_percentage': {},".format(mixin["experiment_percentage"]))

def _generate_mixins_pyl(ctx):
    formatter = _formatter()

    for n in graph.children(keys.project(), _targets_nodes.MIXIN.kind, graph.KEY_ORDER):
        mixin = n.props.mixin_values
        formatter.open_scope("'{}': {{".format(n.key.id))
        if not n.props.pyl_fail_if_unused:
            formatter.add_line("'fail_if_unused': False,")

        _generate_mixin_values(formatter, mixin)

        formatter.close_scope("},")

    ctx.output["testing/mixins.pyl"] = _PYL_HEADER_FMT.format(
        star_file = "//infra/config/targets/mixins.star",
        extra_comments = "\n".join([
            "",
            "# The copy of this file in //testing/buildbot is not read by generate_buildbot_json.py,",
            "# but must be present for downstream uses. It can be kept in sync by running",
            "# //infra/config/scripts/sync-pyl-files.py.",
            "",
        ]),
        entries = formatter.output(),
    )

def _generate_variants_pyl(ctx):
    formatter = _formatter()

    for n in graph.children(keys.project(), _targets_nodes.VARIANT.kind, graph.KEY_ORDER):
        mixin = n.props.mixin_values
        formatter.open_scope("'{}': {{".format(n.key.id))

        formatter.add_line("'identifier': '{}',".format(n.props.identifier))

        if not n.props.enabled:
            formatter.add_line("'enabled': {},".format(n.props.enabled))

        _generate_mixin_values(formatter, mixin, generate_skylab_container = True)

        mixins = []

        # The order that mixins are declared is significant,
        # DEFINITION_ORDER preserves the order that the edges were added
        # from the parent to the child
        for mixin in graph.children(n.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
            mixins.append(mixin.key.id)
        if mixins:
            formatter.open_scope("'mixins': [")
            for m in mixins:
                formatter.add_line("'{}',".format(m))
            formatter.close_scope("],")

        formatter.close_scope("},")

    ctx.output["testing/variants.pyl"] = _PYL_HEADER_FMT.format(
        star_file = "//infra/config/targets/variants.star",
        extra_comments = "",
        entries = formatter.output(),
    )

def _generate_test_suites_pyl(ctx):
    formatter = _formatter()

    # Some tests indicate mixins to remove (sizes tests check the sizes of
    # binaries rather than running them, so they should always run on linux
    # machines). As builders are migrated, some of the mixins will be switched
    # to not generate pyl entries since they would cause an error for not being
    # referenced. However, if the mixins don't exist then an error will be
    # raised if they are present in remove_mixins. To avoid the error while
    # still preserving the intention of removing them in case modifications are
    # made to the configuration that require them to be re-added to mixins.pyl,
    # we won't generate remove_mixins lines for mixins that aren't being
    # generated. We don't have to worry about some non-existent mixin being
    # referenced in the starlark because edges are added for each element in
    # remove_mixins.
    generated_mixins = set(graph.children(keys.project(), _targets_nodes.MIXIN.kind))

    formatter.open_scope("'basic_suites': {")

    for suite in graph.children(keys.project(), _targets_nodes.LEGACY_BASIC_SUITE.kind, graph.KEY_ORDER):
        formatter.add_line("")
        formatter.open_scope("'{}': {{".format(suite.key.id))

        for test_config_node in graph.children(suite.key, _targets_nodes.LEGACY_BASIC_SUITE_CONFIG.kind, graph.KEY_ORDER):
            test_name = test_config_node.key.id
            suite_test_config = test_config_node.props.config

            test_nodes = graph.children(test_config_node.key, _targets_nodes.LEGACY_TEST.kind)
            if len(test_nodes) != 1:
                fail("internal error: test config {} should have exactly 1 test: {}", test_config_node, test_nodes)
            test_node = test_nodes[0]
            target_test_config = test_node.props.basic_suite_test_config

            binary_nodes = graph.children(test_node.key, _targets_nodes.BINARY.kind)
            if len(binary_nodes) > 1:
                fail("internal error: test {} has more than 1 binary: {}", test_node, binary_nodes)
            binary_test_config = None
            if binary_nodes:
                binary_test_config = binary_nodes[0].props.test_config
            binary_test_config = binary_test_config or _targets_common.binary_test_config()

            # Generate the test common dict, which contains fields that can be
            # specified at both the test/binary level and at the suite level.
            # This allows for applying test-specific args and mixins before
            # applying suite-specific args and mixins.
            test_common_formatter = _formatter(indent_level = 0)

            # The order that mixins are declared is significant,
            # DEFINITION_ORDER preserves the order that the edges were added
            # from the parent to the child
            test_common_mixins = graph.children(test_node.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER)
            if test_common_mixins:
                test_common_formatter.open_scope("'mixins': [")
                for m in test_common_mixins:
                    test_common_formatter.add_line("'{}',".format(m.key.id))
                test_common_formatter.close_scope("],")

            test_common_mixin_values = {}

            for a in ("args", "precommit_args", "non_precommit_args"):
                test_common_args = getattr(target_test_config, a)
                if test_common_args:
                    test_common_mixin_values[a] = test_common_args

            for a in ("merge", "resultdb"):
                value = getattr(binary_test_config, a)
                if value:
                    test_common_mixin_values[a] = value

            _generate_mixin_values(test_common_formatter, test_common_mixin_values)

            test_formatter = _formatter(indent_level = 0)

            test_common_lines = test_common_formatter.lines()
            if test_common_lines:
                test_formatter.open_scope("'test_common': {")
                for l in test_common_lines:
                    test_formatter.add_line(l)
                test_formatter.close_scope("},")

            # Generate the other top-level fields in the test dict
            if target_test_config.script:
                test_formatter.add_line("'script': '{}',".format(target_test_config.script))

            # This is intentionally transforming binary -> test to remain
            # backwards-compatible with //testing/buildbot
            if target_test_config.binary:
                test_formatter.add_line("'test': '{}',".format(target_test_config.binary))
            if binary_test_config.results_handler:
                test_formatter.add_line("'results_handler': '{}',".format(binary_test_config.results_handler))

            if target_test_config.telemetry_test_name:
                test_formatter.add_line("'telemetry_test_name': '{}',".format(target_test_config.telemetry_test_name))

            if suite_test_config.tast_expr:
                test_formatter.add_line("'tast_expr': '{}',".format(suite_test_config.tast_expr))
            if suite_test_config.test_level_retries:
                test_formatter.add_line("'test_level_retries': {},".format(suite_test_config.test_level_retries))

            # The order that mixins are declared is significant,
            # DEFINITION_ORDER preserves the order that the edges were added
            # from the parent to the child
            test_config_mixins = graph.children(test_config_node.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER)
            if test_config_mixins:
                test_formatter.open_scope("'mixins': [")
                for m in test_config_mixins:
                    test_formatter.add_line("'{}',".format(m.key.id))
                test_formatter.close_scope("],")

            remove_mixins = [
                n.key.id
                for n in _targets_nodes.LEGACY_BASIC_SUITE_REMOVE_MIXIN.children(test_config_node.key)
                if n in generated_mixins
            ]
            if remove_mixins:
                test_formatter.open_scope("'remove_mixins': [")
                for m in remove_mixins:
                    test_formatter.add_line("'{}',".format(m))
                test_formatter.close_scope("],")

            _generate_mixin_values(test_formatter, suite_test_config.mixin_values or {})

            # Generate the test definition using the generated fields, if any
            test_lines = test_formatter.lines()
            if test_lines:
                formatter.open_scope("'{}': {{".format(test_name))
                for l in test_lines:
                    formatter.add_line(l)
                formatter.close_scope("},")
            else:
                formatter.add_line("'{}': {{}},".format(test_name))

        formatter.close_scope("},")

    formatter.close_scope("},")

    formatter.add_line("")

    formatter.open_scope("'compound_suites': {")

    for suite in graph.children(keys.project(), _targets_nodes.LEGACY_COMPOUND_SUITE.kind, graph.KEY_ORDER):
        formatter.add_line("")
        formatter.open_scope("'{}': [".format(suite.key.id))
        for basic_suite in graph.children(suite.key, _targets_nodes.LEGACY_BASIC_SUITE.kind, graph.KEY_ORDER):
            formatter.add_line("'{}',".format(basic_suite.key.id))
        formatter.close_scope("],")

    formatter.close_scope("},")

    formatter.add_line("")

    formatter.open_scope("'matrix_compound_suites': {")

    for suite in graph.children(keys.project(), _targets_nodes.LEGACY_MATRIX_COMPOUND_SUITE.kind, graph.KEY_ORDER):
        formatter.add_line("")
        formatter.open_scope("'{}': {{".format(suite.key.id))
        for matrix_config in graph.children(suite.key, _targets_nodes.LEGACY_MATRIX_CONFIG.kind, graph.KEY_ORDER):
            # The order that mixins are declared is significant,
            # DEFINITION_ORDER preserves the order that the edges were added
            # from the parent to the child
            mixins = graph.children(matrix_config.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER)
            variants = graph.children(matrix_config.key, _targets_nodes.VARIANT.kind, graph.KEY_ORDER)
            if not (mixins or variants):
                formatter.add_line("'{}': {{}},".format(matrix_config.key.id))
                continue
            formatter.open_scope("'{}': {{".format(matrix_config.key.id))
            if mixins:
                formatter.open_scope("'mixins': [")
                for m in mixins:
                    formatter.add_line("'{}',".format(m.key.id))
                formatter.close_scope("],")
            if variants:
                formatter.open_scope("'variants': [")
                for v in variants:
                    formatter.add_line("'{}',".format(v.key.id))
                formatter.close_scope("],")
            formatter.close_scope("},")
        formatter.close_scope("},")

    formatter.close_scope("},")

    ctx.output["testing/test_suites.pyl"] = _PYL_HEADER_FMT.format(
        star_file = "//infra/config/targets/basic_suites.star, //infra/config/targets/compound_suites.star and/or //infra/config/targets/matrix_compound_suites.star",
        extra_comments = "",
        entries = formatter.output(),
    )

def register_pyl_generators():
    lucicfg.generator(_generate_gn_isolate_map_pyl)
    lucicfg.generator(_generate_mixins_pyl)
    lucicfg.generator(_generate_variants_pyl)
    lucicfg.generator(_generate_test_suites_pyl)
