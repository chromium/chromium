# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining targets that the chromium family of recipes can build/test."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./args.star", "args")
load("./chrome_settings.star", "targets_config")
load("./enums.star", "enums")
load("./structs.star", "structs")
load("./targets-internal/common.star", _targets_common = "common")
load("./targets-internal/nodes.star", _targets_nodes = "nodes")
load("./targets-internal/test-types/gpu_telemetry_test.star", "gpu_telemetry_test")
load("./targets-internal/test-types/gtest_test.star", "gtest_test")
load("./targets-internal/test-types/isolated_script_test.star", "isolated_script_test")
load("./targets-internal/test-types/junit_test.star", "junit_test")
load("./targets-internal/test-types/script_test.star", "script_test")

def _compile_target(*, name, label = None, skip_usage_check = False):
    """Define a compile target to use in targets specs.

    A compile target provides a mapping to any ninja target that will
    only be built, not executed.

    Args:
        name: The ninja target name. This is the name that can be used
            to refer to the target in other starlark declarations.
        label: The GN label for the ninja target.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
    """

    # The all target is a special ninja target that doesn't map to a GN label
    # and so we don't create an entry in gn_isolate_map.pyl
    if name != "all":
        if label == None:
            fail("label must be set in compile_target {}".format(name))
        _targets_common.create_label_mapping(
            name = name,
            type = "additional_compile_target",
            label = label,
            skip_usage_check = skip_usage_check,
        )

    _targets_common.create_compile_target(
        name = name,
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
        name: The ninja target name. This is the name that can be used
            to refer to the target/binary in other starlark
            declarations.
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
    _targets_common.create_binary(
        name = name,
        type = "console_test_launcher",
        label = label,
        label_type = label_type,
        skip_usage_check = skip_usage_check,
        args = args,
    )

def _generated_script(
        *,
        name,
        label,
        skip_usage_check = False,
        args = None,
        results_handler = None,
        merge = None,
        resultdb = None):
    """Define a generated script target to use in targets specs.

    A generated script target is a test that is executed via a script
    generated at build time. The script must be in
    output_dir/bin/run_$target (or output_dir\bin\run_$target.bat on
    Windows).

    Args:
        name: The ninja target name. This is the name that can be used
            to refer to the target/binary in other starlark
            declarations.
        label: The GN label for the ninja target.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
        args: The arguments to the test. These arguments will be
            included when the test is run using "mb try"
        results_handler: The name of the results handler to use for the
            test.
        merge: A targets.merge describing the invocation to merge the
            results from the test's tasks.
        resultdb: A targets.resultdb describing the ResultDB integration
            for the test.
    """
    _targets_common.create_binary(
        name = name,
        type = "generated_script",
        label = label,
        skip_usage_check = skip_usage_check,
        args = args,
        test_config = _targets_common.binary_test_config(
            results_handler = results_handler,
            merge = merge,
            resultdb = resultdb,
        ),
    )

def _script(
        *,
        name,
        label,
        script,
        skip_usage_check = False,
        args = None,
        merge = None,
        resultdb = None):
    """Define a script target to use in targets specs.

    A script target is a test that is executed via a python script.

    Args:
        name: The ninja target name. This is the name that can be used
            to refer to the target/binary in other starlark
            declarations.
        label: The GN label for the ninja target.
        script: The GN path (e.g. //testing/scripts/foo.py" to the python
            script to run.
        skip_usage_check: Disables checking that the target is actually
            referenced in a targets spec for some builder.
        args: The arguments to the test. These arguments will be
            included when the test is run using "mb try"
        merge: A targets.merge describing the invocation to merge the
            results from the test's tasks.
        resultdb: A targets.resultdb describing the ResultDB integration
            for the test.
    """
    _targets_common.create_binary(
        name = name,
        type = "script",
        label = label,
        script = script,
        skip_usage_check = skip_usage_check,
        args = args,
        test_config = _targets_common.binary_test_config(
            merge = merge,
            resultdb = resultdb,
        ),
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
        name: The ninja target name. This is the name that can be used
            to refer to the target/binary in other starlark
            declarations.
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
    _targets_common.create_binary(
        name = name,
        type = "windowed_test_launcher",
        label = label,
        label_type = label_type,
        executable = executable,
        executable_suffix = executable_suffix,
        skip_usage_check = skip_usage_check,
        args = args,
    )

def _cipd_package(
        *,
        package,
        location,
        revision):
    """Define a CIPD package to be downloaded by a swarmed test.

    Args:
        package: The package to download.
        location: The path relative to the task's execution directory to
            download the package to.
        revision: The revision of the package to download.

    Returns:
        A struct that can be passed as an element of the cipd_packages
        argument of targets.swarming.
    """
    return struct(
        package = package,
        location = location,
        revision = revision,
    )

def _resultdb(
        *,
        enable = None,
        has_native_resultdb_integration = None,
        result_format = None,
        result_file = None):
    """Define the ResultDB integration to be used for a test.

    Args:
        enable: Whether or not ResultDB is enabled for the test.
        has_native_resultdb_integration: Whether or not the test has
            native integration with resultdb. If not, result_adapter
            will be used, which parses output to determine results to
            upload results to ResultDB.
        result_format: The format of the test results.
        result_file: The file to write out test results to.

    Return:
        A struct that can be passed to the resultdb argument of
        `target.mixin`
    """
    return struct(
        enable = enable,
        has_native_resultdb_integration = has_native_resultdb_integration,
        result_format = result_format,
        result_file = result_file,
    )

def _skylab(
        *,
        cros_board = "",
        cros_img = "",
        use_lkgm = False,
        cros_model = None,
        autotest_name = None,
        bucket = None,
        dut_pool = None,
        public_builder = None,
        public_builder_bucket = None,
        shards = None,
        run_cft = False,
        args = []):
    """Define a Skylab test target.

    Args:
        cros_board: The CrOS build target name, e.g. "eve", "kevin".
        cros_img: ChromeOS image version to be deployed to DUT.
            Must be empty when use_lkgm is true.
            For example, "brya-release/R118-15604.42.0"
        use_lkgm: If True, use a ChromeOS image version derived from
            chromeos/CHROMEOS_LKGM file.
        cros_model: Optional ChromeOS DUT model.
        autotest_name: The name of the autotest to be executed in
            Skylab.
        bucket: Optional Google Storage bucket where the specified
            image(s) are stored.
        dut_pool: The skylab device pool to run the test. By default the
            quota pool, shared by all CrOS tests.
        public_builder: Optional Public CTP Builder.
            The public_builder and public_builder_bucket fields can be
            used when default CTP builder is not sufficient/advised
            (ex: chromium cq, satlab for partners).
        public_builder_bucket: Optional luci bucket. See public_builder
            above.
        shards: The number of shards used to run the test.
        run_cft: Whether enabled CFT mode for chromium tests on Skylab.
        args: The list of test arguments to be added to test CLI.
    """
    return struct(
        cros_board = cros_board,
        cros_img = cros_img,
        use_lkgm = use_lkgm,
        cros_model = cros_model,
        autotest_name = autotest_name,
        bucket = bucket,
        dut_pool = dut_pool,
        public_builder = public_builder,
        public_builder_bucket = public_builder_bucket,
        shards = shards,
        run_cft = run_cft,
        args = args,
    )

def _mixin_values(
        description = None,
        args = None,
        precommit_args = None,
        android_args = None,
        chromeos_args = None,
        desktop_args = None,
        lacros_args = None,
        linux_args = None,
        mac_args = None,
        win_args = None,
        win64_args = None,
        swarming = None,
        android_swarming = None,
        chromeos_swarming = None,
        skylab = None,
        use_isolated_scripts_api = None,
        ci_only = None,
        retry_only_failed_tests = None,
        check_flakiness_for_new_tests = None,
        resultdb = None,
        isolate_profile_data = None,
        merge = None,
        timeout_sec = None,
        shards = None,
        experiment_percentage = None):
    """Define values to be mixed into a target.

    Unless otherwise specified, each field will overwrite an existing
    value on a test.

    Args:
        description: A description to attach to the test. If specified,
            it will be appended to any description already present on
            the test.
        args: Arguments to be passed to the test binary. Will be
            appended to any existing args for the test.
        precommit_args: Arguments to be passed to the test binary when
            running on a try builder. Will be appended to any existing
            precommit_args for the test. These are applied at
            build-time, so they will always be emitted into the test
            spec if present.
        android_args: Arguments to be passed to the test when the
            builder is targeting android. Will be appended to any
            existing android_args for the test.
        chromeos_args: Arguments to be passed to the test when the
            builder is targeting chromeos. Will be appended to any
            existing chromeos_args for the test.
        desktop_args: Arguments to be passed to the test when the
            builder is targeting linux, mac or windows. Will be appended
            to any existing desktop_args for the test.
        linux_args: Arguments to be passed to the test when the builder
            is targeting linux. Will be appended to any existing
            linux_args for the test.
        lacros_args: Arguments to be passed to the test when the
            builder is targeting lacros. Will be appended to any
            existing lacros_args for the test.
        mac_args: Arguments to be passed to the test when the builder is
            targeting mac. Will be appended to any existing mac_args for
            the test.
        win_args: Arguments to be passed to the test when the builder
            is targeting win. Will be appended to any existing
            win_args for the test.
        win64_args: Arguments to be passed to the test when the builder
            is targeting win64. Will be appended to any existing
            win64_args for the test.
        swarming: A targets.swarming to be applied to the test. See
            targets.swarming for details about how each field is applied
            to the test's swarming details.
        android_swarming: A targets.swarming to be applied to the test
            when the builder is targeting android.
        chromeos_swarming: A targets.swarming to be applied to the test
            when the builder is targeting chromeos.
        skylab: A targets.skylab to be applied to the test. See
            targets.skylab for details about how each field is applied
            to the test.
        use_isolated_scripts_api: A bool indicating whether to use the
            isolated scripts interface to run the test. Only
            applicable to gtests.
        ci_only: A bool indicating whether the test should only be run
            in CI by default.
        check_flakiness_for_new_tests: A bool indicating whether try
            builders running the test should rerun new tests additional
            times to check for flakiness.
        retry_only_failed_tests: A bool indicating whether retrying the
            failing test will limit execution to only the failed test
            cases. By default, the entire shards that contain failing
            test cases will be rerun. Applies only to swarmed tests.
        resultdb: A targets.resultdb describing the ResultDB integration
            for the test.
        isolate_profile_data: A bool indicating whether profile data for
            the test should be included in the test tasks' output
            isolates.
        merge: A targets.merge describing the invocation to merge the
            results from the test's tasks.
        timeout_sec: The maximum time the test can take to run.
        shards: The number of shards to use for running the test on
            skylab.
        experiment_percentage: An integer in the range [0, 100]
            indicating the percentage chance that the test will be run,
            with failures in the test not resulting in failures in the
            build.

    Returns:
        A dict containing the values to be mixed in.
    """
    mixin_values = dict(
        description = description,
        args = args,
        precommit_args = precommit_args,
        android_args = android_args,
        chromeos_args = chromeos_args,
        desktop_args = desktop_args,
        lacros_args = lacros_args,
        linux_args = linux_args,
        mac_args = mac_args,
        win_args = win_args,
        win64_args = win64_args,
        swarming = swarming,
        android_swarming = android_swarming,
        chromeos_swarming = chromeos_swarming,
        skylab = skylab,
        use_isolated_scripts_api = use_isolated_scripts_api,
        ci_only = ci_only,
        retry_only_failed_tests = retry_only_failed_tests,
        check_flakiness_for_new_tests = check_flakiness_for_new_tests,
        resultdb = resultdb,
        isolate_profile_data = isolate_profile_data,
        merge = merge,
        timeout_sec = timeout_sec,
        shards = shards,
        experiment_percentage = experiment_percentage,
    )
    return {k: v for k, v in mixin_values.items() if v != None}

def _mixin(*, name = None, generate_pyl_entry = True, **kwargs):
    """Define a mixin used for defining tests.

    //infra/config/generated/testing/mixins.pyl will be generated from
    the declared mixins to be copied to //testing/buildbot and consumed
    by //testing/buildbot/generate_buildbot_json.py.

    Args:
        name: The name of the mixin.
        generate_pyl_entry: If true and name is provided, then the
            generated mixin.pyl file will contain an entry allowing this
            mixin to be used by generate_buildbot_json.py.
        **kwargs: The mixin values, see _mixin_values for allowed
            keywords and their meanings.
    """
    key = _targets_nodes.MIXIN.add(name, props = dict(
        mixin_values = _mixin_values(**kwargs),
    ))
    if generate_pyl_entry and name != None:
        graph.add_edge(keys.project(), key)
    return graph.keyset(key)

def _variant(
        *,
        name,
        identifier,
        enabled = None,
        mixins = None,
        **kwargs):
    """Define a variant used for defining tests.

    //infra/config/generated/testing/variants.pyl will be generated from
    the declared variants to be copied to //testing/buildbot and consumed
    by //testing/buildbot/generate_buildbot_json.py.

    Args:
        name: The name of the variant.
        identifier: A string suitable for display to users that
            identifies the variant of the test being run. When tests are
            expanded with the variant, this will be appended to the test
            name.
        enabled: Whether or not the variant is enabled. By default, a
            variant is enabled. If a variant is not enabled, then it
            will be ignored when expanding a test suite with variants.
        mixins: Names of mixins to apply when expanding a test with the
            variant.
        **kwargs: The mixin values, see _mixin_values for allowed
            keywords and their meanings.
    """
    if enabled == None:
        enabled = True
    key = _targets_nodes.VARIANT.add(name, props = dict(
        identifier = identifier,
        enabled = enabled,
        mixins = mixins,
        mixin_values = _mixin_values(**kwargs),
    ))

    graph.add_edge(keys.project(), key)

_builder_defaults = args.defaults(
    mixins = [],
)

def _bundle(*, name = None, additional_compile_targets = None, targets = None, mixins = None, per_test_modifications = None):
    """Define a targets bundle.

    A bundle is a grouping of targets to build and test.

    Args:
        name: The name of the bundle.
        targets: A list of targets, bundles or legacy basic suites to
            include in the bundle.
    """
    return graph.keyset(_targets_common.create_bundle(
        name = name,
        additional_compile_targets = args.listify(additional_compile_targets),
        targets = args.listify(targets),
        mixins = args.listify(mixins),
        per_test_modifications = per_test_modifications or {},
    ))

def _legacy_basic_suite(*, name, tests):
    """Define a basic suite.

    //infra/config/generated/testing/test_suites.pyl will be generated from the
    declared basic suites, as well as declared compound suites (see
    _legacy_compound_suite) and declared matrix compound suites (see
    _legacy_matrix_compound_suite) to be copied to //testing/buildbot and
    consumed by //testing/buildbot/generate_buildbot_json.py.

    A basic suite is a collection of tests that can be specified in
    waterfalls.pyl as the test suite for one of the test types or collected into
    a compound suite or matrix compound suite.

    Args:
        name: The name of the suite.
        tests: A dict mapping the name of the test to the base definition for
            the test, which must be an instance returned from
            targets.legacy_test_config.
    """
    basic_suite_key = _targets_nodes.LEGACY_BASIC_SUITE.add(name)
    graph.add_edge(keys.project(), basic_suite_key)

    bundle_key = _targets_common.create_bundle(
        name = name,
        targets = tests.keys(),
    )

    for t, config in tests.items():
        if not config:
            fail("The value for test {} in basic suite {} must be an object returned from targets.legacy_test_config"
                .format(t, name))
        d = {a: getattr(config, a) for a in dir(config)}
        mixins = d.pop("mixins") or []
        remove_mixins = d.pop("remove_mixins") or []

        config_key = _targets_nodes.LEGACY_BASIC_SUITE_CONFIG.add(name, t, props = dict(
            config = struct(**d),
        ))
        graph.add_edge(basic_suite_key, config_key)
        graph.add_edge(config_key, _targets_nodes.LEGACY_TEST.key(t))

        modification_key = _targets_nodes.PER_TEST_MODIFICATION.add(name, t)
        graph.add_edge(bundle_key, modification_key)

        basic_config_mixin = _mixin(**(config.mixin_values or {}))
        graph.add_edge(modification_key, _targets_nodes.MIXIN.key(basic_config_mixin))

        for m in mixins:
            mixin_key = _targets_nodes.MIXIN.key(m)
            graph.add_edge(config_key, mixin_key)
            graph.add_edge(modification_key, mixin_key)
        for r in remove_mixins:
            _targets_nodes.LEGACY_BASIC_SUITE_REMOVE_MIXIN.link(config_key, _targets_nodes.MIXIN.key(r))

def _legacy_test_config(
        *,
        # TODO(gbeaty) Tast tests should have their own test function defined
        # and this should be removed from this function
        tast_expr = None,
        # TODO(gbeaty) Skylab details should be modified to be under a separate
        # structure like swarming details are and this should be made a part of
        # mixins and removed from this function
        test_level_retries = None,
        mixins = None,
        remove_mixins = None,
        **kwargs):
    """Define the details of a test in a basic suite.

    Args:
        tast_expr: The tast expression to run. Only applicable to skylab tests.
        test_level_retries: The number of times to retry tests. Only applicable
            to skylab tests.
        mixins: A list of names of mixins to apply to the test.
        remove_mixins: A list of names of mixins to skip applying to the test.
        **kwargs: The mixin values, see _mixin_values for allowed keywords and
            their meanings.

    Returns:
        An object that can be used as a value in the dict passed to the
        tests argument of targets.legacy_basic_suite.
    """
    return struct(
        tast_expr = tast_expr,
        test_level_retries = test_level_retries,
        mixins = mixins,
        remove_mixins = remove_mixins,
        mixin_values = _mixin_values(**kwargs) or None,
    )

def _legacy_compound_suite(*, name, basic_suites):
    """Define a matrix compound suite.

    //infra/config/generated/testing/test_suites.pyl will be generated from the
    declared compound suites, as well as declared basic suites (see
    _legacy_basic_suite) and declared matrix compound suites (see
    _legacy_matrix_compound_suite) to be copied to //testing/buildbot and
    consumed by //testing/buildbot/generate_buildbot_json.py.

    A compound suite is a collection of basic suites that can be specified in
    waterfalls.pyl as the test suite for one of the test types.

    Args:
        name: The name of the matrix compound suite.
        basic_suites: A list of names of basic suites to compose.
    """
    legacy_compound_suite_key = _targets_nodes.LEGACY_COMPOUND_SUITE.add(name)
    graph.add_edge(keys.project(), legacy_compound_suite_key)

    for s in basic_suites:
        graph.add_edge(legacy_compound_suite_key, _targets_nodes.LEGACY_BASIC_SUITE.key(s))

    _targets_common.create_bundle(
        name = name,
        targets = basic_suites,
    )

def _legacy_matrix_compound_suite(*, name, basic_suites):
    """Define a matrix compound suite.

    //infra/config/generated/testing/test_suites.pyl will be generated from the
    declared matrix compound suites, as well as declared basic suites (see
    _legacy_basic_suite) and declared compound suites (see
    _legacy_compound_suite) to be copied to //testing/buildbot and
    consumed by //testing/buildbot/generate_buildbot_json.py.

    A matrix compound suite is a suite that composes multiple basic suites, with
    the capability to expand the tests of basic suites with specified variants.
    A matrix compound suite can be specified in waterfalls.pyl as the test suite
    for one of the test suite types.

    Args:
        name: The name of the matrix compound suite.
        basic_suites: A dict mapping the name of a basic suite to the matrix
            config for the suite, which must be an instance returned from
            targets.legacy_matrix_config. A None value is equivalent to
            targets.legacy_matrix_config(), which will add the tests from the
            basic suite without performing any variant expansion.
    """
    key = _targets_nodes.LEGACY_MATRIX_COMPOUND_SUITE.add(name)
    graph.add_edge(keys.project(), key)

    for basic_suite_name, config in basic_suites.items():
        # This edge won't actually be used, but it ensures that the basic suite exists
        graph.add_edge(key, _targets_nodes.LEGACY_BASIC_SUITE.key(basic_suite_name))
        matrix_config_key = _targets_nodes.LEGACY_MATRIX_CONFIG.add(name, basic_suite_name)
        graph.add_edge(key, matrix_config_key)
        config = config or _legacy_matrix_config()
        for m in config.mixins:
            graph.add_edge(matrix_config_key, _targets_nodes.MIXIN.key(m))
        for v in config.variants:
            graph.add_edge(matrix_config_key, _targets_nodes.VARIANT.key(v))

    # TODO: crbug.com/1420012 - Make matrix compound suites usable as bundles

def _legacy_matrix_config(*, mixins = [], variants = []):
    """Define the matrix details for a basic suite.

    Args:
        mixins: An optional list of mixins to apply to the tests of the
            corresponding basic suite.
        variants: An optional list of variants with which to expand the tests of
            the corresponding basic suite. If not provided, then the tests from
            the basic suite will be used without any variants applied.

    Returns:
        An object that can be used as a value in the dict passed to the
        basic_suites argument of targets.legacy_matrix_compound_suite.
    """
    return struct(
        mixins = mixins,
        variants = variants,
    )

# TODO: crbug.com/40258588 - Add support for remaining OS types
_os_type = enums.enum(
    ANDROID = "android",
)

_settings_defaults = args.defaults(
    os_type = None,
    use_swarming = True,
)

def _settings(
        *,
        os_type = args.DEFAULT,
        use_swarming = args.DEFAULT):
    """Settings that control the expansions of tests for a builder.

    Args:
      os_type - One of the values from targets.os_type that indicates the OS
        type that the tests target. Supports a module-level default.
      use_swarming - Whether tests for the builder should be swarmed. Supports a
        module-level default.

    Returns:
        A struct that can be passed to the targets_setting argument of the
        builder to control the expansion of tests for the builder.
    """
    os_type = _settings_defaults.get_value("os_type", os_type)
    if os_type and os_type not in _os_type.values:
        fail("unknown os_type: {}".format(os_type))
    use_swarming = _settings_defaults.get_value("use_swarming", use_swarming)
    return struct(
        os_type = os_type,
        use_swarming = use_swarming,

        # Computed properties
        is_android = os_type == _os_type.ANDROID,
    )

targets = struct(
    # Functions for declaring binaries, which can be referred to by gtests and
    # isolated script tests
    binaries = struct(
        console_test_launcher = _console_test_launcher,
        generated_script = _generated_script,
        script = _script,
        windowed_test_launcher = _windowed_test_launcher,
    ),

    # Functions for declaring tests
    tests = struct(
        gpu_telemetry_test = gpu_telemetry_test,
        gtest_test = gtest_test,
        isolated_script_test = isolated_script_test,
        junit_test = junit_test,
        script_test = script_test,
    ),

    # Functions for declaring compile targets
    compile_target = _compile_target,

    # Functions for declaring bundles
    bundle = _bundle,
    builder_defaults = _builder_defaults,
    settings = _settings,
    settings_defaults = _settings_defaults,
    os_type = _os_type,
    legacy_basic_suite = _legacy_basic_suite,
    legacy_test_config = _legacy_test_config,
    legacy_compound_suite = _legacy_compound_suite,
    legacy_matrix_compound_suite = _legacy_matrix_compound_suite,
    legacy_matrix_config = _legacy_matrix_config,
    mixin = _mixin,
    variant = _variant,
    cipd_package = _cipd_package,
    merge = _targets_common.merge,
    remove = _targets_common.remove,
    resultdb = _resultdb,
    swarming = _targets_common.swarming,
    skylab = _skylab,
)

################################################################################
# Code for generating targets spec files                                       #
################################################################################

def register_targets(*, parent_key, builder_group, builder_name, name, targets, settings):
    """Register the targets for a builder.

    This will create the necessary nodes and edges so that the targets spec for
    the builder can be generated via get_targets_spec_generator.

    Args:
      parent_key - The graph key of the parent node to register the targets for.
      name - The name to use for the registered bundle. This will allow for
        other builders to specify their targets in terms of another builder's.
      targets - The targets for the builder. Can take the form of the name of a
        separately-declared bundle, an unnamed targets.bundle instance or a list
        of such elements.
      settings - The targets.settings instance to use for expanding the tests
        for the builder. If None, then a default targets.setting instance will
        be used.
    """
    targets_key = _targets_common.create_bundle(
        name = name,
        builder_group = builder_group,
        builder_name = builder_name,
        targets = args.listify(targets),
        mixins = _builder_defaults.mixins.get(),
        settings = settings or _settings(),
    )

    graph.add_edge(parent_key, targets_key)

_OS_SPECIFIC_ARGS = set([
    "android_args",
])

_OS_SPECIFIC_SWARMING = set([
    "android_swarming",
])

def _apply_mixin(spec, mixin_values):
    invalid_mixin_values = set([k for k in mixin_values if k not in spec.value])
    if "args" in spec.value:
        invalid_mixin_values -= _OS_SPECIFIC_ARGS
    if "swarming" in spec.value:
        invalid_mixin_values -= _OS_SPECIFIC_SWARMING
    if invalid_mixin_values:
        # Return the original spec in the case of an error so that the caller
        # doesn't have to save the original value
        return spec, "unsupported mixin values: {}".format(sorted(invalid_mixin_values))

    spec_value = dict(spec.value)
    mixin_values = dict(mixin_values)

    # TODO: crbug.com/40258588 Implement support for the os-specific mixin values
    for a in _OS_SPECIFIC_ARGS | _OS_SPECIFIC_SWARMING:
        mixin_values.pop(a, None)

    args_mixin = mixin_values.pop("args", None)
    if args_mixin:
        spec_value["args"] = args.listify(spec_value["args"], args_mixin) or None

    swarming_mixin = mixin_values.pop("swarming", None)
    if swarming_mixin:
        spec_value["swarming"] = _targets_common.merge_swarming(spec_value["swarming"], swarming_mixin)

    spec_value.update(mixin_values)

    return structs.evolve(spec, value = spec_value), None

def _get_bundle_resolver():
    def resolved_bundle(*, additional_compile_targets, test_spec_and_source_by_name):
        return struct(
            additional_compile_targets = additional_compile_targets,
            test_spec_and_source_by_name = test_spec_and_source_by_name,
        )

    def visitor(_, children):
        return [c for c in children if c.key.kind == _targets_nodes.BUNDLE.kind]

    resolved_bundle_by_bundle_node_by_settings = {}

    def resolve(bundle_node, settings):
        resolved_bundle_by_bundle_node = resolved_bundle_by_bundle_node_by_settings.setdefault(settings, {})
        for n in graph.descendants(bundle_node.key, visitor = visitor, topology = graph.DEPTH_FIRST):
            if n in resolved_bundle_by_bundle_node:
                continue

            # TODO: crbug.com/1420012 - Update the handling of conflicting defs
            # so that more context is provided about where the error is
            # resulting from
            additional_compile_targets = set([t.key.id for t in graph.children(n.key, _targets_nodes.COMPILE_TARGET.kind)])

            test_spec_and_source_by_name = {}
            for test in graph.children(n.key, kind = _targets_nodes.TEST.kind):
                spec_handler = test.props.spec_handler
                spec_value = spec_handler.init(test, settings)
                spec = struct(handler = spec_handler, value = spec_value)

                # The order that mixins are declared is significant,
                # DEFINITION_ORDER preserves the order that the edges were added
                # from the parent to the child
                for m in graph.children(test.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
                    spec, error = _apply_mixin(spec, m.props.mixin_values)
                    if error:
                        fail("modifying {} {} with {} failed: {}"
                            .format(spec.handler.type_name, test.key.id, m, error))
                test_spec_and_source_by_name[test.key.id] = spec, n.key

            for child in graph.children(n.key, kind = _targets_nodes.BUNDLE.kind):
                child_resolved_bundle = resolved_bundle_by_bundle_node[child]
                additional_compile_targets = additional_compile_targets | child_resolved_bundle.additional_compile_targets
                for name, (spec, source) in child_resolved_bundle.test_spec_and_source_by_name.items():
                    if name in test_spec_and_source_by_name:
                        existing_spec, existing_source = test_spec_and_source_by_name[name]
                        if existing_spec != spec:
                            fail("target {} has conflicting definitions in deps of {}\n  {}: {}\n  {}: {}".format(
                                name,
                                n.key,
                                existing_source,
                                existing_spec,
                                source,
                                spec,
                            ))
                    test_spec_and_source_by_name[name] = (spec, source)

            def update_spec_with_mixin(test_name, spec, mixin):
                new_spec, error = _apply_mixin(spec, mixin.props.mixin_values)
                if error:
                    fail(
                        "modifying {} {} with {} failed: {}"
                            .format(spec.handler.type_name, test_name, mixin, error),
                        trace = n.props.stacktrace,
                    )
                test_spec_and_source_by_name[test_name] = new_spec, n.key

            for name in n.props.tests_to_remove:
                if name not in test_spec_and_source_by_name:
                    fail(
                        "attempting to remove test '{}' that is not contained in the bundle"
                            .format(name),
                        trace = n.props.stacktrace,
                    )
                test_spec_and_source_by_name.pop(name)

            # The order that mixins are declared is significant,
            # DEFINITION_ORDER preserves the order that the edges were added
            # from the parent to the child
            for mixin in graph.children(n.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
                for name, (spec, _) in test_spec_and_source_by_name.items():
                    update_spec_with_mixin(name, spec, mixin)
            for per_test_modification in graph.children(n.key, kind = _targets_nodes.PER_TEST_MODIFICATION.kind):
                name = per_test_modification.key.id
                if name not in test_spec_and_source_by_name:
                    fail(
                        "attempting to modify test '{}' that is not contained in the bundle"
                            .format(name),
                        trace = n.props.stacktrace,
                    )

                # The order that mixins are declared is significant,
                # DEFINITION_ORDER preserves the order that the edges were added
                # from the parent to the child
                for mixin in graph.children(per_test_modification.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
                    update_spec_with_mixin(name, test_spec_and_source_by_name[name][0], mixin)

            resolved_bundle_by_bundle_node[n] = resolved_bundle(
                additional_compile_targets = additional_compile_targets,
                test_spec_and_source_by_name = test_spec_and_source_by_name,
            )

        resolved = resolved_bundle_by_bundle_node[bundle_node]
        return (
            resolved.additional_compile_targets,
            {name: spec for name, (spec, _) in resolved.test_spec_and_source_by_name.items()},
        )

    return resolve

def get_targets_spec_generator():
    """Get a generator for builders' targets specs.

    Returns:
      A function that can be used to get the targets specs for a builder. The
      function takes a single argument that is a node. If the node corresponds
      to a builder that has tests registered using register_targets, then a dict
      will be returned with the target specs for the builder. Otherwise, None
      will be returned.
    """
    bundle_resolver = _get_bundle_resolver()
    autoshard_exceptions = targets_config().autoshard_exceptions

    def get_targets_spec(parent_node):
        bundle_nodes = graph.children(parent_node.key, _targets_nodes.BUNDLE.kind)
        if not bundle_nodes:
            return None
        if len(bundle_nodes) > 1:
            fail("internal error: there should be at most 1 targets_spec")
        bundle_node = bundle_nodes[0]

        settings = bundle_node.props.settings
        if not settings:
            fail("internal error: settings should be set for bundle_node")
        builder_group = bundle_node.props.builder_group
        if not builder_group:
            fail("internal error: builder_group should be set for bundle_node")
        builder_name = bundle_node.props.builder_name
        if not builder_name:
            fail("internal error: builder_name should be set for bundle_node")

        current_autoshard_exceptions = autoshard_exceptions.get(builder_group, {}).get(builder_name, {})

        additional_compile_targets, test_spec_by_name = bundle_resolver(bundle_node, settings)
        sort_key_and_specs_by_type_key = {}
        for name, spec in test_spec_by_name.items():
            spec_value = dict(spec.value)
            type_key, sort_key, spec_value = spec.handler.finalize(name, settings, spec_value)
            finalized_spec = {k: v for k, v in spec_value.items() if v not in ([], None)}
            if name in current_autoshard_exceptions:
                spec_value["swarming"]["shards"] = current_autoshard_exceptions[name]
            sort_key_and_specs_by_type_key.setdefault(type_key, []).append((sort_key, finalized_spec))

        specs_by_type_key = {}
        if additional_compile_targets:
            specs_by_type_key["additional_compile_targets"] = sorted(additional_compile_targets)
        for type_key, sort_key_and_specs in sorted(sort_key_and_specs_by_type_key.items()):
            specs_by_type_key[type_key] = [spec for _, spec in sorted(sort_key_and_specs)]

        return specs_by_type_key

    return get_targets_spec

################################################################################
# Generators for legacy .pyl files                                             #
################################################################################

_PYL_HEADER_FMT = """\
# THIS IS A GENERATED FILE DO NOT EDIT!!!
# Instead:
# 1. Modify {star_file}
# 2. Run //infra/config/main.star
# 3. Run //infra/config/scripts/sync-pyl-files.py

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
        entries = "\n".join(entries),
    )

lucicfg.generator(_generate_gn_isolate_map_pyl)

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
      formatter: The formatter object used for generating indented
        output.
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
      formatter: The formatter object used for generating indented
        output.
      mixin: Dict containing the mixin values to output.
      generate_skylab_container: Whether or not to generate the skylab
        key to contain the fields of the skylab value. Mixins and the
        generated test have those fields at top-level, but variants have
        them under a skylab key.
    """
    if "description" in mixin:
        formatter.add_line("'description': '{}',".format(mixin["description"]))

    for args_attr in (
        "args",
        "precommit_args",
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
        if skylab.cros_model:
            formatter.add_line("'cros_model': '{}',".format(skylab.cros_model))
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

        _generate_mixin_values(formatter, mixin)

        formatter.close_scope("},")

    ctx.output["testing/mixins.pyl"] = _PYL_HEADER_FMT.format(
        star_file = "//infra/config/targets/mixins.star",
        entries = formatter.output(),
    )

lucicfg.generator(_generate_mixins_pyl)

def _generate_variants_pyl(ctx):
    formatter = _formatter()

    for n in graph.children(keys.project(), _targets_nodes.VARIANT.kind, graph.KEY_ORDER):
        mixin = n.props.mixin_values
        formatter.open_scope("'{}': {{".format(n.key.id))

        formatter.add_line("'identifier': '{}',".format(n.props.identifier))

        if not n.props.enabled:
            formatter.add_line("'enabled': {},".format(n.props.enabled))

        _generate_mixin_values(formatter, mixin, generate_skylab_container = True)

        if n.props.mixins:
            formatter.open_scope("'mixins': [")
            for m in n.props.mixins:
                formatter.add_line("'{}',".format(m))
            formatter.close_scope("],")

        formatter.close_scope("},")

    ctx.output["testing/variants.pyl"] = _PYL_HEADER_FMT.format(
        star_file = "//infra/config/targets/variants.star",
        entries = formatter.output(),
    )

lucicfg.generator(_generate_variants_pyl)

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

            test_formatter = _formatter(indent_level = 0)

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

            mixins = []
            for n in (test_node, test_config_node):
                # The order that mixins are declared is significant,
                # DEFINITION_ORDER preserves the order that the edges were added
                # from the parent to the child
                for mixin in graph.children(n.key, _targets_nodes.MIXIN.kind, graph.DEFINITION_ORDER):
                    mixins.append(mixin.key.id)
            if mixins:
                test_formatter.open_scope("'mixins': [")
                for m in mixins:
                    test_formatter.add_line("'{}',".format(m))
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

            mixin_values = dict(suite_test_config.mixin_values or {})

            # Merge any args from the target with those specified for the test
            # in the suite
            merged_args = args.listify(target_test_config.args, mixin_values.get("args"))
            if merged_args:
                mixin_values["args"] = merged_args

            # merge and resultdb can be set on the binary, but don't override
            # values set on the test in the suite
            for a in ("merge", "resultdb"):
                value = getattr(binary_test_config, a)
                if value:
                    mixin_values.setdefault(a, value)
            _generate_mixin_values(test_formatter, mixin_values)

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
        entries = formatter.output(),
    )

lucicfg.generator(_generate_test_suites_pyl)
