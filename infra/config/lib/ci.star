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

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("//project.star", "settings")
load("./args.star", "args")
load("./branches.star", "branches")
load("./builders.star", "builders")

defaults = args.defaults(
    extends = builders.defaults,
    add_to_console_view = False,
    console_view = args.COMPUTE,
    header = None,
    main_console_view = None,
    cq_mirrors_console_view = None,
    repo = None,
    refs = None,
)

def declare_bucket(milestone_vars, *, branch_selector = branches.MAIN_ONLY):
    if not branches.matches(branch_selector):
        return

    luci.bucket(
        name = milestone_vars.ci_bucket,
        acls = [
            acl.entry(
                roles = acl.BUILDBUCKET_READER,
                groups = "all",
            ),
            acl.entry(
                roles = acl.BUILDBUCKET_TRIGGERER,
                groups = "project-chromium-ci-schedulers",
            ),
            acl.entry(
                roles = acl.BUILDBUCKET_OWNER,
                groups = "google/luci-task-force@google.com",
            ),
        ],
    )

    luci.gitiles_poller(
        name = milestone_vars.ci_poller,
        bucket = milestone_vars.ci_bucket,
        repo = "https://chromium.googlesource.com/chromium/src",
        refs = [milestone_vars.ref],
    )

    # TODO(gbeaty) Determine what should be in each main console and define it
    # separately
    for name, title in (
        (milestone_vars.main_console_name, milestone_vars.main_console_title),
        (milestone_vars.cq_mirrors_console_name, milestone_vars.cq_mirrors_console_title),
    ):
        ci.overview_console_view(
            name = name,
            branch_selector = branch_selector,
            header = "//chromium-header.textpb",
            repo = "https://chromium.googlesource.com/chromium/src",
            refs = [milestone_vars.ref],
            title = title,
            top_level_ordering = [
                "chromium",
                "chromium.win",
                "chromium.mac",
                "chromium.linux",
                "chromium.chromiumos",
                "chromium.android",
                "chrome",
                "chromium.memory",
                "chromium.dawn",
                "chromium.gpu",
                "chromium.fyi",
                "chromium.android.fyi",
                "chromium.clang",
                "chromium.fuzz",
                "chromium.gpu.fyi",
                "chromium.swangle",
            ],
        )

def set_defaults(milestone_vars, **kwargs):
    default_values = dict(
        add_to_console_view = milestone_vars.is_master,
        bucket = milestone_vars.ci_bucket,
        build_numbers = True,
        configure_kitchen = True,
        cores = 8,
        cpu = builders.cpu.X86_64,
        executable = "recipe:chromium",
        execution_timeout = 3 * time.hour,
        header = "//chromium-header.textpb",
        os = builders.os.LINUX_DEFAULT,
        pool = "luci.chromium.ci",
        project_trigger_overrides = {"chromium": settings.project} if not settings.is_master else None,
        repo = "https://chromium.googlesource.com/chromium/src",
        refs = [milestone_vars.ref],
        service_account = "chromium-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
        swarming_tags = ["vpython:native-python-wrapper"],
        triggered_by = [milestone_vars.ci_poller],
        # TODO(crbug.com/1129723): set default goma_backend here.
    )
    default_values.update(kwargs)
    for k, v in default_values.items():
        getattr(defaults, k).set(v)

def _console_view_ordering_graph_key(console_name):
    return graph.key("@chromium", "", "console_view_ordering", console_name)

def _overview_console_view_ordering_graph_key(console_name):
    return graph.key("@chromium", "", "overview_console_view_ordering", console_name)

def _console_view_ordering_impl(ctx, *, console_name, ordering):
    key = _console_view_ordering_graph_key(console_name)
    graph.add_node(key, props = {
        "ordering": ordering,
    })
    graph.add_edge(keys.project(), key)
    return graph.keyset(key)

_console_view_ordering = lucicfg.rule(impl = _console_view_ordering_impl)

def _overview_console_view_ordering_impl(ctx, *, console_name, top_level_ordering):
    key = _overview_console_view_ordering_graph_key(console_name)
    graph.add_node(key, props = {
        "top_level_ordering": top_level_ordering,
    })
    graph.add_edge(keys.project(), key)
    return graph.keyset(key)

_overview_console_view_ordering = lucicfg.rule(impl = _overview_console_view_ordering_impl)

