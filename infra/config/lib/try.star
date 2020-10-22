# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining try builders.

The `try_builder` function defined in this module enables defining a builder and
the tryjob verifier for it in the same location. It can also be accessed through
`try_.builder`.

The `tryjob` function specifies the verifier details for a builder. It can also
be accessed through `try_.job`.

The `defaults` struct provides module-level defaults for the arguments to
`try_builder`. The parameters that support module-level defaults have a
corresponding attribute on `defaults` that is a `lucicfg.var` that can be used
to set the default value. Can also be accessed through `try_.defaults`.
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./args.star", "args")
load("./branches.star", "branches")
load("./builders.star", "builders")

DEFAULT_EXCLUDE_REGEXPS = [
    # Contains documentation that doesn't affect the outputs
    ".+/[+]/docs/.+",
    # Contains configuration files that aren't active until after committed
    ".+/[+]/infra/config/.+",
]

defaults = args.defaults(
    extends = builders.defaults,
    add_to_list_view = False,
    cq_group = None,
    list_view = args.COMPUTE,
    main_list_view = None,
    subproject_list_view = None,
    resultdb_bigquery_exports = [],
)

def _sorted_list_view_graph_key(console_name):
    return graph.key("@chromium", "", "sorted_list_view", console_name)

def _sorted_list_view_impl(ctx, *, console_name):
    key = _sorted_list_view_graph_key(console_name)
    graph.add_node(key)
    graph.add_edge(keys.project(), key)
    return graph.keyset(key)

_sorted_list_view = lucicfg.rule(impl = _sorted_list_view_impl)

def _sort_console_entries(ctx):
    milo = ctx.output["luci-milo.cfg"]
    consoles = []
    for console in milo.consoles:
        if not console.builders:
            continue
        graph_key = _sorted_list_view_graph_key(console.id)
        node = graph.node(graph_key)
        if node:
            console.builders = sorted(console.builders, lambda b: b.name)
        consoles.append(console)

lucicfg.generator(_sort_console_entries)

def list_view(*, name, branch_selector = branches.MAIN, **kwargs):
    if not branches.matches(branch_selector):
        return

    luci.list_view(
        name = name,
        **kwargs
    )

    _sorted_list_view(
        console_name = name,
    )

def tryjob(
        *,
        disable_reuse = None,
        experiment_percentage = None,
        location_regexp = None,
        location_regexp_exclude = None,
        cancel_stale = None,
        add_default_excludes = True):
    """Specifies the details of a tryjob verifier.

    See https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/master/lucicfg/doc/README.md#luci.cq_tryjob_verifier
    for details on the most of the arguments.

    Arguments:
      add_default_excludes - A bool indicating whether to add exclude regexps
        for certain directories that would have no impact when building chromium
        with the patch applied (docs, config files that don't take effect until
        landing, etc., see DEFAULT_EXCLUDE_REGEXPS).

    Returns:
      A struct that can be passed to the `tryjob` argument of `try_.builder` to
      enable the builder for CQ.
    """
    if add_default_excludes:
        location_regexp_exclude = DEFAULT_EXCLUDE_REGEXPS + (location_regexp_exclude or [])
    return struct(
        disable_reuse = disable_reuse,
        experiment_percentage = experiment_percentage,
        location_regexp = location_regexp,
        location_regexp_exclude = location_regexp_exclude,
        cancel_stale = cancel_stale,
    )

