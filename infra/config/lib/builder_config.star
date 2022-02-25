# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining chromium_tests_builder_config properties."""

load("./args.star", "args")
load("./nodes.star", "nodes")

# TODO(gbeaty) Add support for PROVIDE_TEST_SPEC mirrors

def _enum(**kwargs):
    """Create an enum struct.

    Args:
        **kwargs - The enum values to create. A field will be added the struct
            with the key for the name of the field and the value for the value
            of the field.

    Returns:
        A struct with fields for each item in `kwargs`. The struct will
        also have a `_values` field that contains a list of the values in
        `kwargs`.
    """
    if "_values" in kwargs:
        fail("cannot create an enum value named '_values'")
    return struct(_values = kwargs.values(), **kwargs)

def _struct_with_non_none_values(**kwargs):
    return struct(**{k: v for k, v in kwargs.items() if v != None})

_execution_mode = _enum(
    # The builder will perform compilation of any targets configured in the
    # testing spec as well as any tests that will be run by the builder or any
    # triggered builders.
    COMPILE_AND_TEST = "COMPILE_AND_TEST",
    # The builder will only execute tests, which another builder must have
    # already compiled and either isolated or transferred using the legacy
    # package transfer mechanism.
    TEST = "TEST",
)

def _gclient_config(*, config, apply_configs = None):
    """The details for configuring gclient recipe module.

    This uses the recipe engine's config item facility.

    Args:
        config: (str) The name of the recipe module config item to use.
        apply_configs: (list[str]|str) Additional configs to apply.

    Returns:
        A struct that can be passed to the `gclient_config` argument of
        `builder_spec`.
    """
    if not config:
        fail("config must be provided")
    return _struct_with_non_none_values(
        config = config,
        apply_configs = args.listify(apply_configs) or None,
    )

_build_config = _enum(
    RELEASE = "Release",
    DEBUG = "Debug",
)

_target_arch = _enum(
    INTEL = "intel",
    ARM = "arm",
    MIPS = "mips",
    MIPSEL = "mipsel",
)

_target_platform = _enum(
    LINUX = "linux",
    WIN = "win",
    MAC = "mac",
    IOS = "ios",
    ANDROID = "android",
    CHROMEOS = "chromeos",
    FUCHSIA = "fuchsia",
)

def _chromium_config(
        *,
        config,
        apply_configs = None,
        build_config = None,
        target_arch = None,
        target_bits = None,
        target_platform = None,
        target_cros_boards = None,
        cros_boards_with_qemu_images = None):
    """The details for configuring chromium recipe module.

    This uses the recipe engine's config item facility.

    Args:
        config: (str) The name of the recipe module config item to use.
        apply_configs: (list[str]|str) Additional configs to apply.
        build_config: (build_config) The build config value to use.
        target_arch: (target_arch) The target architecture to build for.
        target_bits: (int) The target bit count to build for.
        target_platform: (target_platform) The target platform to build for.
        target_cros_boards: (list[str]|str) The CROS boards to target, SDKs will
            be downloaded for each board. Can only be specified if
            `target_platform` is `target_platform.CHROMEOS`.
        cros_boards_with_qemu_images: (list[str]|str) Same as
            `target_cros_boards`, but a VM image for the board will be
            downloaded as well. Can only be specified if `target_platform` is
            `target_platform.CHROMEOS`.

    Returns:
        A struct that can be passed to the `chromium_config` argument of
        `builder_spec`.
    """
    if not config:
        fail("config must be provided")
    if build_config != None and build_config not in _build_config._values:
        fail("unknown build_config: {}".format(build_config))
    if target_arch != None and target_arch not in _target_arch._values:
        fail("unknown target_arch: {}".format(target_arch))
    if target_bits != None and target_bits not in (32, 64):
        fail("unknown target_bits: {}".format(target_bits))
    if target_platform != None and target_platform not in _target_platform._values:
        fail("unknown target_platform: {}".format(target_platform))
    if ((target_cros_boards or cros_boards_with_qemu_images) and
        target_platform != _target_platform.CHROMEOS):
        fail("CrOS boards can only be specified for target platform '{}'"
            .format(_target_platform.CHROMEOS))

    return _struct_with_non_none_values(
        config = config,
        apply_configs = args.listify(apply_configs) or None,
        build_config = build_config,
        target_arch = target_arch,
        target_bits = target_bits,
        target_platform = target_platform,
        target_cros_boards = args.listify(target_cros_boards) or None,
        cros_boards_with_qemu_images = args.listify(cros_boards_with_qemu_images) or None,
    )