def _category_join(parent, category):
    return "|".join([c for c in (parent, category) if c])

def _level_sort_key(category, ordering):
    """Compute the key for a single level of ordering.

    A key that can be used to sort categories/short names at the same
    category nesting level.
    """
    for i, c in enumerate(ordering):
        if c == category:
            # We found the category in the ordering, so the index in the ordering is
            # sufficient for sorting
            return (i,)

    # We didn't find the category, the key uses:
    # 1. The length of the ordering so that it sorts after all the categories in
    #    the ordering
    # 2. The category itself, which lexicographically sorts all of the categories
    #    that do not match the ordering
    return (len(ordering), category)

def _builder_sort_key(console_ordering, category, short_name, name):
    """Compute the key for a builder.

    Builders are sorted lexicographically by the sequence of category
    components, then lexicographically by the short name, then by the
    builder names. The ordering for the console_view modifies the sorting
    for category components and short names for given prefixes of the
    category component sequence. Builders with no short name will sort
    before builders with a short name for a given category, which cannot
    be modified by the ordering.

    Returns:
      A key that can be used to sort builder entries within the same console.
    """
    current_category = None

    # Build the category key as a sequence of the keys for each level
    category_key = []
    if category:
        for c in category.split("|"):
            ordering = console_ordering.get(current_category, [])
            if type(ordering) == type(""):
                ordering = console_ordering[ordering]
            if type(ordering) == type(struct()):
                ordering = ordering.categories
            category_key.append(_level_sort_key(c, ordering))
            current_category = _category_join(current_category, c)

    short_name_key = ()
    if short_name:
        ordering = console_ordering.get(category, [])
        if type(ordering) == type(""):
            ordering = console_ordering[ordering]
        short_name_ordering = getattr(ordering, "short_names", [])
        short_name_key = _level_sort_key(short_name, short_name_ordering)

    return (
        category_key,
        short_name_key,
        name,
    )

def _get_console_ordering(console_name):
    """Get the ordering dict used for sorting entries of a console_view.

    Returns:
      The ordering dict used to sort entries of the console_view with the
      given name or None if the name does not refer to a console_view with
      an ordering.
    """
    graph_key = _console_view_ordering_graph_key(console_name)
    node = graph.node(graph_key)
    return node.props.ordering if node != None else None

def _get_console_view_key_fn(console_name):
    """Get the key function for sorting entries of a console_view.

    Returns:
      The key function used to sort entries of the console_view with the
      given name or None if the name does not refer to a console_view with
      an ordering.
    """
    ordering = _get_console_ordering(console_name)
    if ordering == None:
        return None

    def key_fn(b):
        return _builder_sort_key(ordering, b.category, b.short_name, b.name)

    return key_fn

def _get_overview_console_view_key_fn(console_name):
    """Get the key function for sorting overview_console_view entries.

    Returns:
      The key function used to sort entries of the overview_console_view
      with the given name or None if the name does not refer to an
      overview_console_view.
    """
    overview_console_ordering = graph.node(
        _overview_console_view_ordering_graph_key(console_name),
    )
    if overview_console_ordering == None:
        return None

    top_level_ordering = overview_console_ordering.props.top_level_ordering

    def key_fn(b):
        if not b.category:
            fail("Builder {} must have a category".format(b))
        category_components = b.category.split("|", 1)

        subconsole = category_components[0]
        subconsole_sort_key = _level_sort_key(subconsole, top_level_ordering)

        builder_sort_key = ()
        subconsole_ordering = _get_console_ordering(subconsole)
        if subconsole_ordering != None:
            category = ""
            if len(category_components) > 1:
                category = category_components[1]
            builder_sort_key = _builder_sort_key(
                subconsole_ordering,
                category,
                b.short_name,
                b.name,
            )

        return (
            subconsole_sort_key,
            builder_sort_key,
        )

    return key_fn

def _sort_console_entries(ctx):
    milo = ctx.output["luci-milo.cfg"]
    consoles = []
    for console in milo.consoles:
        if not console.builders:
            continue
        key_fn = (_get_console_view_key_fn(console.id) or
                  _get_overview_console_view_key_fn(console.id))
        if key_fn:
            console.builders = sorted(console.builders, key_fn)
        consoles.append(console)

lucicfg.generator(_sort_console_entries)

