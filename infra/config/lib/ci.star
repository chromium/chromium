# Copyright 2020 The Chromium Authors. All rights reserved.
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

load("./args.star", "args")
load("./branches.star", "branches")
load("./builders.star", "builders")

defaults = args.defaults(
    extends = builders.defaults,
    main_console_view = None,
    cq_mirrors_console_view = None,
)

def ci_builder(
        *,
        name,
        branch_selector = branches.MAIN,
        console_view = args.DEFAULT,
        main_console_view = args.DEFAULT,
        cq_mirrors_console_view = args.DEFAULT,
        console_view_entry = None,
        tree_closing = False,
        notifies = None,
        resultdb_bigquery_exports = None,
        experiments = None,
        **kwargs):
    """Define a CI builder.

    Arguments:
      name - name of the builder, will show up in UIs and logs. Required.
      branch_selector - A branch selector value controlling whether the
        builder definition is executed. See branches.star for more
        information.
      console_view_entry - A `consoles.console_view_entry` struct or a list of
        them describing console view entries to create for the builder.
        See `consoles.console_view_entry` for details.
      main_console_view - A string identifying the ID of the main console
        view to add an entry to. Supports a module-level default that
        defaults to None. An entry will be added only if
        `console_view_entry` is provided.
      cq_mirrors_console_view - A string identifying the ID of the CQ
        mirrors console view to add an entry to. Supports a module-level
        default that defaults to None. An entry will be added only if
        `console_view_entry` is provided.
      console_view_entry - A structure providing the details of the entry
        to add to the console view. See `ci.console_view_entry` for details.
      tree_closing - If true, failed builds from this builder that meet certain
        criteria will close the tree and email the sheriff. See the
        'chromium-tree-closer' config in notifiers.star for the full criteria.
      notifies - Any extra notifiers to attach to this builder.
      resultdb_bigquery_exports - a list of resultdb.export_test_results(...)
        specifying additional parameters for exporting test results to BigQuery.
        Will always upload to the following tables in addition to any tables
        specified by the list's elements:
          luci-resultdb.chromium.ci_test_results
          luci-resultdb.chromium.gpu_ci_test_results
      experiments - a dict of experiment name to the percentage chance (0-100)
        that it will apply to builds generated from this builder.
    """
    if not branches.matches(branch_selector):
        return

    # Branch builders should never close the tree, only builders from the main
    # "ci" bucket.
    bucket = defaults.get_value_from_kwargs("bucket", kwargs)
    if tree_closing and bucket == "ci":
        notifies = (notifies or []) + ["chromium-tree-closer", "chromium-tree-closer-email"]

    merged_resultdb_bigquery_exports = [
        resultdb.export_test_results(
            bq_table = "luci-resultdb.chromium.ci_test_results",
        ),
        resultdb.export_test_results(
            bq_table = "luci-resultdb.chromium.gpu_ci_test_results",
            predicate = resultdb.test_result_predicate(
                # Only match the telemetry_gpu_integration_test and
                # fuchsia_telemetry_gpu_integration_test targets.
                test_id_regexp = "ninja://(chrome/test:|content/test:fuchsia_)telemetry_gpu_integration_test/.+",
            ),
        ),
    ]
    merged_resultdb_bigquery_exports.extend(resultdb_bigquery_exports or [])

    # Enable "chromium.resultdb.result_sink" on ci builders at 50%.
    experiments = experiments or {}
    experiments.setdefault("chromium.resultdb.result_sink", 50)

    # Define the builder first so that any validation of luci.builder arguments
    # (e.g. bucket) occurs before we try to use it
    builders.builder(
        name = name,
        branch_selector = branch_selector,
        console_view_entry = console_view_entry,
        resultdb_bigquery_exports = merged_resultdb_bigquery_exports,
        notifies = notifies,
        experiments = experiments,
        **kwargs
    )

    if console_view_entry:
        # builder didn't fail, we're guaranteed that console_view_entry is
        # either a single console_view_entry or a list of them and that they are valid
        if type(console_view_entry) == type(struct()):
            entry = console_view_entry
        else:
            entry = console_view_entry[0]

        if branches.matches(entry.branch_selector):
            console_view = entry.console_view
            if console_view == None:
                console_view = defaults.get_value_from_kwargs("builder_group", kwargs)

        if console_view:
            builder = "{}/{}".format(bucket, name)

            overview_console_category = console_view
            if console_view_entry.category:
                overview_console_category = "|".join([console_view, console_view_entry.category])
            main_console_view = defaults.get_value("main_console_view", main_console_view)
            if main_console_view:
                luci.console_view_entry(
                    builder = builder,
                    console_view = main_console_view,
                    category = overview_console_category,
                    short_name = console_view_entry.short_name,
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
                    short_name = console_view_entry.short_name,
                )

