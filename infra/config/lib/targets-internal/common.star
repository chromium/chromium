# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions needed for targets implementation."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("//lib/args.star", args_lib = "args")
load("//lib/enums.star", "enums")
load("./nodes.star", _targets_nodes = "nodes")

_builder_defaults = args_lib.defaults(
    mixins = [],
)

_browser_config = enums.enum(
    ANDROID_CHROMIUM = "android-chromium",
    ANDROID_CHROMIUM_MONOCHROME = "android-chromium-monochrome",
    ANDROID_WEBVIEW = "android-webview",
    CROS_CHROME = "cros-chrome",
    DEBUG = "debug",
    DEBUG_X64 = "debug_x64",
    LACROS_CHROME = "lacros-chrome",
    RELEASE = "release",
    RELEASE_X64 = "release_x64",
    WEB_ENGINE_SHELL = "web-engine-shell",
)

# TODO: crbug.com/40258588 - Add support for remaining OS types
_os_type = enums.enum(
    ANDROID = "android",
    CROS = "chromeos",
    FUCHSIA = "fuchsia",
    LACROS = "lacros",
    LINUX = "linux",
    MAC = "mac",
    WINDOWS = "win",
)

_settings_defaults = args_lib.defaults(
    allow_script_tests = True,
    browser_config = None,
    os_type = None,
    use_swarming = True,
    use_android_merge_script_by_default = True,
)

def _settings(
        *,
        allow_script_tests = args_lib.DEFAULT,
        browser_config = args_lib.DEFAULT,
        os_type = args_lib.DEFAULT,
        use_swarming = args_lib.DEFAULT,
        use_android_merge_script_by_default = args_lib.DEFAULT):
    """Settings that control the expansions of tests for a builder.

    Args:
        allow_script_tests: A bool controlling whether the builder can be
            configured to run script tests. It is an error if allow_script_tests
            is False and a builder includes script tests. Supports a
            module-level default.
        browser_config: One of the values from targets.browser_config that
            indicates the configuration of the browser to execute the test with.
        os_type: One of the values from targets.os_type that indicates the OS
            type that the tests target. Supports a module-level default.
        use_swarming: Whether tests for the builder should be swarmed. Supports
            a module-level default.
        use_android_merge_script_by_default: Whether tests targeting Android
            will use the Android merge script by default. Has no effect for
            non-swarming tests, non-Android tests or tests that have a merge
            script specified.

    Returns:
        A struct that can be passed to the targets_setting argument of the
        builder to control the expansion of tests for the builder.
    """
    browser_config = _settings_defaults.get_value("browser_config", browser_config)
    if browser_config and browser_config not in _browser_config.values:
        fail("unknown browser_config: {}".format(browser_config))
    os_type = _settings_defaults.get_value("os_type", os_type)
    if os_type and os_type not in _os_type.values:
        fail("unknown os_type: {}".format(os_type))

    allow_script_tests = _settings_defaults.get_value("allow_script_tests", allow_script_tests)
    use_swarming = _settings_defaults.get_value("use_swarming", use_swarming)
    use_android_merge_script_by_default = _settings_defaults.get_value(
        "use_android_merge_script_by_default",
        use_android_merge_script_by_default,
    )
    return struct(
        allow_script_tests = allow_script_tests,
        browser_config = browser_config,
        os_type = os_type,
        use_swarming = use_swarming,
        use_android_merge_script_by_default = use_android_merge_script_by_default,

        # Computed properties
        is_android = os_type == _os_type.ANDROID,
        is_cros = os_type == _os_type.CROS,
        is_desktop = os_type != _os_type.ANDROID,
        is_fuchsia = os_type == _os_type.FUCHSIA,
        is_lacros = os_type == _os_type.LACROS,
        is_linux = os_type == _os_type.LINUX,
        is_mac = os_type == _os_type.MAC,
        is_win = os_type == _os_type.WINDOWS,
        is_win64 = os_type == (_os_type.WINDOWS and browser_config == _browser_config.RELEASE_X64),
    )

def _create_compile_target(*, name):
    _targets_nodes.COMPILE_TARGET.add(name)

