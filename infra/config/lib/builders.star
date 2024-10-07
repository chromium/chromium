# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining builders.

The `builder` function defined in this module simplifies setting all of the
dimensions and many of the properties used for chromium builders by providing
direct arguments for them rather than requiring them to appear as part of a
dict. This simplifies creating wrapper functions that need to fix or override
the default value of specific dimensions or property fields without having to
handle merging dictionaries. Can also be accessed through `builders.builder`.

The `defaults` struct provides module-level defaults for the arguments to
`builder`. Each parameter of `builder` besides `name` and `kwargs` have a
corresponding attribute on `defaults` that is a `lucicfg.var` that can be used
to set the default value. Additionally, the module-level defaults defined for
use with `luci.builder` can be accessed through `defaults`. Finally module-level
defaults are provided for the `bucket` and `executable` arguments, removing the
need to create a wrapper function just to set those default values for a bucket.
Can also be accessed through `builders.defaults`.

The `cpu`, and `os` module members are structs that provide constants
for use with the corresponding arguments to `builder`. Can also be accessed
through `builders.cpu` and `builders.os` respectively.
"""

load("//project.star", "settings")
load("./args.star", "args")
load("./bootstrap.star", "register_bootstrap")
load("./branches.star", "branches")
load("./builder_config.star", "register_builder_config")
load("./builder_exemptions.star", "exempted_from_description_builders")
load("./builder_health_indicators.star", "register_health_spec")
load("./consoles.star", "register_builder_to_console_view")
load("./gn_args.star", "register_gn_args")
load("./nodes.star", "nodes")
load("./recipe_experiments.star", "register_recipe_experiments_ref")
load("./sheriff_rotations.star", "register_gardener_builder")

################################################################################
# Constants for use with the builder function                                  #
################################################################################

# The cpu constants to be used with the builder function
cpu = struct(
    ARM64 = "arm64",
    X86 = "x86",
    X86_64 = "x86-64",
)

# The category for an os: a more generic grouping than specific OS versions that
# can be used for computing defaults
os_category = struct(
    ANDROID = "Android",
    LINUX = "Linux",
    MAC = "Mac",
    WINDOWS = "Windows",
)

# The os constants to be used for the os parameter of the builder function
# The *_DEFAULT members enable distinguishing between a use that runs the
# "current" version of the OS and a use that runs against a specific version
# that happens to be the "current" version
def os_enum(category, dimension, dimension_overrides = None):
    if dimension_overrides:
        def get_dimension(bucket, builder):
            return dimension_overrides.get(bucket, {}).get(builder, dimension)
    else:
        def get_dimension(_bucket, _builder):
            return dimension

    return struct(category = category, get_dimension = get_dimension)

os = struct(
    ANDROID = os_enum(os_category.ANDROID, "Android"),
    LINUX_BIONIC = os_enum(os_category.LINUX, "Ubuntu-18.04"),
    LINUX_FOCAL = os_enum(os_category.LINUX, "Ubuntu-20.04"),
    # A migration off of bionic is in progress, builders identified in
    # linux-default.json will have a different os dimension
    LINUX_DEFAULT = os_enum(os_category.LINUX, "Ubuntu-22.04", json.decode(io.read_file("./linux-default.json"))),
    LINUX_NOBLE = os_enum(os_category.LINUX, "Ubuntu-24.04"),
    LINUX_UBUNTU_ANY = os_enum(os_category.LINUX, "Ubuntu"),
    LINUX_ANY = os_enum(os_category.LINUX, "Linux"),
    MAC_10_15 = os_enum(os_category.MAC, "Mac-10.15"),
    MAC_12 = os_enum(os_category.MAC, "Mac-12"),
    MAC_13 = os_enum(os_category.MAC, "Mac-13"),
    MAC_14 = os_enum(os_category.MAC, "Mac-14"),
    MAC_DEFAULT = os_enum(os_category.MAC, "Mac-14"),
    MAC_ANY = os_enum(os_category.MAC, "Mac"),
    MAC_BETA = os_enum(
        os_category.MAC,
        "Mac-15" if settings.project.startswith("chromium") else "Mac-14",
    ),
    WINDOWS_10 = os_enum(os_category.WINDOWS, "Windows-10"),
    # TODO(crbug.com/41492657): remove after slow compile issue resolved.
    WINDOWS_10_1909 = os_enum(os_category.WINDOWS, "Windows-10-18363"),
    WINDOWS_11 = os_enum(os_category.WINDOWS, "Windows-11"),
    WINDOWS_DEFAULT = os_enum(os_category.WINDOWS, "Windows-10"),
    WINDOWS_ANY = os_enum(os_category.WINDOWS, "Windows"),
)

siso = struct(
    project = struct(
        DEFAULT_TRUSTED = "rbe-chromium-trusted",
        TEST_TRUSTED = "rbe-chromium-trusted-test",
        DEFAULT_UNTRUSTED = "rbe-chromium-untrusted",
        TEST_UNTRUSTED = "rbe-chromium-untrusted-test",
    ) if settings.project.startswith("chromium") else struct(
        DEFAULT_TRUSTED = "rbe-chrome-trusted",
        TEST_TRUSTED = "rbe-chrome-trusted-test",
        DEFAULT_UNTRUSTED = "rbe-chrome-untrusted",
        TEST_UNTRUSTED = "rbe-chrome-untrusted-test",
    ),
    remote_jobs = struct(
        DEFAULT = 250,
        LOW_JOBS_FOR_CI = 80,
        HIGH_JOBS_FOR_CI = 500,
        LOW_JOBS_FOR_CQ = 150,
        # Calculated based on the number of CPUs inside Siso.
        HIGH_JOBS_FOR_CQ = -1 if settings.project.startswith("chromium") else 300,
    ),
)

def _rotation(name):
    return branches.value(
        branch_selector = branches.selector.MAIN,
        value = [name],
    )

# Gardener rotations that a builder can be added to (only takes effect on trunk)
# New rotations can be added, but won't automatically show up in SoM without
# changes to SoM code.
gardener_rotations = struct(
    ANDROID = _rotation("android"),
    ANGLE = _rotation("angle"),
    CHROMIUM = _rotation("chromium"),
    CFT = _rotation("cft"),
    DAWN = _rotation("dawn"),
    FUCHSIA = _rotation("fuchsia"),
    CHROMIUM_CLANG = _rotation("chromium.clang"),
    CHROMIUM_GPU = _rotation("chromium.gpu"),
    CHROMIUM_PERF = _rotation("chromium.perf"),
    IOS = _rotation("ios"),
    CHROMIUMOS = _rotation("chromiumos"),  # This group is not on SoM.
    LACROS_SKYLAB = _rotation("lacros_skylab"),
)

# Free disk space in a machine reserved for build tasks.
# The values in this enum will be used to populate bot dimension "free_space",
# and each bot will allocate a corresponding amount of free disk space based on
# the value of the dimension through "bot_config.py".
free_space = struct(
    standard = "standard",
    high = "high",
)

################################################################################
# Implementation details                                                       #
################################################################################

_DEFAULT_BUILDERLESS_OS_CATEGORIES = [os_category.LINUX, os_category.WINDOWS]

# Macs all have SSDs, so it doesn't make sense to use the default behavior of
# setting ssd:0 dimension
_EXCLUDE_BUILDERLESS_SSD_OS_CATEGORIES = [os_category.MAC]

def _code_coverage_property(
        *,
        coverage_gs_bucket,
        use_clang_coverage,
        use_java_coverage,
        use_javascript_coverage,
        coverage_exclude_sources,
        coverage_test_types,
        export_coverage_to_zoss,
        generate_blame_list):
    code_coverage = {}

    coverage_gs_bucket = defaults.get_value(
        "coverage_gs_bucket",
        coverage_gs_bucket,
    )
    if coverage_gs_bucket:
        code_coverage["coverage_gs_bucket"] = coverage_gs_bucket

    use_clang_coverage = defaults.get_value(
        "use_clang_coverage",
        use_clang_coverage,
    )
    if use_clang_coverage:
        code_coverage["use_clang_coverage"] = True

    use_java_coverage = defaults.get_value("use_java_coverage", use_java_coverage)
    if use_java_coverage:
        code_coverage["use_java_coverage"] = True

    use_javascript_coverage = defaults.get_value("use_javascript_coverage", use_javascript_coverage)
    if use_javascript_coverage:
        code_coverage["use_javascript_coverage"] = True

    coverage_exclude_sources = defaults.get_value(
        "coverage_exclude_sources",
        coverage_exclude_sources,
    )
    if coverage_exclude_sources:
        code_coverage["coverage_exclude_sources"] = coverage_exclude_sources

    coverage_test_types = defaults.get_value(
        "coverage_test_types",
        coverage_test_types,
    )
    if coverage_test_types:
        code_coverage["coverage_test_types"] = coverage_test_types

    export_coverage_to_zoss = defaults.get_value(
        "export_coverage_to_zoss",
        export_coverage_to_zoss,
    )
    if export_coverage_to_zoss:
        code_coverage["export_coverage_to_zoss"] = export_coverage_to_zoss

    generate_blame_list = defaults.get_value(
        "generate_blame_list",
        generate_blame_list,
    )
    if generate_blame_list:
        code_coverage["generate_blame_list"] = generate_blame_list

    return code_coverage or None

def _pgo_property(*, use_pgo, skip_profile_upload):
    pgo = {}

    use_pgo = defaults.get_value("use_pgo", use_pgo)
    if use_pgo:
        pgo["use_pgo"] = True
        skip_profile_upload = defaults.get_value(
            "skip_profile_upload",
            skip_profile_upload,
        )
        if skip_profile_upload:
            pgo["skip_profile_upload"] = True

    return pgo or None

_VALID_REPROXY_ENV_PREFIX_LIST = ["RBE_", "GLOG_", "GOMA_"]

def _reclient_property(*, instance, service, jobs, rewrapper_env, profiler_service, publish_trace, cache_silo, ensure_verified, bootstrap_env, scandeps_server, disable_bq_upload):
    reclient = {}
    if not instance:
        return None
    reclient["instance"] = instance
    reclient["metrics_project"] = "chromium-reclient-metrics"
    service = defaults.get_value("reclient_service", service)
    if service:
        reclient["service"] = service
    jobs = defaults.get_value("reclient_jobs", jobs)
    if jobs:
        reclient["jobs"] = jobs
    rewrapper_env = defaults.get_value("reclient_rewrapper_env", rewrapper_env)
    if rewrapper_env:
        for k in rewrapper_env:
            if not k.startswith("RBE_"):
                fail("Environment variables in rewrapper_env must start with " +
                     "'RBE_', got '%s'" % k)
        reclient["rewrapper_env"] = rewrapper_env
    bootstrap_env = defaults.get_value("reclient_bootstrap_env", bootstrap_env)
    if bootstrap_env:
        for k in bootstrap_env:
            if not any([k.startswith(prefix) for prefix in _VALID_REPROXY_ENV_PREFIX_LIST]):
                fail("Environment variables in bootstrap_env must start with one of (" +
                     ", ".join(_VALID_REPROXY_ENV_PREFIX_LIST) +
                     "), got '%s'" % k)
        reclient["bootstrap_env"] = bootstrap_env
    if scandeps_server:
        reclient["scandeps_server"] = scandeps_server
    profiler_service = defaults.get_value("reclient_profiler_service", profiler_service)
    if profiler_service:
        reclient["profiler_service"] = profiler_service
    publish_trace = defaults.get_value("reclient_publish_trace", publish_trace)
    if publish_trace:
        reclient["publish_trace"] = True
    if cache_silo:
        reclient["cache_silo"] = cache_silo
    ensure_verified = defaults.get_value("reclient_ensure_verified", ensure_verified)
    if ensure_verified:
        reclient["ensure_verified"] = True
    disable_bq_upload = defaults.get_value("reclient_disable_bq_upload", disable_bq_upload)
    if disable_bq_upload:
        reclient["disable_bq_upload"] = True

    return reclient

def _resultdb_settings(*, resultdb_enable, resultdb_bigquery_exports, resultdb_index_by_timestamp):
    resultdb_enable = defaults.get_value("resultdb_enable", resultdb_enable)
    if not resultdb_enable:
        return None

    history_options = None
    resultdb_index_by_timestamp = defaults.get_value(
        "resultdb_index_by_timestamp",
        resultdb_index_by_timestamp,
    )
    if resultdb_index_by_timestamp:
        history_options = resultdb.history_options(
            by_timestamp = resultdb_index_by_timestamp,
        )

    return resultdb.settings(
        enable = True,
        bq_exports = defaults.get_value(
            "resultdb_bigquery_exports",
            resultdb_bigquery_exports,
        ),
        history_options = history_options,
    )

################################################################################
# Builder defaults and function                                                #
################################################################################

# The module-level defaults to use with the builder function
defaults = args.defaults(
    luci.builder.defaults,

    # Our custom arguments
    auto_builder_dimension = args.COMPUTE,
    bootstrap = True,
    builder_cache_name = None,
    builder_group = None,
    builderless = args.COMPUTE,
    free_space = None,
    cores = None,
    cpu = None,
    gce = None,
    fully_qualified_builder_dimension = False,
    console_view = args.COMPUTE,
    list_view = args.COMPUTE,
    os = None,
    pool = None,
    skip_profile_upload = False,
    gardener_rotations = None,
    xcode = None,
    ssd = args.COMPUTE,
    coverage_gs_bucket = None,
    use_clang_coverage = False,
    use_java_coverage = False,
    use_javascript_coverage = False,
    use_pgo = None,
    coverage_exclude_sources = None,
    coverage_test_types = None,
    export_coverage_to_zoss = False,
    generate_blame_list = False,
    resultdb_enable = True,
    resultdb_bigquery_exports = [],
    resultdb_index_by_timestamp = False,
    reclient_service = None,
    reclient_jobs = None,
    reclient_rewrapper_env = None,
    reclient_bootstrap_env = None,
    reclient_profiler_service = None,
    reclient_publish_trace = None,
    reclient_scandeps_server = args.COMPUTE,
    reclient_cache_silo = None,
    reclient_ensure_verified = None,
    reclient_disable_bq_upload = None,
    siso_enabled = None,
    siso_project = None,
    siso_configs = ["builder"],
    siso_enable_cloud_profiler = True,
    siso_enable_cloud_trace = True,
    siso_enable_cloud_monitoring = True,
    siso_experiments = [],
    siso_remote_jobs = None,
    siso_fail_if_reapi_used = None,
    siso_remote_linking = None,
    siso_output_local_strategy = None,
    health_spec = None,
    builder_config_settings = None,

    # Variables for modifying builder characteristics in a shadow bucket
    shadow_builderless = None,
    shadow_free_space = args.COMPUTE,  # None will clear the non-shadow dimension, so use args.COMPUTE as the default
    shadow_pool = None,
    shadow_service_account = None,
    shadow_siso_project = None,
    shadow_properties = {},

    # Provide vars for bucket and executable so users don't have to
    # unnecessarily make wrapper functions
    bucket = args.COMPUTE,
    executable = args.COMPUTE,
    notifies = None,
    triggered_by = args.COMPUTE,
    contact_team_email = None,
)

# This node won't actually be accessed, but creating it for builders that have
# builder_group set will enforce that there can't be builders with the same name
# in different buckets that use the same builder group since lucicfg will check
# that there aren't two graphs with the same ID
_BUILDER_GROUP_ID_NODE = nodes.create_unscoped_node_type("builder-group-id")

# For staging, we specifically want to reuse the same builder group so that the
# staging builders look up the same GN args and targets that the prod official
# builders use
_BUILDER_GROUP_REUSE_BUCKET_ALLOWLIST = [
    "official.diffs.staging",
    "official.infra.staging",
    "official.staging",
] if settings.project.startswith("chrome") else []

def builder(
        *,
        name,
        branch_selector = branches.selector.MAIN,
        bucket = args.DEFAULT,
        executable = args.DEFAULT,
        notifies = args.DEFAULT,
        triggered_by = args.DEFAULT,
        os = args.DEFAULT,
        builderless = args.DEFAULT,
        free_space = args.DEFAULT,
        builder_cache_name = args.DEFAULT,
        override_builder_dimension = None,
        auto_builder_dimension = args.DEFAULT,
        fully_qualified_builder_dimension = args.DEFAULT,
        gce = args.DEFAULT,
        cores = args.DEFAULT,
        cpu = args.DEFAULT,
        bootstrap = args.DEFAULT,
        builder_group = args.DEFAULT,
        builder_spec = None,
        mirrors = None,
        builder_config_settings = args.DEFAULT,
        pool = args.DEFAULT,
        ssd = args.DEFAULT,
        gardener_rotations = None,
        xcode = args.DEFAULT,
        console_view_entry = None,
        list_view = args.DEFAULT,
        coverage_gs_bucket = args.DEFAULT,
        use_clang_coverage = args.DEFAULT,
        use_java_coverage = args.DEFAULT,
        use_javascript_coverage = args.DEFAULT,
        use_pgo = args.DEFAULT,
        coverage_exclude_sources = args.DEFAULT,
        coverage_test_types = args.DEFAULT,
        export_coverage_to_zoss = args.DEFAULT,
        generate_blame_list = args.DEFAULT,
        resultdb_enable = args.DEFAULT,
        resultdb_bigquery_exports = args.DEFAULT,
        resultdb_index_by_timestamp = args.DEFAULT,
        reclient_service = args.DEFAULT,
        reclient_jobs = args.DEFAULT,
        reclient_rewrapper_env = args.DEFAULT,
        reclient_bootstrap_env = args.DEFAULT,
        reclient_profiler_service = args.DEFAULT,
        reclient_publish_trace = args.DEFAULT,
        reclient_scandeps_server = args.DEFAULT,
        reclient_cache_silo = None,
        reclient_ensure_verified = None,
        reclient_disable_bq_upload = None,
        siso_enabled = args.DEFAULT,
        siso_project = args.DEFAULT,
        siso_configs = args.DEFAULT,
        siso_enable_cloud_profiler = args.DEFAULT,
        siso_enable_cloud_trace = args.DEFAULT,
        siso_enable_cloud_monitoring = args.DEFAULT,
        siso_experiments = args.DEFAULT,
        siso_remote_jobs = args.DEFAULT,
        siso_fail_if_reapi_used = None,
        siso_output_local_strategy = args.DEFAULT,
        siso_remote_linking = args.DEFAULT,
        skip_profile_upload = args.DEFAULT,
        health_spec = args.DEFAULT,
        shadow_builderless = args.DEFAULT,
        shadow_free_space = args.DEFAULT,
        shadow_pool = args.DEFAULT,
        shadow_service_account = args.DEFAULT,
        shadow_siso_project = args.DEFAULT,
        shadow_properties = args.DEFAULT,
        gn_args = None,
        targets = None,
        targets_settings = None,
        contact_team_email = args.DEFAULT,
        **kwargs):
    """Define a builder.

    For all of the optional parameters defined by this method, passing None will
    prevent the emission of any dimensions or property fields associated with
    that parameter.

    All parameters defined by this function except for `name` and `kwargs` support
    module-level defaults. The `defaults` struct defined in this module has an
    attribute with a `lucicfg.var` for all of the fields defined here as well as
    all of the parameters of `luci.builder` that support module-level defaults.

    See https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/lucicfg/doc/README.md#luci.builder
    for more information.

    Args:
        name: name of the builder, will show up in UIs and logs. Required.
        branch_selector: A branch selector value controlling whether the
            builder definition is executed. See branches.star for more
            information.
        bucket: a bucket the build is in, see luci.bucket(...) rule. Required
            (may be specified by module-level default).
        executable: an executable to run, e.g. a luci.recipe(...). Required (may
            be specified by module-level default).
        notifies: A string or list of strings with notifiers that will be
            triggered for builds of the builder. Supports a module-level default
            that will be merged with the provided values.
        triggered_by: an optional poller or builder that triggers the builder or
            a list of pollers and/or builders that trigger the builder. Supports
            a module-level default.
        os: a member of the `os` enum indicating the OS the builder requires for
            the machines that run it. Emits a dimension of the form 'os:os'. By
            default considered None.
        builderless: a boolean indicating whether the builder runs on
            builderless machines. If True, emits a 'builderless:1' dimension. By
            default, considered True iff `os` refers to a linux OS.
        free_space: an enum that indicates the amount of free disk space reserved
            in a machine for incoming build tasks. This value is used to create
            a "free_space" dimension, and this dimension is appended to only
            builderless builders.
        builder_cache_name: The name of a cache to mount as the builder cache. Emits
            a cache declaration of the form
            ```{
              name: <builder_cache>
              path: "builder"
            }```. By default, the default buildbucket builder cache will be used,
            which uses a cache name based on a hash of the builder's project, bucket
            and name. This can be used to share the builder cache between multiple
            builders, but care must be taken that the builders can effectively share
            the cache (use same gclient config, use the same GN args or a separate
            output directory, etc.). Sharing a cache between builders limits
            swarming ability to clear space because it only operates at a cache
            level, so if it needs to remove the cache, it will affect multiple
            builders.
        override_builder_dimension: a string to assign to the "builder"
            dimension. Ignores any other "builder" and "builderless" dimensions
            that would have been assigned.
        auto_builder_dimension: a boolean indicating whether the builder runs on
            machines devoted to the builder. If True, a dimension will be
            emitted of the form 'builder:<name>'. By default, considered True
            iff `builderless` is considered False.
        fully_qualified_builder_dimension: a boolean modifying the behavior of
            auto_builder_dimension to generate a builder dimensions that is
            fully-qualified with the project and bucket of the builder. If True,
            and `auto_builder_dimension` is considered True, a dimension will be
            emitted of the form 'builder:<project>/<bucket>/<name>'. By default,
            considered False.
        cores: an int indicating the number of cores the builder requires for
            the machines that run it. Emits a dimension of the form
            'cores:<cores>' will be emitted. By default, considered None.
        cpu: a member of the `cpu` enum indicating the cpu the builder requires
            for the machines that run it. Emits a dimension of the form
            'cpu:<cpu>'. By default, considered None.
        bootstrap: a boolean indicating whether the builder should have its
            properties bootstrapped. If True, the builder's properties will be
            written to a separate file and its definition will be updated with
            new properties and executable that cause a bootstrapping binary to
            be used. The build's default values for properties will be taken
            from the properties file at the version that the build will check
            out.
        builder_group: a string with the group of the builder. Emits a property
            of the form 'builder_group:<builder_group>'. By default, considered
            None.
        builder_spec: The spec describing the configuration for the builder.
            Cannot be set if `mirrors` is set.
        mirrors: References to the builders that the builder should mirror.
            Cannot be set if `builder_spec` is set.
        builder_config_settings: Additional builder configuration that used by
            the recipes. Could be an instance of ci_settings or try_settings.
            It can only be set if one of builder_spec or mirrors is set.
        pool: a string indicating the pool of the machines that run the builder.
            Emits a dimension of the form 'pool:<pool>'. By default, considered
            None. When running a builder that has no explicit pool dimension,
            buildbucket inserts one of the form 'pool:luci.<project>.<bucket>'.
        ssd: a boolean indicating whether the builder runs on machines with ssd.
            If True, emits a 'ssd:1' dimension. If False, emits a 'ssd:0'
            parameter. By default, considered False if builderless is considered
            True and otherwise None.
        gardener_rotations: A string or list of strings identifying the gardener
            rotations that the builder should be included in. Will be merged
            with the module-level default.
        xcode: a member of the `xcode` enum indicating the xcode version the
            builder requires. Emits a cache declaration of the form
            ```{
              name: <xcode.cache_name>
              path: <xcode.cache_path>
            }```.
            Also emits a 'xcode_build_version:<xcode.version>' property if the
            property is not already set.
        console_view_entry: A `consoles.console_view_entry` struct or a list of
            them describing console view entries to create for the builder.
            See `consoles.console_view_entry` for details.
        list_view: A string or a list of strings identifying the ID(s) of the
            list view(s) to add an entry to. Supports a module-level default
            that defaults to no list views.
        gce: A boolean indicating whether the builder runs on GCE machines.
            If True, emits a 'gce:1' dimension. If False, emits a 'gce:0'
            dimension. If None, 'gce' dimension is not emitted, meaning don't
            care if running on GCE machines or not. By default, considered None.
        coverage_gs_bucket: a string specifying the GS bucket to upload
            coverage data to. Will be copied to '$build/code_coverage' property.
            By default, considered None.
        use_clang_coverage: a boolean indicating whether clang coverage should
            be used. If True, the 'use_clang_coverage" field will be set in the
            '$build/code_coverage' property. By default, considered False.
        use_java_coverage: a boolean indicating whether java coverage should be
            used. If True, the 'use_java_coverage" field will be set in the
            '$build/code_coverage' property. By default, considered False.
        use_javascript_coverage: a boolean indicating whether javascript
            coverage should be enabled. If True the 'use_javascript_coverage'
            field will be set in the '$build/code_coverage' property. By
            default, considered False.
        use_pgo: a boolean indicating whether PGO should be used. If True, the
            'use_pgo' will be set in '$build/pgo' property. Defaults to False.
        coverage_exclude_sources: a string as the key to find the source file
            exclusion pattern in code_coverage recipe module. Will be copied to
            '$build/code_coverage' property if set. By default, considered None.
        coverage_test_types: a list of string as test types to process data for
            in code_coverage recipe module. Will be copied to
            '$build/code_coverage' property. By default, considered None.
        export_coverage_to_zoss: a boolean indicating if the raw coverage data
            be exported zoss(and eventually in code search) in code_coverage
            recipe module. Will be copied to '$build/code_coverage' property
            if set. By default, considered False.
        generate_blame_list: a boolean indicating if blame list data for
            files whose coverage is known gets generated and exported to GCS.
            Will be copied to '$build/code_coverage' property if set.
            By default considered False.
        resultdb_enable: a boolean indicating if resultdb should be enabled for
            the builder.
        resultdb_bigquery_exports: a list of resultdb.export_test_results(...)
            specifying parameters for exporting test results to BigQuery. By
            default, do not export.
        resultdb_index_by_timestamp: a boolean specifying whether ResultDB
            should index the results of the tests run on this builder by
            timestamp, i.e. for purposes of retrieving a test's history. If
            false, the results will not be searchable by timestamp on ResultDB's
            test history api.
        reclient_service: a string indicating the RBE service to dial via gRPC.
            By default, this is "remotebuildexecution.googleapis.com:443" (set
            in the reclient recipe module). Has no effect if reclient_instance
            is not set.
        reclient_jobs: an integer indicating the number of concurrent
            compilations to run when using re-client as the compiler. Has no
            effect if reclient_instance is not set.
        reclient_rewrapper_env: a map that sets the rewrapper flags via the
            environment variables. All such vars must start with the "RBE_"
            prefix. Has no effect if reclient_instance is not set.
        reclient_bootstrap_env: a map that sets the bootstrap flags via the
            environment variables. All such vars must start with the "RBE_"
            prefix. Has no effect if reclient_instance is not set.
        reclient_profiler_service: a string indicating service name for
            re-client's cloud profiler. Has no effect if reclient_instance is
            not set.
        reclient_publish_trace: If True, it publish trace by rpl2cloudtrace. Has
            no effect if reclient_instance is not set.
        reclient_scandeps_server: If true, reproxy should start its own scandeps_server
        reclient_cache_silo: A string indicating a cache siling key to use for
            remote caching. Has no effect if reclient_instance is not set.
        reclient_ensure_verified: If True, it verifies build artifacts. Has no
            effect if reclient_instance is not set.
        reclient_disable_bq_upload: If True, rbe_metrics will not be uploaded to
            BigQuery after each build
        siso_enabled: If True, $build/siso properties will be set, and Siso will
            be used at compile step.
        siso_project: a string indicating the GCP project hosting the RBE
            instance and Cloud logging/trace/profile for Siso to use.
        siso_configs: a list of siso configs to enable. available values are defined in
            //build/config/siso/config.star.
        siso_enable_cloud_profiler: If True, enable cloud profiler in Siso.
        siso_enable_cloud_trace: If True, enable cloud trace in Siso.
        siso_enable_cloud_monitoring: If true, enable cloud monitoring in Siso.
            When Siso uses Reclient for remote executions, this flag is noop because
            Reclient already sends metrics to Cloud monitoring.
        siso_experiments: a list of experiment flags for siso.
        siso_remote_jobs: an integer indicating the number of concurrent remote jobs
            to run when building with Siso.
        siso_fail_if_reapi_used: If True, check siso_metrics.json to see if the build
            used remote execution and fail the build if any step used it.
        siso_output_local_strategy: a string indicating the output strategy
            for `--output_local_strategy`. full, greedy or minimum.
        siso_remote_linking: If True, enable remote linking. Siso has to use the
            builtin RBE client instead of Reclient. Relevant configs and GN args
            will be adjusted accordingly.
        health_spec: a health spec instance describing the threshold for when
            the builder should be considered unhealthy.
        shadow_builderless: If set to True, then led builds created for this
            builder will have the builderless dimension set and the builder
            dimension removed. If set to False, then led builds created for this
            builder will have the builderless dimension removed and the builder
            dimension set. See description of builderless and
            auto_builder_dimension for more information.
        shadow_free_space: If set, then led builds created for this builder will
            use the specified value for the free_space dimension. None will
            cause the free_space dimension to be removed for led builds. See
            description of free_space for more information.
        shadow_pool: If set, then led builds created for this Builder will be
            set to use this alternate pool instead.
        shadow_service_account: If set, then led builds created for this builder
            will use this service account instead.
        shadow_siso_project: If set, then led builds for this builder will
            use the RBE and other cloud instances of this project instead of the
            ones of siso_project. The other reclient, siso values will continue
            to be used for the shadow build.
        shadow_properties: If set, the led builds created for this Builder will
            override the top-level input properties with the same keys.
        gn_args: If set, the GN args config to use for the builder. It can be
            set to the name of a predeclared config or an unnamed
            gn_args.config declaration for an unphased config. A builder can use
            phased configs by setting a dict with the phase names as keys and
            the values being the config to use for the phase.
        targets: The targets that should be built and/or run by the builder. Can
            take the form of the name of a targets bundle (individual targets
            define a bundle with the same name containing only that target), a
            targets.bundle instance or a list where each element is the name of
            a targets bundle or a targets.bundle instance.
        targets_settings: The settings to use when expanding the targets for the
            builder.
        contact_team_email: The e-mail of the team responsible for the health of
            the builder.
        **kwargs: Additional keyword arguments to forward on to `luci.builder`.

    Returns:
        The lucicfg keyset for the builder
    """

    # We don't have any need of an explicit dimensions dict,
    # instead we have individual arguments for dimensions
    if "dimensions" in kwargs:
        fail("Explicit dimensions are not supported: " +
             "use builderless, cores, cpu, os or ssd instead")

    if builder_spec and mirrors:
        fail("Only one of builder_spec or mirrors can be set")

    dimensions = {}

    properties = kwargs.pop("properties", {})
    if "gardener_rotations" in properties:
        fail('Setting "gardener_rotations" property is not supported: ' +
             "use gardener_rotations instead")
    if "$build/code_coverage" in properties:
        fail('Setting "$build/code_coverage" property is not supported: ' +
             "use coverage_gs_bucket, use_clang_coverage, use_java_coverage, " +
             "use_javascript_coverage, coverage_exclude_sources, " +
             "coverage_test_types instead")
    if "$build/reclient" in properties:
        fail('Setting "$build/reclient" property is not supported: ' +
             "use reclient_instance and reclient_rewrapper_env instead")
    if "$build/pgo" in properties:
        fail('Setting "$build/pgo" property is not supported: ' +
             "use use_pgo and skip_profile_upload instead")
    properties = dict(properties)

    shadow_properties = dict(defaults.get_value("shadow_properties", shadow_properties))

    # bucket might be the args.COMPUTE sentinel value if the caller didn't set
    # bucket in some way, which will result in a weird fully-qualified builder
    # dimension, but it shouldn't matter because the call to luci.builder will
    # fail without bucket being set
    bucket = defaults.get_value("bucket", bucket)

    os = defaults.get_value("os", os)
    if os:
        dimensions["os"] = os.get_dimension(bucket, name)

    if override_builder_dimension:
        dimensions["builder"] = override_builder_dimension
    else:
        builderless = defaults.get_value("builderless", builderless)
        if builderless == args.COMPUTE:
            builderless = os != None and os.category in _DEFAULT_BUILDERLESS_OS_CATEGORIES
        if builderless:
            dimensions["builderless"] = "1"

            free_space = defaults.get_value("free_space", free_space)
            if free_space:
                dimensions["free_space"] = free_space
        elif free_space and free_space != args.DEFAULT:
            fail("\'free_space\' dimension can only be specified for builderless builders")

        auto_builder_dimension = defaults.get_value(
            "auto_builder_dimension",
            auto_builder_dimension,
        )
        if auto_builder_dimension == args.COMPUTE:
            auto_builder_dimension = builderless == False
        if auto_builder_dimension:
            fully_qualified_builder_dimension = defaults.get_value("fully_qualified_builder_dimension", fully_qualified_builder_dimension)
            if fully_qualified_builder_dimension:
                dimensions["builder"] = "{}/{}/{}".format(settings.project, bucket, name)
            else:
                dimensions["builder"] = name

    if not kwargs.get("description_html", "").strip() and name not in exempted_from_description_builders.get(bucket, []) and not mirrors:
        fail("Builder " + name + " must have a description_html. All new builders must specify a description.")
    elif kwargs.get("description_html", "").strip() and name in exempted_from_description_builders.get(bucket, []):
        fail("Need to remove builder " + bucket + "/" + name + " from exempted_from_description_builders")

    cores = defaults.get_value("cores", cores)
    if cores != None:
        dimensions["cores"] = str(cores)

    cpu = defaults.get_value("cpu", cpu)
    if cpu != None:
        dimensions["cpu"] = cpu

    builder_group = defaults.get_value("builder_group", builder_group)
    if builder_group != None:
        properties["builder_group"] = builder_group

    pool = defaults.get_value("pool", pool)
    if pool:
        dimensions["pool"] = pool

    gardener_rotations = defaults.get_value("gardener_rotations", gardener_rotations, merge = args.MERGE_LIST)
    if gardener_rotations:
        # TODO(343503161): Remove gardener_rotations after SoM is updated.
        properties["sheriff_rotations"] = gardener_rotations
        properties["gardener_rotations"] = gardener_rotations

    ssd = defaults.get_value("ssd", ssd)
    if ssd == args.COMPUTE:
        ssd = None
        if (builderless and os != None and
            os.category not in _EXCLUDE_BUILDERLESS_SSD_OS_CATEGORIES):
            ssd = False
    if ssd != None:
        dimensions["ssd"] = "1" if ssd else "0"

    gce = defaults.get_value("gce", gce)
    if gce != None:
        dimensions["gce"] = "1" if gce else "0"

    code_coverage = _code_coverage_property(
        coverage_gs_bucket = coverage_gs_bucket,
        use_clang_coverage = use_clang_coverage,
        use_java_coverage = use_java_coverage,
        use_javascript_coverage = use_javascript_coverage,
        coverage_exclude_sources = coverage_exclude_sources,
        coverage_test_types = coverage_test_types,
        export_coverage_to_zoss = export_coverage_to_zoss,
        generate_blame_list = generate_blame_list,
    )
    if code_coverage != None:
        properties["$build/code_coverage"] = code_coverage

    reclient_scandeps_server = defaults.get_value(
        "reclient_scandeps_server",
        reclient_scandeps_server,
    )

    # Enable scandeps_server by default for Chromium.
    if reclient_scandeps_server == args.COMPUTE:
        reclient_scandeps_server = settings.project.startswith("chromium") or (os and os.category == os_category.MAC)

    rbe_project = defaults.get_value("siso_project", siso_project)
    shadow_rbe_project = defaults.get_value("shadow_siso_project", shadow_siso_project)
    reclient = _reclient_property(
        instance = rbe_project,
        service = reclient_service,
        jobs = reclient_jobs,
        rewrapper_env = reclient_rewrapper_env,
        bootstrap_env = reclient_bootstrap_env,
        profiler_service = reclient_profiler_service,
        publish_trace = reclient_publish_trace,
        scandeps_server = reclient_scandeps_server,
        cache_silo = reclient_cache_silo,
        ensure_verified = reclient_ensure_verified,
        disable_bq_upload = reclient_disable_bq_upload,
    )
    if reclient != None:
        properties["$build/reclient"] = reclient
        shadow_reclient_instance = shadow_rbe_project
        shadow_reclient = _reclient_property(
            instance = shadow_reclient_instance,
            service = reclient_service,
            jobs = reclient_jobs,
            rewrapper_env = reclient_rewrapper_env,
            bootstrap_env = reclient_bootstrap_env,
            profiler_service = reclient_profiler_service,
            publish_trace = reclient_publish_trace,
            scandeps_server = reclient_scandeps_server,
            cache_silo = reclient_cache_silo,
            ensure_verified = reclient_ensure_verified,
            disable_bq_upload = reclient_disable_bq_upload,
        )
        if shadow_reclient:
            shadow_properties["$build/reclient"] = shadow_reclient
            shadow_rbe_project = shadow_reclient["instance"]
    use_siso = defaults.get_value("siso_enabled", siso_enabled) and rbe_project
    use_siso_remote_linking = use_siso and defaults.get_value("siso_remote_linking", siso_remote_linking)
    if use_siso:
        siso = {
            "enable_cloud_profiler": defaults.get_value("siso_enable_cloud_profiler", siso_enable_cloud_profiler),
            "enable_cloud_trace": defaults.get_value("siso_enable_cloud_trace", siso_enable_cloud_trace),
            "experiments": defaults.get_value("siso_experiments", siso_experiments),
            "project": rbe_project,
        }
        remote_jobs = defaults.get_value("siso_remote_jobs", siso_remote_jobs)
        if remote_jobs:
            siso["remote_jobs"] = remote_jobs
        siso_configs = defaults.get_value("siso_configs", siso_configs)
        if use_siso_remote_linking:
            siso_configs = siso_configs + ["remote-link"]
        siso["configs"] = siso_configs
        if siso_fail_if_reapi_used:
            siso["fail_if_reapi_used"] = siso_fail_if_reapi_used
        siso_output_local_strategy = defaults.get_value("siso_output_local_strategy", siso_output_local_strategy)
        if not siso_output_local_strategy and use_siso_remote_linking:
            siso_output_local_strategy = "minimum"
        if siso_output_local_strategy:
            siso["output_local_strategy"] = siso_output_local_strategy

        # Since Siso's remote linking doesn't use Reclient, it needs to enable
        # Cloud Monitoring for monitoring and alerts.
        if defaults.get_value("siso_enable_cloud_monitoring", siso_enable_cloud_monitoring) and use_siso_remote_linking:
            siso["enable_cloud_monitoring"] = True

            # TODO: crbug.com/368518993 - It uses the same GCP project with
            # Reclient so that we can reuse the existing monitoring setup.
            # We need to consider migrating to chromium-build-stats or the
            # RBE project.
            siso["metrics_project"] = "chromium-reclient-metrics"
        properties["$build/siso"] = siso
        if shadow_rbe_project:
            shadow_siso = dict(siso)
            shadow_siso["project"] = shadow_rbe_project
            shadow_properties["$build/siso"] = shadow_siso

    pgo = _pgo_property(
        use_pgo = use_pgo,
        skip_profile_upload = skip_profile_upload,
    )
    if pgo != None:
        properties["$build/pgo"] = pgo

    shadow_dimensions = {}
    shadow_builderless = defaults.get_value("shadow_builderless", shadow_builderless)
    if shadow_builderless:
        shadow_dimensions["builderless"] = "1"
        shadow_dimensions["builder"] = None
    elif shadow_builderless != None:
        shadow_dimensions["builderless"] = None
        shadow_dimensions["builder"] = name
    shadow_free_space = defaults.get_value("shadow_free_space", shadow_free_space)
    if shadow_free_space != args.COMPUTE:
        shadow_dimensions["free_space"] = shadow_free_space
    shadow_pool = defaults.get_value("shadow_pool", shadow_pool)
    if shadow_pool != None:
        shadow_dimensions["pool"] = shadow_pool

    shadow_dimensions = {k: v for k, v in shadow_dimensions.items() if dimensions.get(k) != v}

    kwargs = dict(kwargs)
    if bucket != args.COMPUTE:
        kwargs["bucket"] = bucket
    executable = defaults.get_value("executable", executable)
    if executable != args.COMPUTE:
        kwargs["executable"] = executable

    caches = kwargs.pop("caches", None) or []
    builder_cache_name = defaults.get_value("builder_cache_name", builder_cache_name)
    if builder_cache_name:
        if any([c.path == "builder" for c in caches]):
            fail("Can't specify both 'builder_cache_name' and a cache with path 'builder'")
        caches.append(swarming.cache(
            name = builder_cache_name,
            path = "builder",
            wait_for_warm_cache = 4 * time.minute,
        ))
    xcode = defaults.get_value("xcode", xcode)
    if xcode:
        caches.append(xcode.cache)
        properties.setdefault("xcode_build_version", xcode.version)
    kwargs["caches"] = caches

    if notifies != None:
        kwargs["notifies"] = defaults.get_value("notifies", notifies, merge = args.MERGE_LIST)

    triggered_by = defaults.get_value("triggered_by", triggered_by)
    if triggered_by != args.COMPUTE:
        kwargs["triggered_by"] = triggered_by

    contact_team_email = defaults.get_value("contact_team_email", contact_team_email)
    builder = branches.builder(
        name = name,
        branch_selector = branch_selector,
        contact_team_email = contact_team_email,
        dimensions = dimensions,
        properties = properties,
        resultdb_settings = _resultdb_settings(
            resultdb_enable = resultdb_enable,
            resultdb_bigquery_exports = resultdb_bigquery_exports,
            resultdb_index_by_timestamp = resultdb_index_by_timestamp,
        ),
        shadow_dimensions = shadow_dimensions,
        shadow_service_account = defaults.get_value("shadow_service_account", shadow_service_account),
        shadow_properties = shadow_properties,
        **kwargs
    )

    # builder will be None if the builder isn't being defined due to the project
    # settings and the branch selector
    if builder == None:
        return None

    # Define a node to ensure there's only one builder using the
    # (builder_group, builder)
    if builder_group != None and bucket not in _BUILDER_GROUP_REUSE_BUCKET_ALLOWLIST:
        _BUILDER_GROUP_ID_NODE.add("{}:{}".format(builder_group, name))

    register_gardener_builder(bucket, name, gardener_rotations)

    register_recipe_experiments_ref(bucket, name, executable)

    # When Siso enables remote linking, it must use the builtin RBE client
    # instead of Reclient. Modify GN args inside register_gn_args().
    use_siso_rbe_client = use_siso_remote_linking
    additional_exclusions = register_gn_args(builder_group, bucket, name, gn_args, use_siso, use_siso_rbe_client)

    builder_config_settings = defaults.get_value(
        "builder_config_settings",
        builder_config_settings,
    )
    register_builder_config(
        bucket,
        name,
        builder_group,
        builder_spec,
        mirrors,
        builder_config_settings,
        targets,
        targets_settings,
        additional_exclusions,
        kwargs.get("description_html", "").strip(),
    )

    bootstrap = defaults.get_value("bootstrap", bootstrap)
    register_bootstrap(bucket, name, bootstrap, executable)

    health_spec = defaults.get_value("health_spec", health_spec)

    register_health_spec(bucket, name, health_spec, contact_team_email)

    builder_name = "{}/{}".format(bucket, name)

    if console_view_entry:
        if type(console_view_entry) == type(struct()):
            entries = [console_view_entry]
        else:
            entries = console_view_entry
        entries_without_console_view = [
            e
            for e in entries
            if e.console_view == None
        ]
        if len(entries_without_console_view) > 1:
            fail("Multiple entries provided without console_view: {}"
                .format(entries_without_console_view))

        for entry in entries:
            if not branches.matches(entry.branch_selector):
                continue

            console_view = entry.console_view
            if console_view == None:
                console_view = defaults.console_view.get()
                if console_view == args.COMPUTE:
                    console_view = builder_group
                if not console_view:
                    fail("Builder does not have builder group and " +
                         "console_view_entry does not have console view: {}".format(entry))

            register_builder_to_console_view(
                console_view,
                entry.category,
                entry.short_name,
                settings.project,
                bucket,
                builder_group,
                name,
            )

            luci.console_view_entry(
                builder = builder_name,
                console_view = console_view,
                category = entry.category,
                short_name = entry.short_name,
            )

    list_view = defaults.get_value("list_view", list_view)

    # The default for list_view is set to args.COMPUTE instead of None so that
    # the try builder function can override the default behavior
    if list_view == args.COMPUTE:
        list_view = None
    if list_view:
        if type(list_view) == type(""):
            list_view = [list_view]
        for view in list_view:
            luci.list_view_entry(
                builder = builder_name,
                list_view = view,
            )

    return builder

builders = struct(
    builder = builder,
    cpu = cpu,
    defaults = defaults,
    os = os,
    gardener_rotations = gardener_rotations,
    free_space = free_space,
)
