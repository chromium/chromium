# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining targets that the chromium family of recipes can build/test."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./args.star", "args")
load("./targets-internal/common.star", _targets_common = "common")
load("./targets-internal/magic_args.star", _targets_magic_args = "magic_args")
load("./targets-internal/nodes.star", _targets_nodes = "nodes")
load("./targets-internal/pyl-generators.star", "register_pyl_generators")
load("./targets-internal/test-types/gpu_telemetry_test.star", "gpu_telemetry_test")
load("./targets-internal/test-types/gtest_test.star", "gtest_test")
load("./targets-internal/test-types/isolated_script_test.star", "isolated_script_test")
load("./targets-internal/test-types/junit_test.star", "junit_test")
load("./targets-internal/test-types/script_test.star", "script_test")

register_pyl_generators()

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
        result_file = None,
        inv_extended_properties_dir = None):
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
        inv_extended_properties_dir = inv_extended_properties_dir,
    )

def _skylab(
        *,
        cros_board = "",
        cros_img = "",
        cros_build_target = "",
        use_lkgm = False,
        cros_model = None,
        cros_cbx = False,
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
        cros_board: The CrOS DUT board name, e.g. "eve", "kevin".
        cros_build_target: The CrOS build target name, e.g. "eve-arc-t".
            If unspecified, the build target equals to cros_board will be used.
        cros_img: ChromeOS image version to be deployed to DUT.
            Must be empty when use_lkgm is true.
            For example, "brya-release/R118-15604.42.0"
        use_lkgm: If True, use a ChromeOS image version derived from
            chromeos/CHROMEOS_LKGM file.
        cros_model: Optional ChromeOS DUT model.
        cros_cbx: Whether to require a CBX DUT for given cros_board. For a
             board, not all models are CBX-capable.
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
        cros_build_target = cros_build_target,
        cros_img = cros_img,
        use_lkgm = use_lkgm,
        cros_model = cros_model,
        cros_cbx = cros_cbx,
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
        expand_as_isolated_script = None,
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
        expand_as_isolated_script: A bool indicating that the test
            should be expanded as an isolated script. Only applicable to
            gtests. Note this is different from
            use_isolated_scripts_api; expand_as_isolated_script is not
            part of the resultant spec and causes the spec to be output
            as an isolated script, whereas use_isolated_scripts_api
            is part of the expanded spec and tells the recipe to treat
            it as an isolated script but the expanded spec is still that
            of a gtest.
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
        expand_as_isolated_script = expand_as_isolated_script,
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

_IGNORE_UNUSED = "ignore_unused"

def _mixin(*, name = None, generate_pyl_entry = None, **kwargs):
    """Define a mixin used for defining tests.

    //infra/config/generated/testing/mixins.pyl will be generated from
    the declared mixins to be copied to //testing/buildbot and consumed
    by //testing/buildbot/generate_buildbot_json.py.

    Args:
        name: The name of the mixin.
        generate_pyl_entry: If true, the generated mixin.pyl will
            contain an entry allowing the mixin to be used by
            generate_buildbot_json.py. If set to targets.IGNORE_UNUSED,
            then an entry will be generated that
            generate_buildbot_json.py which won't cause an error if it
            isn't used. This enables mixins to be generated to the pyl
            file that are only used by the angle repo, which reuses the
            generated mixins.pyl. By default, this will be True if name
            is provided.
        **kwargs: The mixin values, see _mixin_values for allowed
            keywords and their meanings.
    """
    if generate_pyl_entry not in (None, False, True, _IGNORE_UNUSED):
        fail("unexpected value for generate_pyl_entry: {}".format(generate_pyl_entry))
    if generate_pyl_entry == None:
        generate_pyl_entry = name != None
    elif generate_pyl_entry:
        if name == None:
            fail("pyl entries can't be generated for anonymous mixins")
    key = _targets_nodes.MIXIN.add(name, props = dict(
        mixin_values = _mixin_values(**kwargs),
        pyl_fail_if_unused = generate_pyl_entry == True,
    ))
    if generate_pyl_entry and name != None:
        graph.add_edge(keys.project(), key)
    return graph.keyset(key)

def _variant(
        *,
        name,
        identifier,
        generate_pyl_entry = True,
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
        generate_pyl_entry: If true, the generated variants.pyl will
            contain an entry allowing the mixin to be used by
            generate_buildbot_json.py.
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
    variant_key = _targets_nodes.VARIANT.add(name, props = dict(
        identifier = identifier,
        enabled = enabled,
        mixin_values = _mixin_values(**kwargs),
    ))

    for m in mixins or []:
        if generate_pyl_entry and type(m) != type(""):
            fail("variants used by //testing/buildbot cannot use anonymous mixins", trace = stacktrace(skip = 2))
        mixin_key = _targets_nodes.MIXIN.key(m)
        graph.add_edge(variant_key, mixin_key)

    if generate_pyl_entry:
        graph.add_edge(keys.project(), variant_key)

def _bundle(*, name = None, additional_compile_targets = None, targets = None, mixins = None, variants = None, per_test_modifications = None):
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
        variants = args.listify(variants),
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

    def _per_test_modification(test_config):
        mixins = args.listify(_mixin(**(test_config.mixin_values or {})), test_config.mixins)
        return _targets_common.per_test_modification(
            mixins = mixins,
            remove_mixins = test_config.remove_mixins,
        )

    _targets_common.create_bundle(
        name = name,
        targets = tests.keys(),
        per_test_modifications = {
            t: _per_test_modification(config)
            for t, config in tests.items()
        },
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

        for m in mixins:
            mixin_key = _targets_nodes.MIXIN.key(m)
            graph.add_edge(config_key, mixin_key)
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

    dep_targets = []
    for basic_suite_name, config in basic_suites.items():
        # This edge won't actually be used, but it ensures that the basic suite exists
        graph.add_edge(key, _targets_nodes.LEGACY_BASIC_SUITE.key(basic_suite_name))
        matrix_config_key = _targets_nodes.LEGACY_MATRIX_CONFIG.add(name, basic_suite_name)
        graph.add_edge(key, matrix_config_key)
        config = config or _legacy_matrix_config()
        for v in config.variants:
            graph.add_edge(matrix_config_key, _targets_nodes.VARIANT.key(v))
        for m in config.mixins:
            graph.add_edge(matrix_config_key, _targets_nodes.MIXIN.key(m))
        if config.variants or config.mixins:
            dep_targets.append(_bundle(
                targets = basic_suite_name,
                variants = config.variants,
                mixins = config.mixins,
            ))
        else:
            dep_targets.append(basic_suite_name)

    _targets_common.create_bundle(
        name = name,
        targets = dep_targets,
    )

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
    per_test_modification = _targets_common.per_test_modification,
    builder_defaults = _targets_common.builder_defaults,
    settings = _targets_common.settings,
    settings_defaults = _targets_common.settings_defaults,
    browser_config = _targets_common.browser_config,
    os_type = _targets_common.os_type,
    legacy_basic_suite = _legacy_basic_suite,
    legacy_test_config = _legacy_test_config,
    legacy_compound_suite = _legacy_compound_suite,
    legacy_matrix_compound_suite = _legacy_matrix_compound_suite,
    legacy_matrix_config = _legacy_matrix_config,
    mixin = _mixin,
    IGNORE_UNUSED = _IGNORE_UNUSED,
    variant = _variant,
    cipd_package = _cipd_package,
    merge = _targets_common.merge,
    remove = _targets_common.remove,
    resultdb = _resultdb,
    swarming = _targets_common.swarming,
    skylab = _skylab,
    magic_args = _targets_magic_args,
)