def android_builder(
        *,
        name,
        # TODO(tandrii): migrate to this gradually (current value of
        # goma.jobs.MANY_JOBS_FOR_CI is 500).
        # goma_jobs=goma.jobs.MANY_JOBS_FOR_CI
        goma_jobs = builders.goma.jobs.J150,
        **kwargs):
    return ci_builder(
        name = name,
        builder_group = "chromium.android",
        goma_backend = builders.goma.backend.RBE_PROD,
        goma_jobs = goma_jobs,
        **kwargs
    )

def android_fyi_builder(*, name, **kwargs):
    return ci_builder(
        name = name,
        builder_group = "chromium.android.fyi",
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs
    )

def chromium_builder(*, name, tree_closing = True, **kwargs):
    return ci_builder(
        name = name,
        builder_group = "chromium",
        goma_backend = builders.goma.backend.RBE_PROD,
        tree_closing = tree_closing,
        **kwargs
    )

def chromiumos_builder(*, name, tree_closing = True, **kwargs):
    return ci_builder(
        name = name,
        builder_group = "chromium.chromiumos",
        goma_backend = builders.goma.backend.RBE_PROD,
        tree_closing = tree_closing,
        **kwargs
    )

def clang_builder(*, name, builderless = True, cores = 32, properties = None, **kwargs):
    properties = properties or {}
    properties.update({
        "perf_dashboard_machine_group": "ChromiumClang",
    })
    return ci_builder(
        name = name,
        builder_group = "chromium.clang",
        builderless = builderless,
        cores = cores,
        # Because these run ToT Clang, goma is not used.
        # Naturally the runtime will be ~4-8h on average, depending on config.
        # CFI builds will take even longer - around 11h.
        execution_timeout = 12 * time.hour,
        properties = properties,
        **kwargs
    )

def clang_mac_builder(*, name, cores = 24, **kwargs):
    return clang_builder(
        name = name,
        cores = cores,
        os = builders.os.MAC_10_15,
        ssd = True,
        properties = {
            # The Chromium build doesn't need system Xcode, but the ToT clang
            # bots also build clang and llvm and that build does need system
            # Xcode.
            "xcode_build_version": "12a7209",
        },
        **kwargs
    )

def dawn_builder(*, name, **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.dawn",
        service_account =
            "chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        **kwargs
    )

def dawn_linux_builder(
        *,
        name,
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs):
    return dawn_builder(
        name = name,
        builderless = True,
        goma_backend = goma_backend,
        os = builders.os.LINUX_DEFAULT,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def dawn_mac_builder(*, name, **kwargs):
    return dawn_builder(
        name = name,
        builderless = False,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_ANY,
        **kwargs
    )

# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def dawn_thin_tester(
        *,
        name,
        **kwargs):
    return dawn_linux_builder(
        name = name,
        cores = 2,
        # Setting goma_backend for testers is a no-op, but better to be explicit
        goma_backend = None,
        **kwargs
    )

def dawn_windows_builder(*, name, **kwargs):
    return dawn_builder(
        name = name,
        builderless = True,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.WINDOWS_ANY,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def fuzz_builder(*, name, **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.fuzz",
        goma_backend = builders.goma.backend.RBE_PROD,
        notifies = ["chromesec-lkgr-failures"],
        **kwargs
    )

def fuzz_libfuzzer_builder(*, name, **kwargs):
    return fuzz_builder(
        name = name,
        executable = "recipe:chromium_libfuzzer",
        **kwargs
    )

def fyi_builder(
        *,
        name,
        execution_timeout = 10 * time.hour,
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.fyi",
        execution_timeout = execution_timeout,
        goma_backend = goma_backend,
        # TODO(crbug.com/1108016): Move this kwarg to ci.builder(), after
        # ResultSink and result_adapter is confirmed to work.
        experiments = {
            "chromium.resultdb.result_sink": 100,
        },
        **kwargs
    )

def fyi_celab_builder(*, name, **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.fyi",
        os = builders.os.WINDOWS_ANY,
        executable = "recipe:celab",
        goma_backend = builders.goma.backend.RBE_PROD,
        properties = {
            "exclude": "chrome_only",
            "pool_name": "celab-chromium-ci",
            "pool_size": 20,
            "tests": "*",
        },
        **kwargs
    )

def fyi_coverage_builder(
        *,
        name,
        cores = 32,
        ssd = True,
        execution_timeout = 20 * time.hour,
        **kwargs):
    return fyi_builder(
        name = name,
        cores = cores,
        ssd = ssd,
        execution_timeout = execution_timeout,
        **kwargs
    )

def fyi_ios_builder(
        *,
        name,
        executable = "recipe:chromium",
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_10_15,
        xcode = builders.xcode.x12a7209,
        **kwargs):
    return fyi_builder(
        name = name,
        cores = None,
        executable = executable,
        os = os,
        xcode = xcode,
        **kwargs
    )

def fyi_mac_builder(
        *,
        name,
        cores = 4,
        os = builders.os.MAC_DEFAULT,
        **kwargs):
    return fyi_builder(
        name = name,
        cores = cores,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = os,
        **kwargs
    )

def fyi_windows_builder(
        *,
        name,
        os = builders.os.WINDOWS_DEFAULT,
        **kwargs):
    return fyi_builder(
        name = name,
        os = os,
        **kwargs
    )

def gpu_fyi_builder(*, name, **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.gpu.fyi",
        service_account =
            "chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        properties = {
            "perf_dashboard_machine_group": "ChromiumGPUFYI",
        },
        **kwargs
    )

