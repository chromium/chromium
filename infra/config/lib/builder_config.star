# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining chromium_tests_builder_config properties."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "kinds")
load("//project.star", "settings")

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
        apply_configs: (list[str]) Additional configs to apply.

    Returns:
        A struct that can be passed to the `gclient_config` argument of
        `builder_spec`.
    """
    if not config:
        fail("config must be provided")
    return _struct_with_non_none_values(
        config = config,
        apply_configs = apply_configs,
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
        apply_configs: (list[str]) Additional configs to apply.
        build_config: (build_config) The build config value to use.
        target_arch: (target_arch) The target architecture to build for.
        target_bits: (int) The target bit count to build for.
        target_platform: (target_platform) The target platform to build for.
        target_cros_boards: (list[str]) The CROS boards to target, SDKs will
            be downloaded for each board. Can only be specified if
            `target_platform` is `target_platform.CHROMEOS`.
        cros_boards_with_qemu_images: (list[str]) Same as `target_cros_boards`,
            but a VM image for the board will be downloaded as well. Can only be
            specified if `target_platform` is `target_platform.CHROMEOS`.

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
        apply_configs = apply_configs,
        build_config = build_config,
        target_arch = target_arch,
        target_bits = target_bits,
        target_platform = target_platform,
        target_cros_boards = target_cros_boards,
        cros_boards_with_qemu_images = cros_boards_with_qemu_images,
    )

def _android_config(*, config, apply_configs = None):
    """The details for configuring android recipe module.

    This uses the recipe engine's config item facility.

    Args:
        config: (str) The name of the recipe module config item to use.
        apply_configs: (list[str]) Additional configs to apply.

    Returns:
        A struct that can be passed to the `android_config` argument of
        `builder_spec`.
    """
    if not config:
        fail("config must be provided")
    return _struct_with_non_none_values(
        config = config,
        apply_configs = apply_configs,
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

# TODO(gbeaty) Expose this to be used with try_settings
def _rts_config(*, condition, recall = None):
    if condition not in _rts_condition._values:
        fail("unknown RTS condition: {}".format(condition))
    return _struct_with_non_none_values(
        condition = condition,
        recall = recall,
    )

# TODO(gbeaty) Expose this function and add support to the generator
def _try_settings(
        *,
        include_all_triggered_testers = False,
        is_compile_only = False,
        analyze_names = None,
        retry_failed_shards = True,
        retry_without_patch = True,
        rts_config = None):
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
)

# Internal details =============================================================

# The kind of the nodes containing the builder config details for a builder
_BUILDER_CONFIG_KIND = "builder_config"

def _builder_config_key(bucket, builder):
    return graph.key("@chromium", "", kinds.BUCKET, bucket, _BUILDER_CONFIG_KIND, builder)

# The kind of the nodes tracking references to other builders in the builder
# config details
#
# There will be multiple builder_config_ref nodes pointing at a single
# builder_config node to allow builders to be referred to either as
# <bucket>/<builder> or just <builder> if it is unambiguous. They will only be
# used via a parent, with the type of the parent corresponding to the purpose of
# the reference.
_BUILDER_CONFIG_REF_KIND = "builder_config_ref"

def _builder_config_ref_key(ref):
    chunks = ref.split(":", 1)
    if len(chunks) != 1:
        fail("reference to builder in external project '{}' is not allowed here"
            .format(chunks[0]))
    chunks = ref.split("/", 1)
    if len(chunks) == 1:
        return graph.key("@chromium", "", _BUILDER_CONFIG_REF_KIND, ref)
    return graph.key("@chromium", "", kinds.BUCKET, chunks[0], _BUILDER_CONFIG_REF_KIND, chunks[1])

# The kind of the nodes representing a link to a parent builder
#
# Will have a single builder_config_ref node as a child and a single
# builder_config node as a parent.
_BUILDER_CONFIG_PARENT_KIND = "builder_config_parent"

def _builder_config_parent_key(ref):
    return graph.key("@chromium", "", _BUILDER_CONFIG_PARENT_KIND, ref)

# The kind of the nodes representing a link to a mirror
#
# Will have a single builder_config_ref node as a child and a single
# builder_config node as a parent.
_BUILDER_CONFIG_MIRROR_KIND = "builder_config_mirror"

def _builder_config_mirror_key(ref):
    return graph.key("@chromium", "", _BUILDER_CONFIG_MIRROR_KIND, ref)

def _follow_builder_config_ref(ref_node, context_node):
    """Get the pointed-at builder config node for a builder config ref.

    Fails if the reference is ambiguous (i.e. 'ref_node' has more than one
    child). Such references can't be used to refer to a single builder.

    Args:
        ref_node: builder config ref node.
        context_node: Node where this ref is used, for error messages.

    Returns:
        builder config graph node.
    """
    if ref_node.key.kind != _BUILDER_CONFIG_REF_KIND:
        fail("{} is not builder_config ref".format(ref_node))

    variants = graph.children(ref_node.key, _BUILDER_CONFIG_KIND)
    if not variants:
        fail("{} is unexpectedly unconnected".format(ref_node))

    if len(variants) == 1:
        return variants[0]

    fail(
        "ambiguous reference '{}' in {}, possible variants:\n  {}".format(
            ref_node.key.id,
            context_node,
            "\n  ".join([str(v) for v in variants]),
        ),
        trace = context_node.trace,
    )

def _struct_to_dict(obj):
    return json.decode(json.encode(obj))

_ALLOW_LIST = (
    ("ci", "chromeos-arm-generic-rel"),
    ("ci", "linux-bootstrap"),
    ("ci", "linux-bootstrap-tests"),
    ("try", "chromeos-arm-generic-rel"),
    ("try", "linux-bootstrap"),
)

def register_builder_config(bucket, name, builder_group, builder_spec, mirrors):
    """Registers the builder config so the properties can be computed.

    At most one of builder_spec or mirrors can be set. If neither builder_spec
    nor mirrors are set, nothing is registered.

    Args:
        bucket: The name of the bucket the builder belongs to.
        name: The name of the builder.
        builder_group: The name of the group the builder belongs to.
        builder_spec: The spec describing the configuration for the builder.
        mirrors: References to the builders that the builder should mirror.
    """
    if not builder_spec and not mirrors:
        # TODO(gbeaty) Eventually make this a failure for the chromium
        # family of recipes
        return

    # TODO(gbeaty) Allow any builders to use builder config once no other
    # systems rely on the recipe-side config
    if (bucket, name) not in _ALLOW_LIST:
        fail("src-side builder config is not available for general use yet")

    if not builder_group:
        fail("builder_group must be set to use chromium_tests_builder_config")
    if builder_spec and mirrors:
        fail("only one of builder_spec or mirrors can be set")

    builder_config_key = _builder_config_key(bucket, name)
    graph.add_node(builder_config_key, props = dict(
        bucket = bucket,
        name = name,
        builder_group = builder_group,
        builder_spec = _struct_to_dict(builder_spec),
        mirrors = mirrors,
    ))
    for ref in (name, "{}/{}".format(bucket, name)):
        ref_key = _builder_config_ref_key(ref)
        graph.add_node(ref_key, idempotent = True)
        graph.add_edge(ref_key, builder_config_key)

    parent = getattr(builder_spec, "parent", None)
    if parent:
        parent_key = _builder_config_parent_key(parent)
        graph.add_node(parent_key, idempotent = True)
        graph.add_edge(builder_config_key, parent_key)
        graph.add_edge(parent_key, _builder_config_ref_key(parent))

    for m in mirrors or []:
        mirror_key = _builder_config_mirror_key(m)
        graph.add_node(mirror_key, idempotent = True)
        graph.add_edge(builder_config_key, mirror_key)
        graph.add_edge(mirror_key, _builder_config_ref_key(m))

def _builder_name(node):
    return "{}/{}".format(node.props.bucket, node.props.name)

def _get_parent_node(node):
    nodes = []
    for p in graph.children(node.key, _BUILDER_CONFIG_PARENT_KIND):
        for r in graph.children(p.key, _BUILDER_CONFIG_REF_KIND):
            nodes.append(_follow_builder_config_ref(r, node))

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
    nodes = []
    for r in graph.parents(node.key, _BUILDER_CONFIG_REF_KIND):
        for p in graph.parents(r.key, _BUILDER_CONFIG_PARENT_KIND):
            nodes.extend(graph.parents(p.key, _BUILDER_CONFIG_KIND))

    execution_mode = node.props.builder_spec["execution_mode"]

    if execution_mode != _execution_mode.COMPILE_AND_TEST and nodes:
        fail("internal error: builder {} has execution_mode {} and has children: {}"
            .format(_builder_name(node), execution_mode, sorted([_builder_name(n) for n in nodes])))

    return nodes

def _get_mirrored_builders(node):
    nodes = []
    for m in graph.children(node.key, _BUILDER_CONFIG_MIRROR_KIND):
        for r in graph.children(m.key, _BUILDER_CONFIG_REF_KIND):
            mirror = _follow_builder_config_ref(r, node)
            if not mirror.props.builder_spec:
                fail("builder {} mirrors builder {} which does not have a builder spec"
                    .format(_builder_name(node), _builder_name(mirror)))
            nodes.append(mirror)

    return nodes

def _get_mirroring_builders(node):
    nodes = []
    for r in graph.parents(node.key, _BUILDER_CONFIG_REF_KIND):
        for m in graph.parents(r.key, _BUILDER_CONFIG_MIRROR_KIND):
            nodes.extend(graph.parents(m.key, _BUILDER_CONFIG_KIND))

    return nodes

def _builder_id(node):
    return dict(
        project = settings.project,
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
            key = _builder_config_key(bucket_name, builder_name)
            node = graph.node(key)
            if not node:
                continue

            entries = []
            builder_ids = []
            builder_ids_in_scope_for_testing = []

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
            )

            if builder_ids_in_scope_for_testing:
                builder_config["builder_ids_in_scope_for_testing"] = (
                    sorted(builder_ids_in_scope_for_testing, key = _builder_id_sort_key)
                )

            mirroring_builders = _get_mirroring_builders(node)
            if mirroring_builders:
                builder_config["mirroring_builders"] = sorted([
                    dict(
                        group = b.props.builder_group,
                        builder = b.props.name,
                    )
                    for b in mirroring_builders
                ])

            builder_properties = json.decode(builder.properties)
            builder_properties["$build/chromium_tests_builder_config"] = dict(
                builder_config = builder_config,
            )
            builder.properties = json.encode(builder_properties)

lucicfg.generator(_set_builder_config_property)