def try_builder(
        *,
        name,
        branch_selector = branches.MAIN,
        add_to_list_view = args.DEFAULT,
        cq_group = args.DEFAULT,
        list_view = args.DEFAULT,
        main_list_view = args.DEFAULT,
        subproject_list_view = args.DEFAULT,
        tryjob = None,
        experiments = None,
        resultdb_bigquery_exports = args.DEFAULT,
        **kwargs):
    """Define a try builder.

    Arguments:
      name - name of the builder, will show up in UIs and logs. Required.
      branch_selector - A branch selector value controlling whether the
        builder definition is executed. See branches.star for more
        information.
      add_to_list_view - A bool indicating whether an entry should be
        created for the builder in the console identified by
        `list_view`. Supports a module-level default that defaults to
        False.
      cq_group - The CQ group to add the builder to. If tryjob is None, it will
        be added as includable_only.
      list_view - A string identifying the ID of the list view to
        add an entry to. Supports a module-level default that defaults to
        the group of the builder, if provided. An entry will be added
        only if `add_to_list_view` is True.
      main_console_view - A string identifying the ID of the main list
        view to add an entry to. Supports a module-level default that
        defaults to None. Note that `add_to_list_view` has no effect on
        adding an entry to the main list view.
      subproject_list_view - A string identifying the ID of the
        subproject list view to add an entry to. Suppoers a module-level
        default that defaults to None. Not that `add_to_list_view` has
        no effect on adding an entry to the subproject list view.
      tryjob - A struct containing the details of the tryjob verifier for the
        builder, obtained by calling the `tryjob` function.
      experiments - a dict of experiment name to the percentage chance (0-100)
        that it will apply to builds generated from this builder.
      resultdb_bigquery_exports - a list of resultdb.export_test_results(...)
        specifying additional parameters for exporting test results to BigQuery.
        Will always upload to the luci-resultdb.chromium.try_test_results table
        in addition to any tables specified by the list's elements.
    """
    if not branches.matches(branch_selector):
        return

    # Disable "chromium.resultdb.result_sink" on all try builders by default.
    experiments = experiments or {}
    experiments.setdefault("chromium.resultdb.result_sink", 0)

    merged_resultdb_bigquery_exports = [
        resultdb.export_test_results(
            bq_table = "luci-resultdb.chromium.try_test_results",
        ),
    ]
    merged_resultdb_bigquery_exports.extend(
        defaults.get_value(
            "resultdb_bigquery_exports",
            resultdb_bigquery_exports,
        ),
    )

    # Define the builder first so that any validation of luci.builder arguments
    # (e.g. bucket) occurs before we try to use it
    builders.builder(
        name = name,
        branch_selector = branch_selector,
        resultdb_bigquery_exports = merged_resultdb_bigquery_exports,
        experiments = experiments,
        **kwargs
    )

    bucket = defaults.get_value_from_kwargs("bucket", kwargs)
    builder = "{}/{}".format(bucket, name)
    cq_group = defaults.get_value("cq_group", cq_group)
    if tryjob != None:
        luci.cq_tryjob_verifier(
            builder = builder,
            cq_group = cq_group,
            disable_reuse = tryjob.disable_reuse,
            experiment_percentage = tryjob.experiment_percentage,
            location_regexp = tryjob.location_regexp,
            location_regexp_exclude = tryjob.location_regexp_exclude,
            cancel_stale = tryjob.cancel_stale,
        )
    else:
        # Allow CQ to trigger this builder if user opts in via CQ-Include-Trybots.
        luci.cq_tryjob_verifier(
            builder = builder,
            cq_group = cq_group,
            includable_only = True,
        )

    add_to_list_view = defaults.get_value("add_to_list_view", add_to_list_view)
    if add_to_list_view:
        list_view = defaults.get_value("list_view", list_view)
        if list_view == args.COMPUTE:
            list_view = defaults.get_value_from_kwargs("builder_group", kwargs)

        if list_view:
            add_to_list_view = defaults.get_value(
                "add_to_list_view",
                add_to_list_view,
            )

            luci.list_view_entry(
                builder = builder,
                list_view = list_view,
            )

    main_list_view = defaults.get_value("main_list_view", main_list_view)
    if main_list_view:
        luci.list_view_entry(
            builder = builder,
            list_view = main_list_view,
        )

    subproject_list_view = defaults.get_value("subproject_list_view", subproject_list_view)
    if subproject_list_view:
        luci.list_view_entry(
            builder = builder,
            list_view = subproject_list_view,
        )

def blink_builder(*, name, goma_backend = None, **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.blink",
        goma_backend = goma_backend,
        **kwargs
    )

def blink_mac_builder(
        *,
        name,
        os = builders.os.MAC_ANY,
        builderless = True,
        **kwargs):
    return blink_builder(
        name = name,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = os,
        builderless = builderless,
        ssd = True,
        **kwargs
    )

def chromium_builder(*, name, **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium",
        builderless = True,
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs
    )

def chromium_android_builder(*, name, **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.android",
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs
    )

def chromium_angle_builder(*, name, **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.angle",
        builderless = False,
        goma_backend = builders.goma.backend.RBE_PROD,
        goma_jobs = builders.goma.jobs.J150,
        service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        **kwargs
    )

def chromium_chromiumos_builder(*, name, **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.chromiumos",
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs
    )

def chromium_dawn_builder(*, name, **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.dawn",
        builderless = False,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        **kwargs
    )

def chromium_linux_builder(*, name, goma_backend = builders.goma.backend.RBE_PROD, **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.linux",
        goma_backend = goma_backend,
        **kwargs
    )

def chromium_mac_builder(
        *,
        name,
        builderless = True,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_ANY,
        **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.mac",
        cores = cores,
        goma_backend = goma_backend,
        os = os,
        builderless = builderless,
        ssd = True,
        **kwargs
    )