def ordering(*, short_names = None, categories = None):
    """Specifies the sorting behavior for a category.

    Args:
      short_names - A list of strings that specifies the order short names
        should appear for builders in the same category. Builders without
        short names will appear before all others and builders with short
        names that do not appear in the list will be sorted
        lexicographically after short names that do appear in the list. By
        default, short names are sorted lexicographically.
      categories - A list of strings that specifies the order the next
        category component should appear for builders with matching
        category prefix. Builders without any additional category
        components will appear before all others and builders whose next
        category component do not appear in the list will be sorted
        lexicographically by the next category component. By default, the
        next category components are sorted lexicographically.
    """
    return struct(
        short_names = short_names or [],
        categories = categories or [],
    )

def console_view(*, name, branch_selector = branches.MAIN_ONLY, ordering = None, **kwargs):
    """Create a console view, optionally providing an entry ordering.

    Args:
      name - The name of the console view.
      ordering - A dictionary defining the ordering of categories for the
        console. If not provided, the console will not be sorted.

        The keys of the dictionary indicate the category that the values
        applies the sorting to and can take one of two forms:
        1.  None: Controls the ordering of the top-level categories and/or
            the short names of builders that have no category.
        2.  str: Category string to apply the ordering to the next nested
            level of categories and/or the short names of builders with
            that category. Arbitrary strings can be used also, which can
            be used as aliases for other entries to refer to.

        The value for each entry defines the ordering to be applied to
        builders that have matched the sequence of category components
        identified by the key and can take one of two forms:
        1.  struct created using `ci.ordering`: See `ci.ordering` for
            details.
        2.  list of category components: Equivalent to a `ci.ordering`
            call that only specifies `categories` with the given list.
        3.  str: An alias for another category. The string must be another
            key in the dict. The ordering will be looked up by that key
            instead.
      kwargs - Additional keyword arguments to forward on to
        `luci.console_view`. The header and repo arguments support
         module-level defaults.
    """
    if not branches.matches(branch_selector):
        return

    kwargs["header"] = defaults.get_value_from_kwargs("header", kwargs)
    kwargs["repo"] = defaults.get_value_from_kwargs("repo", kwargs)
    kwargs["refs"] = defaults.get_value_from_kwargs("refs", kwargs)
    luci.console_view(
        name = name,
        **kwargs
    )

    _console_view_ordering(
        console_name = name,
        ordering = ordering or {},
    )

def overview_console_view(*, name, top_level_ordering, branch_selector = branches.MAIN_ONLY, **kwargs):
    """Create an overview console view.

    An overview console view is a console view that contains a subset of
    entries from other consoles. The entries from each console will have
    that console's name prepended to the entries' categories and will
    appear in the same order as they do in that console. The ordering of
    entries from different consoles is controlled by the
    `top_level_ordering` parameter.

    Args:
      name - The name of the console view.
      top_level_ordering - A list of strings defining the order that
        entries from different consoles will appear. Entries will be
        sorted by the name of the console they come from, appearing in the
        same order as in top_level_ordering. Entries from consoles whose
        name does not appear in the list will be sorted lexicographically
        by the console name and appear after entries whose console does
        appear in the list.
      kwargs - Additional keyword arguments to forward on to
        `luci.console_view`. The header and repo arguments support
         module-level defaults.
    """
    kwargs["header"] = defaults.get_value_from_kwargs("header", kwargs)
    kwargs["repo"] = defaults.get_value_from_kwargs("repo", kwargs)
    luci.console_view(
        name = name,
        **kwargs
    )

    _overview_console_view_ordering(
        console_name = name,
        top_level_ordering = top_level_ordering,
    )

def console_view_entry(*, category = None, short_name = None):
    """Specifies the details of a console view entry.

    See https://chromium.googlesource.com/infra/luci/luci-go/+/refs/heads/master/lucicfg/doc/README.md#luci.console_view_entry
    for details on the arguments.

    Returns:
      A struct that can be passed to the `console_view_entry` argument of
      `ci.builder` to add an entry to the console for the builder's group.
    """
    return struct(
        category = category,
        short_name = short_name,
    )

