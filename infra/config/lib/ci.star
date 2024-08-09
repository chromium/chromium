# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining CI builders.

The `ci_builder` function defined in this module enables defining a CI builder.
It can also be accessed through `ci.builder`.

The `defaults` struct provides module-level defaults for the arguments to
`ci_builder`. The parameters that support module-level defaults have a
corresponding attribute on `defaults` that is a `lucicfg.var` that can be used
to set the default value. Can also be accessed through `ci.defaults`.
"""

load("//project.star", "settings")
load("./args.star", "args")
load("./branches.star", "branches")
load("./builder_config.star", "builder_config")
load("./builders.star", "builders", "os")

defaults = args.defaults(
    extends = builders.defaults,
    main_console_view = None,
    cq_mirrors_console_view = None,
    thin_tester_cores = args.DEFAULT,
    tree_closing = False,
    tree_closing_notifiers = None,
)

def ci_builder(
        *,
        name,
        branch_selector = branches.selector.MAIN,
        console_view_entry = None,
        main_console_view = args.DEFAULT,
        cq_mirrors_console_view = args.DEFAULT,
        gardener_rotations = None,
        tree_closing = args.DEFAULT,
        tree_closing_notifiers = None,
        resultdb_bigquery_exports = None,
        experiments = None,
        **kwargs):
    """Define a CI builder.

    Args:
      name: name of the builder, will show up in UIs and logs. Required.
      branch_selector: A branch selector value controlling whether the
        builder definition is executed. See branches.star for more
        information.
      console_view_entry: A `consoles.console_view_entry` struct or a list of
        them describing console view entries to create for the builder.
        See `consoles.console_view_entry` for details.
      main_console_view: A string identifying the ID of the main console
        view to add an entry to. Supports a module-level default that
        defaults to None. An entry will be added only if
        `console_view_entry` is provided and the first entry's branch
        selector causes the entry to be defined. On branches, the provided
        value is ignored; all CI builders will be added to the `main` console.
      cq_mirrors_console_view: A string identifying the ID of the CQ
        mirrors console view to add an entry to. Supports a module-level
        default that defaults to None. An entry will be added only if
        `console_view_entry` is provided and the first entry's branch
        selector causes the entry to be defined.
      gardener_rotations: The name(s) of any gardener rotations that the builder
        should be added to. On branches, all CI builders will be added to the
        `chrome_browser_release` gardener rotation.
      tree_closing: If true, failed builds from this builder that meet certain
        criteria will close the tree and email the gardener. See the
        'chromium-tree-closer' config in notifiers.star for the full criteria.
      tree_closing_notifiers: A list of notifiers that will be notified when
        tree closing criteria are met by a build of this builder. Supports a
        module-level default that will be merged with the provided value.
      notifies: Any extra notifiers to attach to this builder.
      resultdb_bigquery_exports: a list of resultdb.export_test_results(...)
        specifying additional parameters for exporting test results to BigQuery.
        Will always upload to the following tables in addition to any tables
        specified by the list's elements:
          chrome-luci-data.chromium.ci_test_results
          chrome-luci-data.chromium.gpu_ci_test_results
      experiments: a dict of experiment name to the percentage chance (0-100)
        that it will apply to builds generated from this builder.
      **kwargs: Additional keyword arguments that will be forwarded on to
        `builders.builder`.
    """
    if not branches.matches(branch_selector):
        return

    experiments = experiments or {}

    # TODO(crbug.com/40232671): Remove when the experiment is the default.
    experiments.setdefault(
        "chromium_swarming.expose_merge_script_failures",
        5 if settings.project.startswith("chrome") else 100,
    )

    try_only_kwargs = [k for k in ("mirrors", "try_settings") if k in kwargs]
    if try_only_kwargs:
        fail("CI builders cannot specify the following try-only arguments: {}".format(try_only_kwargs))

    notifies = kwargs.get("notifies", [])
    tree_closing = defaults.get_value("tree_closing", tree_closing)
    if tree_closing:
        tree_closing_notifiers = defaults.get_value("tree_closing_notifiers", tree_closing_notifiers, merge = args.MERGE_LIST)
        tree_closing_notifiers = args.listify("chromium-tree-closer", "chromium-tree-closer-email", tree_closing_notifiers)
        if notifies != None:
            notifies = args.listify(notifies, tree_closing_notifiers)

    kwargs["notifies"] = notifies

    bq_dataset_name = "chrome"
    if settings.project.startswith("chromium"):
        bq_dataset_name = "chromium"
    merged_resultdb_bigquery_exports = [
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.{}.ci_test_results".format(bq_dataset_name),
        ),
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.{}.gpu_ci_test_results".format(bq_dataset_name),
            predicate = resultdb.test_result_predicate(
                # Only match the telemetry_gpu_integration_test target and its
                # Fuchsia and Android variants that have a suffix added to the
                # end. Those are caught with [^/]*. The Fuchsia version is in
                # //content/test since Fuchsia cannot depend on //chrome.
                test_id_regexp = "ninja://(chrome|content)/test:telemetry_gpu_integration_test[^/]*/.+",
            ),
        ),
        resultdb.export_test_results(
            bq_table = "chrome-luci-data.{}.blink_web_tests_ci_test_results".format(bq_dataset_name),
            predicate = resultdb.test_result_predicate(
                # Match the "blink_web_tests" target and all of its
                # flag-specific versions, e.g. "vulkan_swiftshader_blink_web_tests".
                test_id_regexp = "(ninja://[^/]*blink_web_tests/.+)|(ninja://[^/]*_wpt_tests/.+)",
            ),
        ),
    ]
    merged_resultdb_bigquery_exports.extend(resultdb_bigquery_exports or [])

    branch_gardener_rotations = list({
        platform_settings.gardener_rotation: None
        for platform, platform_settings in settings.platforms.items()
        if branches.matches(branch_selector, platform = platform)
    })
    gardener_rotations = args.listify(gardener_rotations, branch_gardener_rotations)

    # Define the builder first so that any validation of luci.builder arguments
    # (e.g. bucket) occurs before we try to use it
    builders.builder(
        name = name,
        branch_selector = branch_selector,
        console_view_entry = console_view_entry,
        resultdb_bigquery_exports = merged_resultdb_bigquery_exports,
        gardener_rotations = gardener_rotations,
        experiments = experiments,
        resultdb_index_by_timestamp = settings.project.startswith("chromium"),
        **kwargs
    )

    if console_view_entry and settings.project.startswith("chromium"):
        # builder didn't fail, we're guaranteed that console_view_entry is
        # either a single console_view_entry or a list of them and that they are valid
        if type(console_view_entry) == type(struct()):
            entry = console_view_entry
        else:
            entry = console_view_entry[0]

        if branches.matches(entry.branch_selector):
            console_view = entry.console_view
            if console_view == None:
                console_view = defaults.console_view.get()
            if console_view == args.COMPUTE:
                console_view = defaults.get_value_from_kwargs("builder_group", kwargs)

            bucket = defaults.get_value_from_kwargs("bucket", kwargs)
            builder = "{}/{}".format(bucket, name)

            overview_console_category = console_view
            if entry.category:
                overview_console_category = "|".join([console_view, entry.category])
            if not settings.is_main:
                main_console_view = "main"
            else:
                main_console_view = defaults.get_value("main_console_view", main_console_view)
            if main_console_view:
                luci.console_view_entry(
                    builder = builder,
                    console_view = main_console_view,
                    category = overview_console_category,
                    short_name = entry.short_name,
                )

            cq_mirrors_console_view = defaults.get_value(
                "cq_mirrors_console_view",
                cq_mirrors_console_view,
            )
            if cq_mirrors_console_view:
                luci.console_view_entry(
                    builder = builder,
                    console_view = cq_mirrors_console_view,
                    category = overview_console_category,
                    short_name = entry.short_name,
                )

def _gpu_linux_builder(*, name, **kwargs):
    """Defines a GPU-related linux builder.

    This sets linux-specific defaults that are common to GPU-related builder
    groups.
    """
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.LINUX_DEFAULT)
    return ci.builder(name = name, **kwargs)

def _gpu_mac_builder(*, name, **kwargs):
    """Defines a GPU-related mac builder.

    This sets mac-specific defaults that are common to GPU-related builder
    groups.
    """
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("os", os.MAC_ANY)
    kwargs.setdefault("reclient_scandeps_server", True)
    return ci.builder(name = name, **kwargs)

def _gpu_windows_builder(*, name, **kwargs):
    """Defines a GPU-related windows builder.

    This sets windows-specific defaults that are common to GPU-related builder
    groups.
    """
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.WINDOWS_ANY)
    return ci.builder(name = name, **kwargs)

def thin_tester(
        *,
        name,
        triggered_by,
        cores = args.DEFAULT,
        **kwargs):
    """Define a thin tester.

    A thin tester is a builder that is triggered by another builder to
    trigger and wait for tests that were built by the triggering builder.
    Because these builders do not perform compilation and are performing
    platform-agnostic operations, they can generally run on small linux
    machines.

    Args:
      name: The name of the builder.
      triggered_by: The triggering builder. See
        https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/main/lucicfg/doc/README.md#luci.builder
        for more information.
      cores: See `builders.builder` for more information. The `thin_tester_core`
        module-level default in `ci.defaults` will be used as the default if it
        is set.
      **kwargs: Additional keyword arguments to forward on to `ci.builder`.

    Returns:
      The `luci.builder` keyset.
    """
    builder_spec = kwargs.get("builder_spec")
    if builder_spec and builder_spec.execution_mode != builder_config.execution_mode.TEST:
        fail("thin testers with builder specs must have TEST execution mode")
    cores = defaults.get_value("thin_tester_cores", cores)
    kwargs.setdefault("siso_project", None)
    kwargs.setdefault("os", builders.os.LINUX_DEFAULT)
    return ci.builder(
        name = name,
        triggered_by = triggered_by,
        cores = cores,
        **kwargs
    )

ci = struct(
    # Module-level defaults for ci functions
    defaults = defaults,

    # Functions for declaring CI builders
    builder = ci_builder,
    thin_tester = thin_tester,

    # CONSTANTS
    DEFAULT_EXECUTABLE = "recipe:chromium",
    DEFAULT_EXECUTION_TIMEOUT = 3 * time.hour,
    DEFAULT_FYI_PRIORITY = 35,
    DEFAULT_POOL = "luci.chromium.ci",
    DEFAULT_SERVICE_ACCOUNT = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    DEFAULT_SHADOW_SERVICE_ACCOUNT = "chromium-try-builder@chops-service-accounts.iam.gserviceaccount.com",

    # Functions and constants for the GPU-related builder groups
    gpu = struct(
        linux_builder = _gpu_linux_builder,
        mac_builder = _gpu_mac_builder,
        windows_builder = _gpu_windows_builder,
        POOL = "luci.chromium.gpu.ci",
        SERVICE_ACCOUNT = "chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        SHADOW_SERVICE_ACCOUNT = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        TREE_CLOSING_NOTIFIERS = ["gpu-tree-closer-email"],
    ),
)