def _android_config(*, config, apply_configs = None):
    """The details for configuring android recipe module.

    This uses the recipe engine's config item facility.

    Args:
        config: (str) The name of the recipe module config item to use.
        apply_configs: (list[str]|str) Additional configs to apply.

    Returns:
        A struct that can be passed to the `android_config` argument of
        `builder_spec`.
    """
    if not config:
        fail("config must be provided")
    return _struct_with_non_none_values(
        config = config,
        apply_configs = args.listify(apply_configs) or None,
    )

def _test_results_config(*, config):
    """The details for configuring test_results recipe module.

    This uses the recipe engine's config item facility.

    Args:
        config: (str) The name of the recipe module config item to use.

    Returns:
        A struct that can be passed to the `test_results_config` argument of
        `builder_spec`.
    """
    if not config:
        fail("config must be provided")
    return _struct_with_non_none_values(
        config = config,
    )

def _skylab_upload_location(*, gs_bucket, gs_extra = None):
    """The details for where tests are uploaded for skylab.

    Args:
        gs_bucket: (str) The Google Storage bucket to upload the tests to.
        gs_extra: (str) Additional URL components to add to the Google Storage
            URL.

    Returns:
        A struct that can be passed to the `skylab_upload_location` argument of
        `builder_spec`.
    """
    if not gs_bucket:
        fail("gs_bucket must be provided")
    return _struct_with_non_none_values(
        gs_bucket = gs_bucket,
        gs_extra = gs_extra,
    )

def _builder_spec(
        *,
        execution_mode = _execution_mode.COMPILE_AND_TEST,
        parent = None,
        gclient_config,
        chromium_config,
        android_config = None,
        test_results_config = None,
        android_version_file = None,
        clobber = None,
        build_gs_bucket = None,
        run_tests_serially = None,
        perf_isolate_upload = None,
        expose_trigger_properties = None,
        skylab_upload_location = None):
    """Details for configuring execution for a single builder.

    Args:
        execution_mode: (execution_mode) The execution mode of the builder.
        parent: (str) A string identifying the parent builder, will be added to
            the triggered_by value for the builder.
        gclient_config: (gclient_config) The gclient config for the builder.
        chromium_config: (chromium_config) The chromium config for the builder.
        android_config: (android_config) The android config for the builder.
        test_results_config: (test_results_config) The test_results config for
            the builder.
        android_version_file: (str) A path relative to the checkout to a file
            containing the Chrome version information for Android.
        clobber: (bool) Whether to have bot_update perform a clobber of any
            pre-existing build outputs.
        build_gs_bucket: (str) Name of a google storage bucket to use when using
            the legacy package transfer where build outputs are uploaded to
            Google Storage and then downloaded by the tester. This must be set
            for builders that trigger testers that run non-isolated tests.
        run_tests_serially: (bool) Whether swarming tests should be run
            serially. By default, swarming tests are run in parallel but it can
            be useful to run tests in series if there is limited hardware for
            the tests.
        perf_isolate_upload: (bool) Whether or not an isolate is uploaded to the
            perf dashboard.
        expose_trigger_properties: (bool) Whether or not properties set on
            triggers should be exposed. If true, the 'trigger_properties' output
            property will be present on the build.  It will contain the
            properties normally set when triggering subsequent builds, which
            includes the isolate digests, the digest of a file containing the
            command lines for each isolate to execute, and the cwd of the
            checkout. This will only do something if the build actually produces
            isolates. This also only works on CI builders. This is normally not
            necessary. Builders only need to archive command lines if another
            build will need to use them. The chromium recipe automatically does
            this if your build triggers another build using the chromium recipe.
            Only set this value if something other than a triggered chromium
            builder needs to use the isolates created during a build execution.
        skylab_upload_location: (skylab_upload_location) The location to upload
            tests when using the lacros on skylab pipeline. This must be set if
            the builder triggers tests on skylab.

    Returns:
        A builder spec struct that can be passed to builder to set the builder
        spec to be used for the builder.
    """
    if execution_mode not in _execution_mode._values:
        fail("unknown execution_mode: {}".format(execution_mode))
    if execution_mode == _execution_mode.COMPILE_AND_TEST and parent != None:
        fail("parent cannot be provided for execution mode {}"
            .format(_execution_mode.COMPILE_AND_TEST))
    elif execution_mode == _execution_mode.TEST and parent == None:
        fail("parent must be specified for execution mode {}"
            .format(_execution_mode.TEST))
    if not gclient_config:
        fail("gclient_config must be provided")
    if not chromium_config:
        fail("chromium_config must be provided")

    return _struct_with_non_none_values(
        execution_mode = execution_mode,
        parent = parent,
        gclient_config = gclient_config,
        chromium_config = chromium_config,
        android_config = android_config,
        test_results_config = test_results_config,
        android_version_file = android_version_file,
        clobber = clobber,
        build_gs_bucket = build_gs_bucket,
        run_tests_serially = run_tests_serially,
        perf_isolate_upload = perf_isolate_upload,
        expose_trigger_properties = expose_trigger_properties,
        skylab_upload_location = skylab_upload_location,
    )