def _create_label_mapping(
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
    mapping_key = _targets_nodes.LABEL_MAPPING.add(name, props = dict(
        type = type,
        label = label,
        label_type = label_type,
        executable = executable,
        executable_suffix = executable_suffix,
        script = script,
        skip_usage_check = skip_usage_check,
        args = args,
    ))
    graph.add_edge(keys.project(), mapping_key)

def _binary_test_config(*, results_handler = None, merge = None, resultdb = None):
    """The details for a test provided by the test's binary.

    When test_suites.pyl is generated, tests that are using the binary
    will have these values written into the test's entry in the basic suite.

    Args:
        results_handler: The name of the results handler to use for the
            test.
        merge: A targets.merge describing the invocation to merge the
            results from the test's tasks.
        resultdb: A targets.resultdb describing the ResultDB integration
            for the test.
    """
    return struct(
        results_handler = results_handler,
        merge = merge,
        resultdb = resultdb,
    )

def _create_binary(
        *,
        name,
        type,
        label,
        label_type = None,
        executable = None,
        executable_suffix = None,
        script = None,
        skip_usage_check = False,
        args = None,
        test_config = None):
    _create_label_mapping(
        name = name,
        type = type,
        label = label,
        label_type = label_type,
        executable = executable,
        executable_suffix = executable_suffix,
        script = script,
        skip_usage_check = skip_usage_check,
        args = args,
    )

    label_pieces = label.split(":")
    if len(label_pieces) != 2:
        fail((
            "malformed label '{}' for binary '{}''," +
            " implicit names (like //f/b meaning //f/b:b) are disallowed",
        ).format(label, name))
    if label_pieces[1] != name:
        fail((
            "binary name '{}' doesn't match GN target name in label '{}'," +
            "see http://crbug.com/1071091 for details"
        ).format(name, label_pieces[1]))
    test_id_prefix = "ninja:{}/".format(label)

    _create_compile_target(
        name = name,
    )

    return _targets_nodes.BINARY.add(name, props = dict(
        test_id_prefix = test_id_prefix,
        test_config = test_config,
    ))

def _basic_suite_test_config(
        *,
        script = None,
        binary = None,
        telemetry_test_name = None,
        args = None,
        precommit_args = None,
        non_precommit_args = None):
    """The details for the test included when included in a basic suite.

    When generating test_suites.pyl, these values will be written out
    for a test in any basic suite that includes it.

    Args:
        script: The name of the file within the //testing/scripts
            directory to run as the test. Only applicable to script tests.
        binary: The name of the binary to run as the test. Only
            applicable to gtests, isolated script tests and junit tests.
        telemetry_test_name: The telemetry test to run. Only applicable
            to telemetry test types.
        args: Arguments to be passed to the test binary.
    """
    return struct(
        script = script,
        binary = binary,
        telemetry_test_name = telemetry_test_name,
        args = args,
        precommit_args = precommit_args,
        non_precommit_args = non_precommit_args,
    )

def _create_legacy_test(*, name, basic_suite_test_config, mixins = None):
    test_key = _targets_nodes.LEGACY_TEST.add(name, props = dict(
        basic_suite_test_config = basic_suite_test_config,
    ))
    for m in args_lib.listify(mixins):
        graph.add_edge(test_key, _targets_nodes.MIXIN.key(m))
    return test_key

def _remove(*, reason):
    """Declaration that can be used to remove a test from a bundle.

    Args:
        reason: The reason that the test is being removed.

    Returns:
        An object that can be passed as a value in the per_test_modifications
            dict of a bundle declaration in order to remove the test from the
            bundle.
    """
    if not reason:
        fail("A non-empty reason must be specified to remove a test")
    return struct(
        __targets_remove__ = reason,
    )

def _per_test_modification(*, mixins = None, remove_mixins = None):
    return struct(
        mixins = args_lib.listify(mixins),
        remove_mixins = args_lib.listify(remove_mixins),
    )

def _create_bundle(
        *,
        name,
        additional_compile_targets = [],
        targets = [],
        builder_group = None,
        builder_name = None,
        settings = None,
        mixins = [],
        variants = [],
        per_test_modifications = {}):
    tests_to_remove = []
    for test_name, mods in per_test_modifications.items():
        if hasattr(mods, "__targets_remove__"):
            tests_to_remove.append(test_name)
            per_test_modifications.pop(test_name)

    bundle_key = _targets_nodes.BUNDLE.add(name, props = dict(
        builder_group = builder_group,
        builder_name = builder_name,
        settings = settings,
        tests_to_remove = tests_to_remove,
        # Record the stacktrace so that failures actually point out the failing
        # definition (this is especially important for unnamed bundles since
        # they won't have a useful name to search for)
        stacktrace = stacktrace(skip = 3),
    ))

    for t in additional_compile_targets:
        graph.add_edge(bundle_key, _targets_nodes.COMPILE_TARGET.key(t))
    for t in targets:
        graph.add_edge(bundle_key, _targets_nodes.BUNDLE.key(t))
    for m in mixins:
        graph.add_edge(bundle_key, _targets_nodes.MIXIN.key(m))
    for v in variants:
        graph.add_edge(bundle_key, _targets_nodes.VARIANT.key(v))
    for test_name, mods in per_test_modifications.items():
        # Use bundle_key.id here instead of name because an inline bundle will
        # have None for name
        modification_key = _targets_nodes.PER_TEST_MODIFICATION.add(bundle_key.id, test_name)
        graph.add_edge(bundle_key, modification_key)

        # mods may be a single unnamed mixin, which would appear here as a
        # keyset, which is also a struct
        if graph.is_keyset(mods) or type(mods) != type(struct()):
            mods = _per_test_modification(
                mixins = mods,
            )
        for m in mods.mixins:
            graph.add_edge(modification_key, _targets_nodes.MIXIN.key(m))
        for r in mods.remove_mixins:
            _targets_nodes.REMOVE_MIXIN.link(modification_key, _targets_nodes.MIXIN.key(r))
    return bundle_key

def _create_test(*, name, spec_handler, details = None, mixins = None):
    test_key = _targets_nodes.TEST.add(name, props = dict(
        spec_handler = spec_handler,
        details = details,
    ))
    bundle_key = _create_bundle(
        name = name,
    )
    graph.add_edge(bundle_key, test_key)
    for m in args_lib.listify(mixins):
        graph.add_edge(test_key, _targets_nodes.MIXIN.key(m))
    return test_key

def _get_test_binary_node(node):
    binary_nodes = graph.children(node.key, _targets_nodes.BINARY.kind)
    if len(binary_nodes) != 1:
        fail("internal error: test node {} should have link to exactly 1 binary node, got {}".format(node, binary_nodes))
    binary_node = binary_nodes[0]
    return binary_node

def _spec_handler(*, type_name, init, finalize):
    """Declare a spec handler for a target type.

    The node that is added for each test (type _target_nodes.TEST) will store a
    spec handler. When creating the initial spec for a test, the init function
    of the handler will be called. After the spec has been modified by all
    applicable mixins, the finalize function of the handler will be called to
    get the (mosty-)final value of the spec.

    Args:
        type_name: The name of the test type. This will be used in error
            messages.
        init: The function to create the initial value of the spec. The
            function will be called with the test node (type _target_nodes.TEST)
            and should return a dict with all keys populated that are supported
            by the type.
        finalize: The function that produces the (mostly-)final value of a spec
            for a target. The function will be passed the name of the builder,
            the name of the test and the spec value (dict) that has been
            modified by all applicable mixins. The function should return a
            3-tuple:
            * The test_suites key that the spec should be added to in the output
                json file (one of "gtest_tests", "isolated_scripts",
                "junit_tests", "scripts" or "skylab_tests").
            * The sort key used to order tests for a given test_suites key. The
                format is up to the spec handler, but all sort keys for a given
                test_suites key must be comparable.
            * The final value of the spec. This must be a dict with string keys.
                Any items in the dict where the value is None or [] will not be
                emitted in the final json.

    Returns:
        An object that can be passed to the spec_handler argument of
        common.create_test.
    """
    return struct(
        type_name = type_name,
        init = init,
        finalize = finalize,
    )

# TODO: crbug.com/1420012 - Update the handling of unimplemented test types so
# that more context is provided about where the error is resulting from
def _spec_handler_for_unimplemented_target_type(type_name):
    def unimplemented():
        fail("support for {} targets is not yet implemented".format(type_name))

    return _spec_handler(
        type_name = type_name,
        init = (lambda node, settings: unimplemented()),
        finalize = (lambda builder_name, test_name, settings, spec: unimplemented()),
    )

def _merge(
        *,
        script,
        args = None):
    """Define a merge script to be used for a swarmed test.

    Args:
        script: GN-format path (e.g. //foo/bar/script.py) to the script
            to use to merge results from the shard tasks.
        args: Any args to pass to the merge script, in addition to any
            arguments supplied by the recipe.

    Returns:
        A struct that can be passed to the merge argument of
        `targets.mixin`.
    """
    return struct(
        script = script,
        args = args,
    )

def _finalize_merge(merge):
    if not merge:
        return None
    d = {a: getattr(merge, a) for a in dir(merge)}
    return {k: v for k, v in d.items() if v != None}

def _swarming(
        *,
        enable = None,
        dimensions = None,
        optional_dimensions = None,
        containment_type = None,
        cipd_packages = None,
        expiration_sec = None,
        hard_timeout_sec = None,
        io_timeout_sec = None,
        shards = None,
        idempotent = None,
        service_account = None,
        named_caches = None):
    """Define the swarming details for a test.

    When specified as a mixin, fields will overwrites the test's values
    unless otherwise indicated.

    Args:
        enable: Whether swarming should be enabled for the test.
        dimensions: A dict of dimensions to apply to all dimension sets
            for the test. This can only be specified in a mixin. After
            any dimension sets from the mixin are added to the test, the
            dimensions will be applied to all of the dimension sets on
            the test. If there are no dimension sets on the test, a
            single dimension set with these dimensions will be added.
        optional_dimensions: Optional dimensions to add to each
            dimension set.
        containment_type: The containment type to use for the swarming
            task(s). See ContainmentType enum in
            https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/swarming/proto/api/swarming.proto
        cipd_packages: A list of targets.cipd_package that detail CIPD
            packages to be downloaded for the test.
        expiration_sec: The time that each task for the test should wait
            to be scheduled.
        hard_timeout_sec: The maximum time each task for the test can
            take after starting.
        io_timeout_sec: The maximum time that can elapse between output
            from tasks for the test.
        shards: The number of tasks to split the test into.
        idempotent: Whether the test task should be considered
            idempotent.
        service_account: The service account used to run the test's
            tasks.
        named_caches: A list of swarming.cache that detail the named
            caches that should be mounted for the test's tasks.
    """
    return struct(
        enable = enable,
        dimensions = dimensions,
        optional_dimensions = optional_dimensions,
        containment_type = containment_type,
        cipd_packages = cipd_packages,
        expiration_sec = expiration_sec,
        hard_timeout_sec = hard_timeout_sec,
        io_timeout_sec = io_timeout_sec,
        shards = shards,
        idempotent = idempotent,
        service_account = service_account,
        named_caches = named_caches,
    )

def _finalize_cipd_package(cipd_package):
    d = {a: getattr(cipd_package, a) for a in dir(cipd_package)}
    d["cipd_package"] = d.pop("package")
    return d

def _finalize_named_cache(named_cache):
    d = {a: getattr(named_cache, a) for a in dir(named_cache)}
    return {k: v for k, v in d.items() if v != None}

def _merge_swarming(swarming1, swarming2):
    if not (swarming1 and swarming2):
        return swarming1 or swarming2

    d = {a: getattr(swarming1, a) for a in dir(swarming1)}
    to_merge = {a: getattr(swarming2, a) for a in dir(swarming2)}

    d["dimensions"] = ((d["dimensions"] or {}) | (to_merge.pop("dimensions") or {})) or None
    d["named_caches"] = args_lib.listify(d["named_caches"], to_merge.pop("named_caches")) or None
    for k, v in to_merge.items():
        if v != None:
            d[k] = v
    return _swarming(**d)

def _finalize_swarming(swarming):
    if not swarming or not swarming.enable:
        return None
    d = {a: getattr(swarming, a) for a in dir(swarming) if a != "enable"}
    for dst, src in (
        ("expiration", "expiration_sec"),
        ("hard_timeout", "hard_timeout_sec"),
        ("io_timeout", "io_timeout_sec"),
    ):
        d[dst] = d.pop(src)
    cipd_packages = d["cipd_packages"]
    if cipd_packages:
        d["cipd_packages"] = [_finalize_cipd_package(p) for p in cipd_packages]
    named_caches = d["named_caches"]
    if named_caches:
        d["named_caches"] = [_finalize_named_cache(c) for c in named_caches]
    if d["shards"] == 1:
        d.pop("shards")
    if d["optional_dimensions"]:
        d["optional_dimensions"] = {str(k): v for k, v in d["optional_dimensions"].items()}
    return {k: v for k, v in d.items() if v != None}

def _finalize_resultdb(resultdb):
    if not resultdb:
        return None
    d = {a: getattr(resultdb, a) for a in dir(resultdb)}
    return {k: v for k, v in d.items() if v != None}

def _spec_init(node, settings, *, additional_fields = {}, binary_node = None):
    """Init for gtest and isolated script test specs."""
    binary_node = binary_node or _get_test_binary_node(node)
    binary_test_config = binary_node.props.test_config or _binary_test_config()
    return dict(
        name = node.key.id,
        description = None,
        test = binary_node.key.id,
        test_id_prefix = binary_node.props.test_id_prefix,
        args = list(node.props.details.args or []),
        ci_only = None,
        experiment_percentage = None,
        precommit_args = [],
        retry_only_failed_tests = None,
        isolate_profile_data = None,
        swarming = _swarming(enable = settings.use_swarming),
        merge = binary_test_config.merge,
        resultdb = binary_test_config.resultdb,
        results_handler = binary_test_config.results_handler,
        **additional_fields
    )

def _update_spec_for_android_presentation(settings, spec_value):
    results_bucket = "chromium-result-details"
    spec_value["args"] = args_lib.listify(spec_value["args"], "--gs-results-bucket={}".format(results_bucket))
    if spec_value["swarming"].enable and not spec_value["merge"] and settings.use_android_merge_script_by_default:
        spec_value["merge"] = _merge(
            script = "//build/android/pylib/results/presentation/test_results_presentation.py",
            args = ["--bucket", results_bucket, "--test-name", spec_value["name"]],
        )

def _resolve_magic_args(builder_name, settings, spec_value):
    new_args = []
    for arg in spec_value["args"]:
        if type(arg) == type(struct()):
            new_args.extend(arg.function(builder_name, settings, spec_value))
        else:
            new_args.append(arg)
    spec_value["args"] = new_args

def _spec_finalize(builder_name, settings, spec_value, default_merge_script):
    swarming = _finalize_swarming(spec_value["swarming"])
    spec_value["swarming"] = swarming

    # Ensure all Android Swarming tests run only on userdebug builds if another
    # build type was not specified.
    if swarming and settings.is_android:
        dimensions = swarming.get("dimensions", {})
        if dimensions.get("os") == "Android" and "device_type_os" not in dimensions:
            swarming["dimensions"] = dimensions | {"device_os_type": "userdebug"}
    if swarming and not spec_value["merge"]:
        spec_value["merge"] = _merge(
            script = "//testing/merge_scripts/{}.py".format(default_merge_script),
        )
    spec_value["merge"] = _finalize_merge(spec_value["merge"])
    spec_value["resultdb"] = _finalize_resultdb(spec_value["resultdb"])

    if spec_value["args"]:
        _resolve_magic_args(builder_name, settings, spec_value)

    return spec_value

common = struct(
    # Functions used for creating objects that are part of the public API that
    # need to be used internally as well
    builder_defaults = _builder_defaults,
    settings = _settings,
    settings_defaults = _settings_defaults,
    browser_config = _browser_config,
    os_type = _os_type,
    merge = _merge,
    remove = _remove,
    swarming = _swarming,

    # Functions for performing common operations
    merge_swarming = _merge_swarming,

    # Functions used for creating nodes by functions that define targets
    binary_test_config = _binary_test_config,
    create_compile_target = _create_compile_target,
    create_label_mapping = _create_label_mapping,
    create_binary = _create_binary,
    basic_suite_test_config = _basic_suite_test_config,
    create_legacy_test = _create_legacy_test,
    create_test = _create_test,
    per_test_modification = _per_test_modification,
    create_bundle = _create_bundle,

    # Functions for defining target spec types
    spec_handler = _spec_handler,
    spec_handler_for_unimplemented_target_type = _spec_handler_for_unimplemented_target_type,

    # Functions for implementing spec handlers
    spec_init = _spec_init,
    update_spec_for_android_presentation = _update_spec_for_android_presentation,
    spec_finalize = _spec_finalize,
    finalize_resultdb = _finalize_resultdb,
)