def chromium_mac_ios_builder(
        *,
        name,
        caches = None,
        executable = "recipe:chromium_trybot",
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_10_15,
        properties = None,
        **kwargs):
    caches = caches or [builders.xcode_cache.x12a7209]

    properties = properties or {}
    properties.setdefault("xcode_build_version", "12a7209")

    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.mac",
        caches = caches,
        cores = None,
        executable = executable,
        goma_backend = goma_backend,
        os = os,
        properties = properties,
        **kwargs
    )

def chromium_swangle_builder(*, name, pinned = True, **kwargs):
    builder_args = dict(kwargs)
    builder_args.update(
        name = name,
        builder_group = "tryserver.chromium.swangle",
        builderless = True,
        service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    )
    if pinned:
        builder_args.update(executable = "recipe:angle_chromium_trybot")
    return try_builder(**builder_args)

def chromium_swangle_linux_builder(*, name, **kwargs):
    return chromium_swangle_builder(
        name = name,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.LINUX_DEFAULT,
        **kwargs
    )

def chromium_swangle_mac_builder(*, name, **kwargs):
    return chromium_swangle_builder(
        name = name,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_ANY,
        **kwargs
    )

def chromium_swangle_windows_builder(*, name, **kwargs):
    return chromium_swangle_builder(
        name = name,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.WINDOWS_DEFAULT,
        **kwargs
    )

def chromium_win_builder(
        *,
        name,
        builderless = True,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.WINDOWS_DEFAULT,
        **kwargs):
    return try_builder(
        name = name,
        builder_group = "tryserver.chromium.win",
        builderless = builderless,
        goma_backend = goma_backend,
        os = os,
        **kwargs
    )

gpu_try_resultdb_exports = [
    resultdb.export_test_results(
        bq_table = "luci-resultdb.chromium.gpu_try_test_results",
        predicate = resultdb.test_result_predicate(
            test_id_regexp = "ninja://chrome/test:telemetry_gpu_integration_test/.+",
        ),
    ),
]

def gpu_try_builder(*, name, builderless = False, execution_timeout = 6 * time.hour, **kwargs):
    return try_builder(
        name = name,
        builderless = builderless,
        execution_timeout = execution_timeout,
        service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        resultdb_bigquery_exports = gpu_try_resultdb_exports,
        **kwargs
    )

def gpu_chromium_android_builder(*, name, **kwargs):
    return gpu_try_builder(
        name = name,
        builder_group = "tryserver.chromium.android",
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs
    )

def gpu_chromium_linux_builder(*, name, **kwargs):
    return gpu_try_builder(
        name = name,
        builder_group = "tryserver.chromium.linux",
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs
    )

def gpu_chromium_mac_builder(*, name, **kwargs):
    return gpu_try_builder(
        name = name,
        builder_group = "tryserver.chromium.mac",
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_ANY,
        **kwargs
    )

def gpu_chromium_win_builder(*, name, os = builders.os.WINDOWS_ANY, **kwargs):
    return gpu_try_builder(
        name = name,
        builder_group = "tryserver.chromium.win",
        goma_backend = builders.goma.backend.RBE_PROD,
        os = os,
        **kwargs
    )

try_ = struct(
    # Module-level defaults for try functions
    defaults = defaults,

    # Functions for declaring automatically maintained list views
    list_view = list_view,

    # Functions for declaring try builders
    builder = try_builder,
    job = tryjob,

    # More specific builder wrapper functions
    blink_builder = blink_builder,
    blink_mac_builder = blink_mac_builder,
    chromium_builder = chromium_builder,
    chromium_android_builder = chromium_android_builder,
    chromium_angle_builder = chromium_angle_builder,
    chromium_chromiumos_builder = chromium_chromiumos_builder,
    chromium_dawn_builder = chromium_dawn_builder,
    chromium_linux_builder = chromium_linux_builder,
    chromium_mac_builder = chromium_mac_builder,
    chromium_mac_ios_builder = chromium_mac_ios_builder,
    chromium_swangle_linux_builder = chromium_swangle_linux_builder,
    chromium_swangle_mac_builder = chromium_swangle_mac_builder,
    chromium_swangle_windows_builder = chromium_swangle_windows_builder,
    chromium_win_builder = chromium_win_builder,
    gpu_chromium_android_builder = gpu_chromium_android_builder,
    gpu_chromium_linux_builder = gpu_chromium_linux_builder,
    gpu_chromium_mac_builder = gpu_chromium_mac_builder,
    gpu_chromium_win_builder = gpu_chromium_win_builder,
    gpu_try_resultdb_exports = gpu_try_resultdb_exports,
)