_rts_condition = _enum(
    NEVER = "NEVER",
    QUICK_RUN_ONLY = "QUICK_RUN_ONLY",
    ALWAYS = "ALWAYS",
)

def _rts_config(*, condition, recall = None):
    """The details for applying RTS for the builder.

    RTS (regression test selection) is an algorithm that trades off accuracy
    against speed by skipping tests that are less likely to provide a useful
    signal. See http://bit.ly/chromium-rts for more information.

    Args:
        condition: (rts_condition) When the RTS algorithm should be applied for
            builds of the builder.
        recall: (float) The recall level to use for the RTS algorithm.
    """
    if condition not in _rts_condition._values:
        fail("unknown RTS condition: {}".format(condition))
    return _struct_with_non_none_values(
        condition = condition,
        recall = recall,
    )

def _try_settings(
        *,
        include_all_triggered_testers = False,
        is_compile_only = None,
        analyze_names = None,
        retry_failed_shards = None,
        retry_without_patch = None,
        rts_config = None):
    """Settings specific to try builders.

    Args:
        include_all_triggered_testers: (bool) If true, any testers that are
            triggered by the builders that are mirrored will also be mirrored.
        is_compile_only: (bool) If true, any configured compile targets or tests
            will be compiled, but not tests will be triggered.
        analyze_names: (list[str]|str) Additional names to analyze in the build.
        retry_failed_shards: (bool) Whether or not failing shards of a test will
            be retried. If retries for all failed shards of a test succeed, the
            test will be considered to have passed.
        retry_without_patch: (bool) Whether or not failing tests will be retried
            without the patch applied. If the retry for a test fails, the test
            will be considered to have passed.
        rts_config: (rts_config) The rts_config object for the builder.

    Returns:
        A struct that can be passed to the `try_settings` argument of the
        builder.
    """
    return _struct_with_non_none_values(
        include_all_triggered_testers = include_all_triggered_testers,
        is_compile_only = is_compile_only,
        analyze_names = analyze_names,
        retry_failed_shards = retry_failed_shards,
        retry_without_patch = retry_without_patch,
        rts_config = rts_config,
    )

builder_config = struct(
    # Function and associated constants for defining builder spec
    builder_spec = _builder_spec,
    execution_mode = _execution_mode,
    skylab_upload_location = _skylab_upload_location,

    # Function for defining gclient recipe module config
    gclient_config = _gclient_config,

    # Function and associated constants for defining chromium recipe module
    # config
    chromium_config = _chromium_config,
    build_config = _build_config,
    target_arch = _target_arch,
    target_platform = _target_platform,

    # Function for defining android recipe module config
    android_config = _android_config,

    # Function for defining test_results recipe module config
    test_results_config = _test_results_config,

    # Function for defining try-specific settings
    try_settings = _try_settings,
    rts_config = _rts_config,
    rts_condition = _rts_condition,
)

# Internal details =============================================================

# Nodes containing the builder config details for a builder
_BUILDER_CONFIG = nodes.create_node_type_with_builder_ref("builder_config")