def gpu_fyi_linux_builder(
        *,
        name,
        execution_timeout = 6 * time.hour,
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs):
    return gpu_fyi_builder(
        name = name,
        execution_timeout = execution_timeout,
        goma_backend = goma_backend,
        os = builders.os.LINUX_DEFAULT,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def gpu_fyi_mac_builder(*, name, **kwargs):
    return gpu_fyi_builder(
        name = name,
        builderless = False,
        cores = None,
        execution_timeout = 6 * time.hour,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_ANY,
        **kwargs
    )

# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def gpu_fyi_thin_tester(
        *,
        name,
        execution_timeout = 6 * time.hour,
        **kwargs):
    return gpu_fyi_linux_builder(
        name = name,
        cores = 2,
        execution_timeout = execution_timeout,
        # Setting goma_backend for testers is a no-op, but better to be explicit
        # here and also leave the generated configs unchanged for these testers.
        goma_backend = None,
        **kwargs
    )

def gpu_fyi_windows_builder(*, name, **kwargs):
    return gpu_fyi_builder(
        name = name,
        builderless = True,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.WINDOWS_ANY,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def gpu_builder(*, name, tree_closing = True, notifies = None, **kwargs):
    if tree_closing:
        notifies = (notifies or []) + ["gpu-tree-closer-email"]
    return ci.builder(
        name = name,
        builder_group = "chromium.gpu",
        tree_closing = tree_closing,
        notifies = notifies,
        **kwargs
    )

def gpu_linux_builder(
        *,
        name,
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs):
    return gpu_builder(
        name = name,
        builderless = True,
        goma_backend = goma_backend,
        os = builders.os.LINUX_DEFAULT,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def gpu_mac_builder(*, name, **kwargs):
    return gpu_builder(
        name = name,
        builderless = False,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_ANY,
        **kwargs
    )

# Many of the GPU testers are thin testers, they use linux VMS regardless of the
# actual OS that the tests are built for
def gpu_thin_tester(*, name, tree_closing = True, **kwargs):
    return gpu_linux_builder(
        name = name,
        cores = 2,
        tree_closing = tree_closing,
        # Setting goma_backend for testers is a no-op, but better to be explicit
        goma_backend = None,
        **kwargs
    )

def gpu_windows_builder(*, name, **kwargs):
    return gpu_builder(
        name = name,
        builderless = True,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.WINDOWS_ANY,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def linux_builder(
        *,
        name,
        goma_backend = builders.goma.backend.RBE_PROD,
        goma_jobs = builders.goma.jobs.MANY_JOBS_FOR_CI,
        tree_closing = True,
        notifies = ("chromium.linux",),
        extra_notifies = None,
        **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.linux",
        goma_backend = goma_backend,
        goma_jobs = goma_jobs,
        tree_closing = tree_closing,
        notifies = list(notifies) + (extra_notifies or []),
        **kwargs
    )

def mac_builder(
        *,
        name,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_DEFAULT,
        tree_closing = True,
        **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.mac",
        cores = cores,
        goma_backend = goma_backend,
        os = os,
        tree_closing = tree_closing,
        **kwargs
    )

def mac_ios_builder(
        *,
        name,
        executable = "recipe:chromium",
        goma_backend = builders.goma.backend.RBE_PROD,
        xcode = builders.xcode.x12a7209,
        **kwargs):
    return mac_builder(
        name = name,
        goma_backend = goma_backend,
        executable = executable,
        os = builders.os.MAC_10_15,
        xcode = xcode,
        **kwargs
    )

def memory_builder(
        *,
        name,
        goma_jobs = builders.goma.jobs.MANY_JOBS_FOR_CI,
        notifies = None,
        tree_closing = True,
        **kwargs):
    if name.startswith("Linux"):
        notifies = (notifies or []) + ["linux-memory"]

    return ci.builder(
        name = name,
        builder_group = "chromium.memory",
        goma_backend = builders.goma.backend.RBE_PROD,
        goma_jobs = goma_jobs,
        notifies = notifies,
        tree_closing = tree_closing,
        **kwargs
    )

def mojo_builder(
        *,
        name,
        execution_timeout = 10 * time.hour,
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.mojo",
        execution_timeout = execution_timeout,
        goma_backend = goma_backend,
        **kwargs
    )

def swangle_builder(*, name, builderless = True, pinned = True, **kwargs):
    builder_args = dict(kwargs)
    builder_args.update(
        name = name,
        builder_group = "chromium.swangle",
        builderless = builderless,
        service_account =
            "chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    )
    if pinned:
        builder_args.update(executable = "recipe:angle_chromium")
    return ci.builder(**builder_args)

def swangle_linux_builder(
        *,
        name,
        **kwargs):
    return swangle_builder(
        name = name,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.LINUX_DEFAULT,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def swangle_mac_builder(
        *,
        name,
        **kwargs):
    return swangle_builder(
        name = name,
        builderless = False,
        cores = None,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_ANY,
        **kwargs
    )

def swangle_windows_builder(*, name, **kwargs):
    return swangle_builder(
        name = name,
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.WINDOWS_ANY,
        pool = "luci.chromium.gpu.ci",
        **kwargs
    )

def thin_tester(
        *,
        name,
        triggered_by,
        builder_group,
        tree_closing = True,
        **kwargs):
    return ci.builder(
        name = name,
        builder_group = builder_group,
        triggered_by = triggered_by,
        goma_backend = None,
        tree_closing = tree_closing,
        **kwargs
    )

def updater_builder(
        *,
        name,
        **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.updater",
        goma_backend = builders.goma.backend.RBE_PROD,
        **kwargs
    )

def win_builder(
        *,
        name,
        os = builders.os.WINDOWS_DEFAULT,
        tree_closing = True,
        **kwargs):
    return ci.builder(
        name = name,
        builder_group = "chromium.win",
        goma_backend = builders.goma.backend.RBE_PROD,
        os = os,
        tree_closing = tree_closing,
        **kwargs
    )

ci = struct(
    # Module-level defaults for ci functions
    defaults = defaults,

    # Functions for declaring CI builders
    builder = ci_builder,

    # More specific builder wrapper functions
    android_builder = android_builder,
    android_fyi_builder = android_fyi_builder,
    chromium_builder = chromium_builder,
    chromiumos_builder = chromiumos_builder,
    clang_builder = clang_builder,
    clang_mac_builder = clang_mac_builder,
    dawn_linux_builder = dawn_linux_builder,
    dawn_mac_builder = dawn_mac_builder,
    dawn_thin_tester = dawn_thin_tester,
    dawn_windows_builder = dawn_windows_builder,
    fuzz_builder = fuzz_builder,
    fuzz_libfuzzer_builder = fuzz_libfuzzer_builder,
    fyi_builder = fyi_builder,
    fyi_celab_builder = fyi_celab_builder,
    fyi_coverage_builder = fyi_coverage_builder,
    fyi_ios_builder = fyi_ios_builder,
    fyi_mac_builder = fyi_mac_builder,
    fyi_windows_builder = fyi_windows_builder,
    gpu_fyi_linux_builder = gpu_fyi_linux_builder,
    gpu_fyi_mac_builder = gpu_fyi_mac_builder,
    gpu_fyi_thin_tester = gpu_fyi_thin_tester,
    gpu_fyi_windows_builder = gpu_fyi_windows_builder,
    gpu_linux_builder = gpu_linux_builder,
    gpu_mac_builder = gpu_mac_builder,
    gpu_thin_tester = gpu_thin_tester,
    gpu_windows_builder = gpu_windows_builder,
    linux_builder = linux_builder,
    mac_builder = mac_builder,
    mac_ios_builder = mac_ios_builder,
    memory_builder = memory_builder,
    mojo_builder = mojo_builder,
    swangle_linux_builder = swangle_linux_builder,
    swangle_mac_builder = swangle_mac_builder,
    swangle_windows_builder = swangle_windows_builder,
    thin_tester = thin_tester,
    updater_builder = updater_builder,
    win_builder = win_builder,
)