def ci_builder(
        *,
        name,
        branch_selector = branches.MAIN_ONLY,
        add_to_console_view = args.DEFAULT,
        console_view = args.DEFAULT,
        main_console_view = args.DEFAULT,
        cq_mirrors_console_view = args.DEFAULT,
        console_view_entry = None,
        tree_closing = False,
        notifies = None,
        **kwargs):
    """Define a CI builder.

    Arguments:
      name - name of the builder, will show up in UIs and logs. Required.
      branch_selector - A branch selector value controlling whether the
        builder definition is executed. See branches.star for more
        information.
      add_to_console_view - A bool indicating whether an entry should be
        created for the builder in the console identified by
        `console_view`. Supports a module-level default that defaults to
        False.
      console_view - A string identifying the ID of the console view to
        add an entry to. Supports a module-level default that defaults to
        the group of the builder, if provided. An entry will be added
        only if `add_to_console_view` is True and `console_view_entry` is
        provided.
      main_console_view - A string identifying the ID of the main console
        view to add an entry to. Supports a module-level default that
        defaults to None. An entry will be added only if
        `console_view_entry` is provided. Note that `add_to_console_view`
        has no effect on creating an entry to the main console view.
      cq_mirrors_console_view - A string identifying the ID of the CQ
        mirrors console view to add an entry to. Supports a module-level
        default that defaults to None. An entry will be added only if
        `console_view_entry` is provided. Note that `add_to_console_view`
        has no effect on creating an entry to the main console view.
      console_view_entry - A structure providing the details of the entry
        to add to the console view. See `ci.console_view_entry` for details.
      tree_closing - If true, failed builds from this builder that meet certain
        criteria will close the tree and email the sheriff. See the
        'chromium-tree-closer' config in notifiers.star for the full criteria.
      notifies - Any extra notifiers to attach to this builder.
    """
    if not branches.matches(branch_selector):
        return

    # Branch builders should never close the tree, only builders from the main
    # "ci" bucket.
    bucket = defaults.get_value_from_kwargs("bucket", kwargs)
    if tree_closing and bucket == "ci":
        notifies = (notifies or []) + ["chromium-tree-closer", "chromium-tree-closer-email"]

    # Define the builder first so that any validation of luci.builder arguments
    # (e.g. bucket) occurs before we try to use it
    builders.builder(
        name = name,
        branch_selector = branch_selector,
        resultdb_bigquery_exports = [resultdb.export_test_results(
            bq_table = "luci-resultdb.chromium.ci_test_results",
        )],
        notifies = notifies,
        **kwargs
    )

    if console_view_entry:
        console_view = defaults.get_value("console_view", console_view)
        if console_view == args.COMPUTE:
            console_view = defaults.get_value_from_kwargs("builder_group", kwargs)

        if console_view:
            add_to_console_view = defaults.get_value(
                "add_to_console_view",
                add_to_console_view,
            )

            builder = "{}/{}".format(bucket, name)

            if add_to_console_view:
                luci.console_view_entry(
                    builder = builder,
                    console_view = console_view,
                    category = console_view_entry.category,
                    short_name = console_view_entry.short_name,
                )

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
        os = builders.os.MAC_10_14,
        ssd = True,
        properties = {
            "xcode_build_version": "11a1027",
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
        caches = None,
        executable = "recipe:chromium",
        goma_backend = builders.goma.backend.RBE_PROD,
        os = builders.os.MAC_10_15,
        properties = None,
        **kwargs):
    # Default cache and properties sync
    caches = caches or [builders.xcode_cache.x12a7209]

    properties = properties or {}
    properties.setdefault("xcode_build_version", "12a7209")

    return fyi_builder(
        name = name,
        caches = caches,
        cores = None,
        executable = executable,
        os = os,
        properties = properties,
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
        caches = None,
        executable = "recipe:chromium",
        goma_backend = builders.goma.backend.RBE_PROD,
        properties = None,
        **kwargs):
    caches = caches or [builders.xcode_cache.x12a7209]

    properties = properties or {}
    properties.setdefault("xcode_build_version", "12a7209")

    return mac_builder(
        name = name,
        caches = caches,
        goma_backend = goma_backend,
        executable = executable,
        os = builders.os.MAC_10_15,
        properties = properties,
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
        os = builders.os.WINDOWS_DEFAULT,
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
    builder = ci_builder,
    console_view = console_view,
    console_view_entry = console_view_entry,
    declare_bucket = declare_bucket,
    defaults = defaults,
    overview_console_view = overview_console_view,
    ordering = ordering,
    set_defaults = set_defaults,
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
    win_builder = win_builder,
)