# Nodes representing a link to a parent builder
_BUILDER_CONFIG_PARENT = nodes.create_link_node_type(
    "builder_config_parent",
    _BUILDER_CONFIG,
    _BUILDER_CONFIG,
)

# Nodes representing a link to a mirrored builder
_BUILDER_CONFIG_MIRROR = nodes.create_link_node_type(
    "builder_config_mirror",
    _BUILDER_CONFIG,
    _BUILDER_CONFIG,
)

def _struct_to_dict(obj):
    return json.decode(json.encode(obj))

def register_builder_config(bucket, name, builder_group, builder_spec, mirrors, try_settings):
    """Registers the builder config so the properties can be computed.

    At most one of builder_spec or mirrors can be set. If neither builder_spec
    nor mirrors are set, nothing is registered.

    Args:
        bucket: The name of the bucket the builder belongs to.
        name: The name of the builder.
        builder_group: The name of the group the builder belongs to.
        builder_spec: The spec describing the configuration for the builder.
        mirrors: References to the builders that the builder should mirror.
        try_settings: The object determining the try-specific settings.
    """
    if not builder_spec and not mirrors:
        if try_settings:
            fail("try_settings specified without builder_spec or mirrors")

        # TODO(gbeaty) Eventually make this a failure for the chromium
        # family of recipes
        return

    if not builder_group:
        fail("builder_group must be set to use chromium_tests_builder_config")
    if builder_spec and mirrors:
        fail("only one of builder_spec or mirrors can be set")

    if try_settings:
        try_settings = _struct_to_dict(try_settings)
        include_all_triggered_testers = try_settings.pop("include_all_triggered_testers")
    else:
        include_all_triggered_testers = not mirrors
    builder_config_key = _BUILDER_CONFIG.add(bucket, name, props = dict(
        bucket = bucket,
        name = name,
        builder_group = builder_group,
        builder_spec = _struct_to_dict(builder_spec),
        mirrors = mirrors,
        include_all_triggered_testers = include_all_triggered_testers,
        try_settings = try_settings,
    ))

    parent = getattr(builder_spec, "parent", None)
    if parent:
        _BUILDER_CONFIG_PARENT.link(builder_config_key, parent)

    for m in mirrors or []:
        _BUILDER_CONFIG_MIRROR.link(builder_config_key, m)

def _builder_name(node):
    return "{}/{}".format(node.props.bucket, node.props.name)

def _get_parent_node(node):
    nodes = _BUILDER_CONFIG_PARENT.children(node.key)

    execution_mode = node.props.builder_spec["execution_mode"]

    if not nodes:
        if execution_mode == _execution_mode.TEST:
            fail("internal error: builder {} has execution_mode {} and has no parent"
                .format(_builder_name(node), execution_mode))
        return None

    if len(nodes) > 1:
        fail("internal error: builder {} has multiple parents: {}"
            .format(_builder_name(node), sorted([_builder_name(t) for t in nodes])))
    if execution_mode != _execution_mode.TEST:
        fail("internal error: builder {} has execution_mode {} and has a parent: {}"
            .format(_builder_name(node), execution_mode, _builder_name(nodes[0])))

    return nodes[0]

def _get_child_nodes(node):
    nodes = _BUILDER_CONFIG_PARENT.parents(node.key)

    execution_mode = node.props.builder_spec["execution_mode"]

    if execution_mode != _execution_mode.COMPILE_AND_TEST and nodes:
        fail("internal error: builder {} has execution_mode {} and has children: {}"
            .format(_builder_name(node), execution_mode, sorted([_builder_name(n) for n in nodes])))

    return nodes

def _get_mirrored_builders(node):
    nodes = _BUILDER_CONFIG_MIRROR.children(node.key)

    for mirror in nodes:
        if not mirror.props.builder_spec:
            fail("builder {} mirrors builder {} which does not have a builder spec"
                .format(_builder_name(node), _builder_name(mirror)))

    return nodes

def _get_mirroring_builders(node):
    if not node.props.builder_spec:
        return []

    nodes = _BUILDER_CONFIG_MIRROR.parents(node.key)

    # If there are builders that mirror the parent of the current builder and
    # include all triggered testers, then they mirror the current builder also
    parent = _get_parent_node(node)
    if parent:
        for m in _BUILDER_CONFIG_MIRROR.parents(parent.key):
            if m.props.include_all_triggered_testers:
                nodes.append(m)

    return nodes

