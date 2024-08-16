# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining chromium_tests_builder_config properties."""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys", "kinds", "triggerer")
load("//project.star", "settings")
load("./args.star", "args")
load(
    "./builder_exemptions.star",
    "mega_cq_excluded_builders",
    "mega_cq_excluded_gardener_rotations",
    "standalone_trybot_excluded_builder_groups",
    "standalone_trybot_excluded_builders",
)
load("./chrome_settings.star", "per_builder_outputs_config")
load("./enums.star", "enums")
load("./html.star", "linkify_builder")
load("./nodes.star", "nodes")
load("./sheriff_rotations.star", "get_gardener_rotations")
load("./structs.star", "structs")
load("./targets-internal/targets-specs-generation.star", "get_targets_spec_generator", "register_targets")

_execution_mode = enums.enum(
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
    return struct(
        config = config,
        apply_configs = args.listify(apply_configs),
    )

_build_config = enums.enum(
    RELEASE = "Release",
    DEBUG = "Debug",
)

_target_arch = enums.enum(
    INTEL = "intel",
    ARM = "arm",
    MIPS = "mips",
    MIPSEL = "mipsel",
)

_target_platform = enums.enum(
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
        target_platform,
        apply_configs = None,
        build_config = None,
        target_arch = None,
        target_bits = None,
        target_cros_boards = None,
        cros_boards_with_qemu_images = None):
    """The details for configuring chromium recipe module.

    This uses the recipe engine's config item facility.

    Args:
        config: (str) The name of the recipe module config item to use.
        target_platform: (target_platform) The target platform to build for.
        apply_configs: (list[str]|str) Additional configs to apply.
        build_config: (build_config) The build config value to use.
        target_arch: (target_arch) The target architecture to build for.
        target_bits: (int) The target bit count to build for.
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
    if build_config != None and build_config not in _build_config.values:
        fail("unknown build_config: {}".format(build_config))
    if target_arch != None and target_arch not in _target_arch.values:
        fail("unknown target_arch: {}".format(target_arch))
    if target_bits != None and target_bits not in (32, 64):
        fail("unknown target_bits: {}".format(target_bits))
    if target_platform not in _target_platform.values:
        fail("unknown target_platform: {}".format(target_platform))
    if ((target_cros_boards or cros_boards_with_qemu_images) and
        target_platform != _target_platform.CHROMEOS):
        fail("CrOS boards can only be specified for target platform '{}'"
            .format(_target_platform.CHROMEOS))

    return struct(
        config = config,
        apply_configs = args.listify(apply_configs),
        build_config = build_config,
        target_arch = target_arch,
        target_bits = target_bits,
        target_platform = target_platform,
        target_cros_boards = args.listify(target_cros_boards),
        cros_boards_with_qemu_images = args.listify(cros_boards_with_qemu_images),
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
    return struct(
        config = config,
        apply_configs = args.listify(apply_configs),
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
    return struct(
        gs_bucket = gs_bucket,
        gs_extra = gs_extra,
    )

def _clusterfuzz_archive(
        *,
        gs_bucket,
        archive_name_prefix,
        gs_acl = None,
        archive_subdir = None):
    """The details for configuring clusterfuzz archiving.

    Args:
        gs_bucket: (str) The name of the Google Cloud Storage bucket to upload
            the archive to.
        archive_name_prefix: (str) The prefix of the archive's name. The name of
            the archive will contain additional details such as platform and
            target among others.
        gs_acl: (str) The name of a Google Cloud Storage canned ACL to apply to
            the uploaded archive.
        archive_subdir: (str) An optional additional subdirectory within the
            platform/target directory to upload the archive to.
    """
    if not gs_bucket:
        fail("gs_bucket must be provided")
    if not archive_name_prefix:
        fail("archive_name_prefix must be provided")
    return struct(
        gs_bucket = gs_bucket,
        gs_acl = gs_acl,
        archive_name_prefix = archive_name_prefix,
        archive_subdir = archive_subdir,
    )

def _bisect_archive(
        *,
        gs_bucket,
        archive_subdir = None):
    """The details for configuring bisect archiving.

    Args:
        gs_bucket: (str) The name of the Google Cloud Storage bucket to upload
            the archive to.
        archive_subdir: (str) An optional additional subdirectory within the
            platform/target directory to upload the archive to.
    """
    if not gs_bucket:
        fail("gs_bucket must be provided")
    return struct(
        gs_bucket = gs_bucket,
        archive_subdir = archive_subdir,
    )

def _builder_spec(
        *,
        gclient_config,
        chromium_config,
        execution_mode = _execution_mode.COMPILE_AND_TEST,
        android_config = None,
        android_version_file = None,
        clobber = None,
        build_gs_bucket = None,
        run_tests_serially = None,
        perf_isolate_upload = None,
        expose_trigger_properties = None,
        skylab_upload_location = None,
        clusterfuzz_archive = None,
        bisect_archive = None):
    """Details for configuring execution for a single builder.

    Args:
        gclient_config: (gclient_config) The gclient config for the builder.
        chromium_config: (chromium_config) The chromium config for the builder.
        execution_mode: (execution_mode) The execution mode of the builder.
        android_config: (android_config) The android config for the builder.
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
        clusterfuzz_archive: (clusterfuzz_archive) The details of archiving for
            clusterfuzz.
        bisect_archive: (bisect_archive) The details of archiving for bisection.

    Returns:
        A builder spec struct that can be passed to builder to set the builder
        spec to be used for the builder.
    """
    if execution_mode not in _execution_mode.values:
        fail("unknown execution_mode: {}".format(execution_mode))
    if not gclient_config:
        fail("gclient_config must be provided")
    if not chromium_config:
        fail("chromium_config must be provided")

    return struct(
        execution_mode = execution_mode,
        gclient_config = gclient_config,
        chromium_config = chromium_config,
        android_config = android_config,
        android_version_file = android_version_file,
        clobber = clobber,
        build_gs_bucket = build_gs_bucket,
        run_tests_serially = run_tests_serially,
        perf_isolate_upload = perf_isolate_upload,
        expose_trigger_properties = expose_trigger_properties,
        skylab_upload_location = skylab_upload_location,
        clusterfuzz_archive = clusterfuzz_archive,
        bisect_archive = bisect_archive,
    )

def _ci_settings(
        *,
        retry_failed_shards = None):
    """Settings specific to CI builders.

    Args:
        retry_failed_shards: (bool) Whether or not failing shards of a test will
            be retried. If retries for all failed shards of a test succeed, the
            test will be considered to have passed.

    Returns:
        A struct that can be passed to the `ci_settings` argument of the builder.
    """
    return struct(
        retry_failed_shards = retry_failed_shards,
    )

def _try_settings(
        *,
        include_all_triggered_testers = False,
        is_compile_only = None,
        analyze_names = None,
        retry_failed_shards = None,
        retry_without_patch = None):
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

    Returns:
        A struct that can be passed to the `try_settings` argument of the
        builder.
    """
    return struct(
        include_all_triggered_testers = include_all_triggered_testers,
        is_compile_only = is_compile_only,
        analyze_names = analyze_names,
        retry_failed_shards = retry_failed_shards,
        retry_without_patch = retry_without_patch,
    )

def _is_copy_from(obj):
    # register_builder_config and the generator use the presence/absence of this
    # attribute to distinguish builders specifying their own spec/mirrors and
    # builders copying from another
    return hasattr(obj, "__copy_from__")

def _copy_from(builder, modifier_fn = None):
    """Details for specifying spec/mirrors in terms of another builder.

    Args:
        builder: (str) The name of another builder to copy from. The name can be
            a simple name if it unambigously refers to another builder.
        modifier_fn: (func(T) -> T) An optional function that can be used to
            modify the spec/mirrors used by the builder. If provided, the
            function will be called with the other builder's spec and should
            return the value to be used for the builder that is using
            `copy_from`. See //lib/structs.star for functions to enable
            returning a modified spec.
    """
    return struct(
        # register_builder_config and the generator use the presence/absence of
        # this attribute to distinguish builders specifying their own
        # spec/mirrors and builders copying from another
        __copy_from__ = "__copy_from__",
        builder = builder,
        modifier_fn = modifier_fn,
    )

builder_config = struct(
    # Function for expressing builder spec or mirrors in terms of another
    # builder's
    copy_from = _copy_from,

    # Functions and associated constants for defining builder spec
    builder_spec = _builder_spec,
    execution_mode = _execution_mode,
    skylab_upload_location = _skylab_upload_location,
    clusterfuzz_archive = _clusterfuzz_archive,
    bisect_archive = _bisect_archive,

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

    # Function for defining try-specific settings
    ci_settings = _ci_settings,
    try_settings = _try_settings,
)

# Internal details =============================================================

# Nodes containing the builder config details for a builder
_BUILDER_CONFIG = nodes.create_node_type_with_builder_ref("builder_config")

# Nodes representing a link to a mirrored builder
_BUILDER_CONFIG_MIRROR = nodes.create_link_node_type(
    "builder_config_mirror",
    _BUILDER_CONFIG,
    _BUILDER_CONFIG,
)

_BUILDER_SPEC_COPY_FROM = nodes.create_link_node_type(
    "builder_spec_copy_from",
    _BUILDER_CONFIG,
    _BUILDER_CONFIG,
)

_MIRRORS_COPY_FROM = nodes.create_link_node_type(
    "mirrors_copy_from",
    _BUILDER_CONFIG,
    _BUILDER_CONFIG,
)

def register_builder_config(
        bucket,
        name,
        builder_group,
        builder_spec,
        mirrors,
        bc_settings,
        targets,
        targets_settings,
        additional_exclusions,
        description_html):
    """Registers the builder config so the properties can be computed.

    At most one of builder_spec or mirrors can be set. If neither builder_spec
    nor mirrors are set, nothing is registered.

    Args:
        bucket: The name of the bucket the builder belongs to.
        name: The name of the builder.
        builder_group: The name of the group the builder belongs to.
        builder_spec: The spec describing the configuration for the builder.
        mirrors: References to the builders that the builder should mirror.
        bc_settings: The object determining the additional settings applied to
            builder_config.
        targets: The targets to be built/run by the builder.
        targets_settings: The settings to use when expanding the targets for the
            builder.
        additional_exclusions: A list of paths that are excluded when analyzing
            the change to determine affected targets. The paths should be
            relative to the per-builder output root dir.
        description_html: A string of html representing the description of the builder.
    """
    if not builder_spec and not mirrors:
        if bc_settings and settings.project.startswith("chrome"):
            fail("bc_settings specified without builder_spec or mirrors")

        # TODO(gbeaty) Eventually make this a failure for the chromium
        # family of recipes
        return

    if not builder_group:
        fail("builder_group must be set to use chromium_tests_builder_config")
    if builder_spec and mirrors:
        fail("only one of builder_spec or mirrors can be set")
    if targets and not builder_spec:
        fail("builder_spec must be set to set targets")

    include_all_triggered_testers = None
    settings_fields = {}
    if bc_settings:
        settings_fields = structs.to_proto_properties(bc_settings)
        include_all_triggered_testers = settings_fields.pop(
            "include_all_triggered_testers",
            None,
        )
    if include_all_triggered_testers == None:
        include_all_triggered_testers = not mirrors

    builder_config_key = _BUILDER_CONFIG.add(bucket, name, props = dict(
        builder_group = builder_group,
        builder_spec = builder_spec,
        mirrors = mirrors,
        include_all_triggered_testers = include_all_triggered_testers,
        settings_fields = settings_fields,
        additional_exclusions = additional_exclusions,
        description_html = description_html,
    ))

    if _is_copy_from(builder_spec):
        _BUILDER_SPEC_COPY_FROM.link(builder_config_key, builder_spec.builder)

    if _is_copy_from(mirrors):
        _MIRRORS_COPY_FROM.link(builder_config_key, mirrors.builder)
    else:
        for m in mirrors or []:
            _BUILDER_CONFIG_MIRROR.link(builder_config_key, m)

    if targets and settings.project.startswith("chrome"):
        fail("Defining targets in starlark is not yet supported in src-internal")

    if targets:
        # Register the bundle under the qualified builder name, this allows for
        # explicitly setting a builder's targets to be another builder's with
        # some modifications
        register_targets(
            name = "{}/{}".format(bucket, name),
            targets = targets,
            settings = targets_settings,
            parent_key = builder_config_key,
            builder_group = builder_group,
            builder_name = name,
        )

    graph.add_edge(builder_config_key, keys.builder(bucket, name))

def _builder_name(node):
    key = node.key
    container = key.container
    if not container or container.kind != kinds.BUCKET:
        fail("got {}, expecting a node with a bucket-scoped key".format(node))
    return "{}/{}".format(container.id, key.id)

def _get_mirroring_nodes(bc_state, node):
    nodes = []

    for mirroring in _BUILDER_CONFIG_MIRROR.parents(node.key):
        nodes.append(mirroring)
        for copying in _MIRRORS_COPY_FROM.parents(mirroring.key):
            if node in bc_state.mirrors(copying):
                nodes.append(copying)

    return nodes

def _get_mirroring_builders(bc_state, node):
    if not bc_state.builder_spec(node):
        return []

    nodes = _get_mirroring_nodes(bc_state, node)

    # If there are builders that mirror the parent of the current builder and
    # include all triggered testers, then they mirror the current builder also
    parent = bc_state.parent(node)
    if parent:
        for m in _get_mirroring_nodes(bc_state, parent):
            if m.props.include_all_triggered_testers:
                nodes.append(m)

    return nodes

def _builder_id(node):
    return dict(
        project = settings.project,
        bucket = node.key.container.id,
        builder = node.key.id,
    )

def _entry(bc_state, node, parent = None):
    builder_spec = dict(
        builder_group = node.props.builder_group,
        **structs.to_proto_properties(bc_state.builder_spec(node))
    )
    for src, dst in (
        ("gclient_config", "legacy_gclient_config"),
        ("chromium_config", "legacy_chromium_config"),
        ("android_config", "legacy_android_config"),
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

# Some fields don't need to be consistent between mirrored specs, either because
# they're only used on CI codepaths or because they're used on a per-spec basis
def _filter_spec_for_consistency(spec):
    spec = dict(spec)
    for a in (
        # Only used in CI code-paths
        "build_gs_bucket",
        "run_tests_serially",
        "expose_trigger_properties",
        # Used on a per-spec basis to look up tests for mirrored builders
        "builder_group",
    ):
        spec.pop(a, None)
    return spec

def _check_specs_for_consistency(bucket_name, builder_name, entries):
    filtered_specs = [
        (e["builder_id"], _filter_spec_for_consistency(e["builder_spec"]))
        for e in entries
    ]
    spec = filtered_specs[0][1]
    for _, s in filtered_specs[1:]:
        if s != spec:
            failure_output = []
            for b, s in filtered_specs:
                failure_output.extend("{}/{}: {}".format(
                    b["bucket"],
                    b["builder"],
                    json.indent(json.encode(s)),
                ).splitlines())
            fail("Builder {}/{} mirrors builders with inconsistent builder specs (omitting fields that do not need to be consistent):{}".format(
                bucket_name,
                builder_name,
                "".join(
                    ["\n  {}".format(o) for o in failure_output],
                ),
            ))

def _get_builder_mirror_description(bucket_name, builder, bc_state):
    node = _BUILDER_CONFIG.get(bucket_name, builder.name)
    if not node:
        return builder.description_html
    mirrored_builders = bc_state.mirrors(node)
    mirroring_builders = _get_mirroring_builders(bc_state, node)
    if not mirrored_builders and not mirroring_builders:
        return builder.description_html
    elif mirrored_builders and mirroring_builders:
        # Need to change the descriptions below if this assertion no
        # longer holds true.
        fail("A builder can't both mirror and be mirrored:", builder.name)

    description = builder.description_html
    if not description and len(mirrored_builders) == 1:
        m_id = _builder_id(mirrored_builders[0])
        mirror_node = _BUILDER_CONFIG.get(m_id["bucket"], m_id["builder"])
        if mirror_node.props.description_html:
            description += "<br>%s<br/>" % mirror_node.props.description_html
    if description:
        description += "<br/>"
    if mirrored_builders:
        description += "This builder mirrors the following CI builders:<br/>"
        if bucket_name not in ("finch", "try"):
            fail("Error with {}: only builders in bucket 'try' and 'finch' can mirror other builders".format(bucket_name))
    else:
        description += "This builder is mirrored by any of the following try builders:<br/>"
        if bucket_name != "ci":
            fail("Error with {}: only builders in buckets 'ci' can be mirrored".format(bucket_name))

    description += "<ul>"
    m_ids = [_builder_id(m) for m in mirrored_builders or mirroring_builders]
    for m_id in sorted(m_ids, key = _builder_id_sort_key):
        link = linkify_builder(m_id["bucket"], m_id["builder"])
        description += "<li>%s</li>" % link
    return description + "</ul>"

def _targets_spec(bc_state, nodes):
    # We only want to return a non-None value if one of the nodes actually
    # specifies targets
    valid = False

    targets_spec = {}

    for n in nodes:
        # Make sure that for each builder group, we generate a file, even if
        # there's no builder with targets defined. The recipe is going to try
        # and open a file for each builder group, so it must exist.
        group_dict = targets_spec.setdefault(n.props.builder_group, {})
        targets_spec_for_n = bc_state.targets_spec(n)
        if targets_spec_for_n != None:
            group_dict[n.key.id] = targets_spec_for_n
            valid = True

    if not valid:
        return None
    return targets_spec

def _set_builder_config_property(ctx):
    cfg = None
    for f in ctx.output:
        if f.startswith("luci/cr-buildbucket"):
            cfg = ctx.output[f]
            break
    if cfg == None:
        fail("There is no buildbucket configuration file to update properties")

    bc_state = _bc_state()
    needs_mega_cq_mode = set()

    for bucket in cfg.buckets:
        if not proto.has(bucket, "swarming"):
            continue
        bucket_name = bucket.name
        for builder in bucket.swarming.builders:
            builder_name = builder.name
            node = _BUILDER_CONFIG.get(bucket_name, builder_name)
            if not node:
                continue

            entries = []
            builder_ids = []
            builder_ids_in_scope_for_testing = []
            targets_spec_nodes = []
            mirrors = []

            builder_spec = bc_state.builder_spec(node)
            if builder_spec:
                parent = bc_state.parent(node)
                if parent:
                    entries.append(_entry(bc_state, parent))
                entries.append(_entry(bc_state, node, parent))
                targets_spec_nodes.append(node)
                builder_ids.append(_builder_id(node))
                for child in bc_state.children(node):
                    entries.append(_entry(bc_state, child, node))
                    targets_spec_nodes.append(child)
                    builder_ids_in_scope_for_testing.append(_builder_id(child))
            else:
                mirrors = bc_state.mirrors(node)

                encountered = {}

                entries_to_check_for_consistency = []

                def add(node, parent = None):
                    node_id = (node.key.container.id, node.key.id)
                    if node_id not in encountered:
                        entry = _entry(bc_state, node, parent)
                        entries.append(entry)
                        targets_spec_nodes.append(node)
                        if bc_state.builder_spec(node).execution_mode == _execution_mode.COMPILE_AND_TEST:
                            builder_id = _builder_id(node)
                            builder_ids.append(builder_id)
                            entries_to_check_for_consistency.append(entry)
                        else:
                            builder_ids_in_scope_for_testing.append(_builder_id(node))
                        encountered[node_id] = True

                for m in mirrors:
                    parent = bc_state.parent(m)
                    if parent:
                        add(parent)
                    add(m, parent)
                    if node.props.include_all_triggered_testers:
                        for child in bc_state.children(m):
                            add(child, m)

                if not entries_to_check_for_consistency:
                    fail("{}/{}".format(bucket_name, builder_name))

                _check_specs_for_consistency(bucket_name, builder_name, entries_to_check_for_consistency)

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
                **node.props.settings_fields
            )
            if node.props.additional_exclusions:
                builder_config["additional_exclusions"] = [
                    "infra/config/generated/{}/{}".format(
                        per_builder_outputs_config().root_dir,
                        exclusion,
                    )
                    for exclusion in node.props.additional_exclusions
                ]

            if builder_ids_in_scope_for_testing:
                builder_config["builder_ids_in_scope_for_testing"] = (
                    sorted(builder_ids_in_scope_for_testing, key = _builder_id_sort_key)
                )

            mirroring_builders = _get_mirroring_builders(bc_state, node)
            if mirroring_builders:
                builder_config["mirroring_builder_group_and_names"] = [
                    dict(group = group, builder = builder)
                    for group, builder in sorted([(b.props.builder_group, b.key.id) for b in mirroring_builders])
                ]

            targets_spec = _targets_spec(bc_state, targets_spec_nodes)
            if targets_spec:
                builder_config["targets_spec_directory"] = "src/infra/config/generated/builders/{}/{}/targets".format(bucket_name, builder_name)
                for builder_group, contents in targets_spec.items():
                    json_file = "builders/{}/{}/targets/{}.json".format(bucket_name, builder_name, builder_group)
                    ctx.output[json_file] = json.indent(json.encode(contents), indent = "  ")

            builder_properties = json.decode(builder.properties)
            builder_properties["$build/chromium_tests_builder_config"] = dict(
                builder_config = builder_config,
            )
            builder.properties = json.encode(builder_properties)

            builder.description_html = _get_builder_mirror_description(bucket_name, builder, bc_state)

            # Enforce that most gardened CI bots have a matching trybot.
            rotations = get_gardener_rotations(bucket_name, builder.name)
            is_excluded = (
                bucket_name != "ci" or
                builder.name in mega_cq_excluded_builders or
                any([s.key.id in mega_cq_excluded_gardener_rotations for s in rotations])
            )
            if rotations and not mirroring_builders and not is_excluded:
                fail("{} is on a sheriff/gardener rotation, but lacks a matching trybot".format(builder.name))

            if (bucket_name == "try" and not mirrors and
                builder_properties.get("builder_group") not in standalone_trybot_excluded_builder_groups and
                builder.name not in standalone_trybot_excluded_builders):
                fail(builder.name + " must not be a stand-alone trybot. Please add a corresponding CI bot for it to mirror.")

            # Put most gardened CI bots' trybots onto the mega CQ. We skip a
            # trybot if any of the following are true:
            # - It doesn't run a normal Chromium trybot recipe
            # - Any of its CI mirrors isn't gardened
            # - All of its CI mirrors are on an excluded rotation
            # The last two prevent trybots of un-gardened child testers
            # triggered by gardened parent builders from getting added to the
            # mega CQ.
            if not mirrors:
                continue
            allowed_trybot_recipes = [
                "chromium_trybot",
                "chromium_trybot_internal",
                "chromium/fuzz",
                "chromium/orchestrator",
            ]
            is_excluded = False
            all_mirror_rotations = []
            for m in mirrors:
                mirror_id = _builder_id(m)
                mirror_rotations = get_gardener_rotations(mirror_id["bucket"], mirror_id["builder"])
                all_mirror_rotations += mirror_rotations
                if not mirror_rotations:
                    is_excluded = True
                if builder_properties["recipe"] not in allowed_trybot_recipes:
                    is_excluded = True
                if mirror_id["builder"] in mega_cq_excluded_builders:
                    is_excluded = True
                if is_excluded:
                    break
            if all([r.key.id in mega_cq_excluded_gardener_rotations for r in all_mirror_rotations]):
                is_excluded = True
            if not is_excluded:
                cq_identifier = "{}/{}/{}".format(
                    settings.project,
                    bucket_name,
                    builder.name,
                )
                needs_mega_cq_mode = needs_mega_cq_mode.union([cq_identifier])

    cq_config_groups = []
    for f in ctx.output:
        if f == "luci/commit-queue.cfg":
            cq_config_groups = ctx.output[f].config_groups
            break
    for cq_group in cq_config_groups:
        if cq_group.name != "cq":
            continue
        for b in cq_group.verifiers.tryjob.builders:
            if b.name not in needs_mega_cq_mode:
                continue

            # TODO(crbug.com/40282038): Uncomment the following when CV actually
            # supports custom run modes.
            #if "CQ_MODE_MEGA_DRY_RUN" not in b.mode_allowlist:
            #    b.mode_allowlist.append("CQ_MODE_MEGA_DRY_RUN")
            #if "CQ_MODE_MEGA_FULL_RUN" not in b.mode_allowlist:
            #    b.mode_allowlist.append("CQ_MODE_MEGA_FULL_RUN")

    # Print the mega CQ bots to a txt file for debugging / parsing purposes.
    # TODO(crbug.com/40282038): Can delete this when CV full supports custom
    # run modes with all features needed by chrome.
    mega_cq_bots_file = "cq-usage/mega_cq_bots.txt"
    ctx.output[mega_cq_bots_file] = "".join(["{}\n".format(b) for b in sorted(needs_mega_cq_mode)])

lucicfg.generator(_set_builder_config_property)

# Capture the details of working with the graph in methods that use caching so
# that we're not repeatedly traversing the graph to compute the same information

def _node_cached(f):
    cache = {}

    def execute(node):
        if node not in cache:
            cache[node] = f(node)
        return cache[node]

    return execute

def _bc_state():
    def get_parent(node):
        if node.key.kind != _BUILDER_CONFIG.kind:
            fail("Expected {} node, got {}".format(_BUILDER_CONFIG.kind, node))

        builder_nodes = graph.children(node.key, kinds.BUILDER)
        if len(builder_nodes) != 1:
            fail(
                "internal error: builder_config node should have edge to exactly 1 builder node",
                node.trace,
            )

        # To find the builder config of the parent builder, we need to find the
        # builder that triggers the builder we're looking at, then the builder
        # config node will be the parent node of that builder.
        #
        # To find the parent builder, we traverse parent nodes of the builder
        # node. The builder node will have builder_ref nodes as parents, which
        # abstract being able to refer to a builder by bucket-qualified name
        # (ci/foo-builder) or simple name (foo-builder). The builder_ref nodes
        # will have triggerer nodes as parents, which abstract things that can
        # trigger builders (pollers or builders). Finally, the triggerer nodes
        # for builders will have a builder node as a parent.
        triggerers = set()
        parents = set()
        for ref in graph.parents(builder_nodes[0].key, kinds.BUILDER_REF):
            for t in graph.parents(ref.key, kinds.TRIGGERER):
                triggerers = triggerers.union([t])
                for b in graph.parents(t.key, kinds.BUILDER):
                    builder_configs = graph.parents(b.key, _BUILDER_CONFIG.kind)
                    if len(builder_configs) > 1:
                        fail(
                            "internal error: multiple builder_config parents for {}: {}"
                                .format(b, builder_configs),
                            b.trace,
                        )
                    parents = parents.union(builder_configs)

        if len(parents) > 1:
            fail("{} has multiple parents: {}".format(
                _builder_name(node),
                sorted([_builder_name(p) for p in parents]),
            ))

        parent = list(parents)[0] if parents else None

        execution_mode = bc_state.builder_spec(node).execution_mode

        if execution_mode == _execution_mode.TEST:
            if len(triggerers) > 1:
                fail(
                    "builder {} has multiple triggerers: {}"
                        .format(_builder_name(node), [t.key.id for t in triggerers]),
                    node.trace,
                )
            elif not triggerers:
                fail(
                    "builder {} has execution_mode {} and has no parent"
                        .format(_builder_name(node), execution_mode),
                    node.trace,
                )
            elif not parent:
                fail(
                    "builder {} is triggered by {} which does not have a builder spec"
                        .format(_builder_name(node), list(triggerers)[0]),
                    node.trace,
                )
        elif execution_mode == _execution_mode.COMPILE_AND_TEST:
            if parent:
                fail(
                    "builder {} has execution_mode {} and has a parent: {}"
                        .format(_builder_name(node), execution_mode, _builder_name(parent)),
                    node.trace,
                )

        return parent

    def get_children(node):
        if node.key.kind != _BUILDER_CONFIG.kind:
            fail("Expected {} node, got {}".format(_BUILDER_CONFIG.kind, node))

        builder_nodes = graph.children(node.key, kinds.BUILDER)
        if len(builder_nodes) != 1:
            fail(
                "internal error: builder_config node should have edge to exactly 1 builder node",
                node.trace,
            )

        children = set()
        for b in triggerer.targets(builder_nodes[0]):
            b_children = graph.parents(b.key, _BUILDER_CONFIG.kind)
            if not b_children:
                fail(
                    "{} is triggered by {}, but does not have a builder spec"
                        .format(_builder_name(b), _builder_name(node)),
                    b.trace,
                )
            if len(b_children) > 1:
                fail(
                    "internal error: builder node should be the target of exactly 1 edge from a builder_config node",
                    b.trace,
                )
            children = children.union(b_children)

        execution_mode = bc_state.builder_spec(node).execution_mode

        if execution_mode != _execution_mode.COMPILE_AND_TEST and children:
            fail(
                "internal error: builder {} has execution_mode {} and has children: {}"
                    .format(_builder_name(node), execution_mode, sorted([_builder_name(c) for c in children])),
                node.trace,
            )

        return children

    def builder_spec_getter():
        builder_specs_by_node = {}

        def get(node):
            if node not in builder_specs_by_node:
                builder_spec = node.props.builder_spec
                if not _is_copy_from(builder_spec):
                    builder_specs_by_node[node] = builder_spec
                    return builder_spec

                copy_froms = _BUILDER_SPEC_COPY_FROM.children(node.key)
                if len(copy_froms) != 1:
                    fail(
                        "internal error: there should be exactly one builder to copy spec from",
                        node.trace,
                    )
                copy_from = copy_froms[0]
                builder_spec_to_copy = copy_from.props.builder_spec
                if not builder_spec_to_copy:
                    fail(
                        "copying builder spec from builder that doesn't have one",
                        node.trace,
                    )
                if _is_copy_from(builder_spec_to_copy):
                    fail(
                        "cannot copy the builder spec from a builder that is copying another builder spec",
                        node.trace,
                    )
                builder_specs_by_node[copy_from] = builder_spec_to_copy

                modifier_fn = builder_spec.modifier_fn
                if modifier_fn:
                    builder_spec_to_copy = modifier_fn(builder_spec_to_copy)
                    if not builder_spec_to_copy:
                        fail(
                            "no builder spec returned from {}".format(modifier_fn),
                            node.trace,
                        )

                builder_specs_by_node[node] = builder_spec_to_copy

            return builder_specs_by_node[node]

        return get

    def _get_mirrored_builders(node):
        nodes = _BUILDER_CONFIG_MIRROR.children(node.key)

        for mirror in nodes:
            if not bc_state.builder_spec(mirror):
                fail("builder {} mirrors builder {} which does not have a builder spec"
                    .format(_builder_name(node), _builder_name(mirror)))

        return nodes

    def mirrors_getter():
        mirrors_by_node = {}

        def get(node):
            if node not in mirrors_by_node:
                mirrors = node.props.mirrors
                if not _is_copy_from(mirrors):
                    mirrors = _get_mirrored_builders(node)
                    mirrors_by_node[node] = mirrors
                    return mirrors

                copy_froms = _MIRRORS_COPY_FROM.children(node.key)
                if len(copy_froms) != 1:
                    fail(
                        "internal error: there should be exactly one builder to copy mirrors from",
                        node.trace,
                    )
                copy_from = copy_froms[0]
                mirrors_to_copy = copy_from.props.mirrors
                if not mirrors_to_copy:
                    fail(
                        "copying mirrors from builder that doesn't have any",
                        node.trace,
                    )
                if _is_copy_from(mirrors_to_copy):
                    fail(
                        "cannot copy the mirrors from a builder that is copying another mirrors",
                        node.trace,
                    )
                mirrors_to_copy = _get_mirrored_builders(copy_from)
                mirrors_by_node[copy_from] = mirrors_to_copy

                modifier_fn = mirrors.modifier_fn
                if modifier_fn:
                    mirrors_to_copy = modifier_fn(mirrors_to_copy)
                    if not mirrors_to_copy:
                        fail(
                            "no mirrors returned from {}".format(modifier_fn),
                            node.trace,
                        )

                mirrors_by_node[node] = mirrors_to_copy

            return mirrors_by_node[node]

        return get

    bc_state = struct(
        parent = _node_cached(get_parent),
        children = _node_cached(get_children),
        builder_spec = builder_spec_getter(),
        mirrors = mirrors_getter(),
        targets_spec = _node_cached(get_targets_spec_generator()),
    )

    return bc_state