def _builder_id(node):
    return dict(
        # TODO(crbug.com/868153) Once the configs for all chromium builders are
        # migrated src-side, switch this to settings.project and remove the use
        # of project_trigger_override within the starlark
        project = "chromium",
        bucket = node.props.bucket,
        builder = node.props.name,
    )

def _entry(node, parent = None):
    builder_spec = dict(
        builder_group = node.props.builder_group,
        **node.props.builder_spec
    )
    for src, dst in (
        ("gclient_config", "legacy_gclient_config"),
        ("chromium_config", "legacy_chromium_config"),
        ("android_config", "legacy_android_config"),
        ("test_results_config", "legacy_test_results_config"),
    ):
        if src in builder_spec:
            builder_spec[dst] = builder_spec.pop(src)
    if parent:
        builder_spec["parent"] = _builder_id(parent)
    elif "parent" in builder_spec:
        fail("internal error: 'parent' is set in builder_spec for {}, but no parent node is provided"
            .format(_builder_name(node)))

    return dict(
        builder_id = _builder_id(node),
        builder_spec = builder_spec,
    )

def _builder_id_sort_key(builder_id):
    return (builder_id["bucket"], builder_id["builder"])

def _set_builder_config_property(ctx):
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break
    if cfg == None:
        fail("There is no buildbucket configuration file to update properties")

    for bucket in cfg.buckets:
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            builder_name = builder.name
            node = _BUILDER_CONFIG.get(bucket_name, builder_name)
            if not node:
                continue

            entries = []
            builder_ids = []
            builder_ids_in_scope_for_testing = []
            try_settings = dict(node.props.try_settings or {})

            if node.props.builder_spec:
                parent = _get_parent_node(node)
                if parent:
                    entries.append(_entry(parent))
                entries.append(_entry(node, parent))
                builder_ids.append(_builder_id(node))
                for child in _get_child_nodes(node):
                    entries.append(_entry(child, node))
                    builder_ids_in_scope_for_testing.append(_builder_id(child))
            else:
                mirrors = _get_mirrored_builders(node)

                encountered = {}

                def add(node, parent = None):
                    node_id = (node.props.bucket, node.props.name)
                    if node_id not in encountered:
                        entries.append(_entry(node, parent))
                        if node.props.builder_spec["execution_mode"] == _execution_mode.COMPILE_AND_TEST:
                            builder_ids.append(_builder_id(node))
                        else:
                            builder_ids_in_scope_for_testing.append(_builder_id(node))
                        encountered[node_id] = True

                for m in mirrors:
                    parent = _get_parent_node(m)
                    if parent:
                        add(parent)
                    add(m, parent)
                    if node.props.include_all_triggered_testers:
                        for child in _get_child_nodes(m):
                            add(child, m)

            if not entries:
                fail("internal error: entries is empty for builder {}"
                    .format(_builder_name(node)))
            if not builder_ids:
                fail("internal error: builder_ids is empty for builder {}"
                    .format(_builder_name(node)))

            builder_config = dict(
                builder_db = dict(
                    entries = sorted(entries, key = lambda e: _builder_id_sort_key(e["builder_id"])),
                ),
                builder_ids = sorted(builder_ids, key = _builder_id_sort_key),
                **try_settings
            )

            if builder_ids_in_scope_for_testing:
                builder_config["builder_ids_in_scope_for_testing"] = (
                    sorted(builder_ids_in_scope_for_testing, key = _builder_id_sort_key)
                )

            mirroring_builders = _get_mirroring_builders(node)
            if mirroring_builders:
                builder_config["mirroring_builder_group_and_names"] = [
                    dict(group = group, builder = builder)
                    for group, builder in sorted([(b.props.builder_group, b.props.name) for b in mirroring_builders])
                ]

            builder_properties = json.decode(builder.properties)
            builder_properties["$build/chromium_tests_builder_config"] = dict(
                builder_config = builder_config,
            )
            builder.properties = json.encode(builder_properties)

lucicfg.generator(_set_builder_config_property)
