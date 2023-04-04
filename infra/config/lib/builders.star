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

The `cpu`, `os`, and `goma` module members are structs that provide constants
for use with the corresponding arguments to `builder`. Can also be accessed
through `builders.cpu`, `builders.os` and `builders.goma` respectively.
"""

load("//project.star", "settings")
load("./args.star", "args")
load("./branches.star", "branches")
load("./bootstrap.star", "register_bootstrap")
load("./builder_config.star", "register_builder_config")
load("./recipe_experiments.star", "register_recipe_experiments_ref")
load("./sheriff_rotations.star", "register_sheriffed_builder")

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
    LINUX_DEFAULT = os_enum(os_category.LINUX, "Ubuntu-18.04", json.decode(io.read_file("./linux-default.json"))),
    MAC_10_15 = os_enum(os_category.MAC, "Mac-10.15"),
    MAC_12 = os_enum(os_category.MAC, "Mac-12"),
    MAC_13 = os_enum(os_category.MAC, "Mac-13"),
    MAC_DEFAULT = os_enum(os_category.MAC, "Mac-12"),
    MAC_ANY = os_enum(os_category.MAC, "Mac"),
    WINDOWS_10 = os_enum(os_category.WINDOWS, "Windows-10"),
    WINDOWS_11 = os_enum(os_category.WINDOWS, "Windows-11"),
    WINDOWS_DEFAULT = os_enum(os_category.WINDOWS, "Windows-10"),
    WINDOWS_ANY = os_enum(os_category.WINDOWS, "Windows"),
)

# The constants to be used for the goma_backend and goma_jobs parameters of the
# builder function
goma = struct(
    backend = struct(
        RBE_PROD = {
            "server_host": "goma.chromium.org",
            "rpc_extra_params": "?prod",
        },
        RBE_STAGING = {
            "server_host": "staging-goma.chromium.org",
            "rpc_extra_params": "?staging",
        },
        RBE_TOT = {
            "server_host": "staging-goma.chromium.org",
            "rpc_extra_params": "?tot",
        },
    ),
    jobs = struct(
        J50 = 50,

        # This is for 4 cores mac. -j40 is too small, especially for clobber
        # builder.
        J80 = 80,

        # This is for tryservers becoming slow and critical path of patch
        # landing.
        J150 = 150,

        # This is for tryservers becoming very slow and critical path of patch
        # landing.
        J300 = 300,

        # CI builders (of which are few) may use high number of concurrent Goma
        # jobs.
        # IMPORTANT: when
        #  * bumping number of jobs below, or
        #  * adding this mixin to many builders at once, or
        #  * adding this mixin to a builder with many concurrent builds
        # get review from Goma team.
        MANY_JOBS_FOR_CI = 500,

        # For load testing of the execution backend
        LOAD_TESTING_J1000 = 1000,
        LOAD_TESTING_J2000 = 2000,
    ),
)

reclient = struct(
    instance = struct(
        DEFAULT_TRUSTED = "rbe-chromium-trusted",
        TEST_TRUSTED = "rbe-chromium-trusted-test",
        DEFAULT_UNTRUSTED = "rbe-chromium-untrusted",
        TEST_UNTRUSTED = "rbe-chromium-untrusted-test",
    ),
    jobs = struct(
        DEFAULT = 250,
        LOW_JOBS_FOR_CI = 80,
        HIGH_JOBS_FOR_CI = 500,
        LOW_JOBS_FOR_CQ = 150,
        HIGH_JOBS_FOR_CQ = 300,
    ),
)

def _rotation(name):
    return branches.value(
        branch_selector = branches.selector.MAIN,
        value = [name],
    )

# Sheriff rotations that a builder can be added to (only takes effect on trunk)
# Arbitrary elements can't be added, new rotations must be added in SoM code
sheriff_rotations = struct(
    ANDROID = _rotation("android"),
    ANGLE = _rotation("angle"),
    CHROMIUM = _rotation("chromium"),
    CFT = _rotation("cft"),
    FUCHSIA = _rotation("fuchsia"),
    CHROMIUM_CLANG = _rotation("chromium.clang"),
    CHROMIUM_FUZZ = _rotation("chromium.fuzz"),
    CHROMIUM_GPU = _rotation("chromium.gpu"),
    IOS = _rotation("ios"),
)

def xcode_enum(version):
    return struct(
        version = version,
        cache_name = "xcode_ios_{}".format(version),
        cache_path = "xcode_ios_{}.app".format(version),
    )

# Keep this in-sync with the versions of bots in //ios/build/bots/.
xcode = struct(
    # (current default for other projects) xc12.0 gm seed
    x12a7209 = xcode_enum("12a7209"),
    # xc12.4 gm seed
    x12d4e = xcode_enum("12d4e"),
    # Xcode 12.5. Requires Mac11+ OS.
    x12e262 = xcode_enum("12e262"),
    # Default Xcode 13 for chromium iOS.
    x13main = xcode_enum("13c100"),
    # A newer Xcode 13 version used on beta bots.
    x13betabots = xcode_enum("13f17a"),
    # Xcode14 RC will be used to build Main iOS
    x14main = xcode_enum("14b47b"),
    # A newer Xcode 14 RC  used on beta bots.
    x14betabots = xcode_enum("14b47b"),
    # in use by ios-webkit-tot
    x13wk = xcode_enum("13a1030dwk"),
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

_DEFAULT_BUILDERLESS_OS_CATEGORIES = [os_category.LINUX]

# Macs all have SSDs, so it doesn't make sense to use the default behavior of
# setting ssd:0 dimension
_EXCLUDE_BUILDERLESS_SSD_OS_CATEGORIES = [os_category.MAC]

def _goma_property(*, goma_backend, goma_debug, goma_enable_ats, goma_jobs):
    goma_properties = {}

    goma_backend = defaults.get_value("goma_backend", goma_backend)
    if goma_backend == None:
        return None
    goma_properties.update(goma_backend)

    goma_debug = defaults.get_value("goma_debug", goma_debug)
    if goma_debug:
        goma_properties["debug"] = True

    if goma_enable_ats != None:
        goma_properties["enable_ats"] = goma_enable_ats

    goma_jobs = defaults.get_value("goma_jobs", goma_jobs)
    if goma_jobs != None:
        goma_properties["jobs"] = goma_jobs

    return goma_properties

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

_VALID_REPROXY_ENV_PREFIX_LIST = ["RBE_", "GLOG_", "GOMA_"]

def _reclient_property(*, instance, service, jobs, rewrapper_env, profiler_service, publish_trace, cache_silo, ensure_verified, bootstrap_env, scandeps_server):
    reclient = {}
    instance = defaults.get_value("reclient_instance", instance)
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
    scandeps_server = defaults.get_value("reclient_scandeps_server", scandeps_server)
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
    builder_group = None,
    builderless = args.COMPUTE,
    free_space = None,
    cores = None,
    cpu = None,
    fully_qualified_builder_dimension = False,
    goma_backend = None,
    goma_debug = False,
    goma_enable_ats = args.COMPUTE,
    goma_jobs = None,
    console_view = args.COMPUTE,
    list_view = args.COMPUTE,
    os = None,
    pool = None,
    sheriff_rotations = None,
    xcode = None,
    ssd = args.COMPUTE,
    coverage_gs_bucket = None,
    use_clang_coverage = False,
    use_java_coverage = False,
    use_javascript_coverage = False,
    coverage_exclude_sources = None,
    coverage_test_types = None,
    export_coverage_to_zoss = False,
    generate_blame_list = False,
    resultdb_enable = True,
    resultdb_bigquery_exports = [],
    resultdb_index_by_timestamp = False,
    reclient_instance = None,
    reclient_service = None,
    reclient_jobs = None,
    reclient_rewrapper_env = None,
    reclient_bootstrap_env = None,
    reclient_profiler_service = None,
    reclient_publish_trace = None,
    reclient_scandeps_server = False,
    reclient_cache_silo = None,
    reclient_ensure_verified = None,

    # This is to enable luci.buildbucket.omit_python2 experiment.
    # TODO(crbug.com/1362440): remove this after enabling this in all builders.
    omit_python2 = True,

    # Provide vars for bucket and executable so users don't have to
    # unnecessarily make wrapper functions
    bucket = args.COMPUTE,
    executable = args.COMPUTE,
    notifies = None,
    triggered_by = args.COMPUTE,
)

def builder(
        *,
        name,
        branch_selector = branches.selector.MAIN,
        bucket = args.DEFAULT,
        executable = args.DEFAULT,
        notifies = None,
        triggered_by = args.DEFAULT,
        os = args.DEFAULT,
        builderless = args.DEFAULT,
        free_space = args.DEFAULT,
        builder_cache_name = None,
        override_builder_dimension = None,
        auto_builder_dimension = args.DEFAULT,
        fully_qualified_builder_dimension = args.DEFAULT,
        cores = args.DEFAULT,
        cpu = args.DEFAULT,
        bootstrap = True,
        builder_group = args.DEFAULT,
        builder_spec = None,
        mirrors = None,
        try_settings = None,
        pool = args.DEFAULT,
        ssd = args.DEFAULT,
        sheriff_rotations = None,
        xcode = args.DEFAULT,
        console_view_entry = None,
        list_view = args.DEFAULT,
        goma_backend = args.DEFAULT,
        goma_debug = args.DEFAULT,
        goma_enable_ats = args.DEFAULT,
        goma_jobs = args.DEFAULT,
        coverage_gs_bucket = args.DEFAULT,
        use_clang_coverage = args.DEFAULT,
        use_java_coverage = args.DEFAULT,
        use_javascript_coverage = args.DEFAULT,
        coverage_exclude_sources = args.DEFAULT,
        coverage_test_types = args.DEFAULT,
        export_coverage_to_zoss = args.DEFAULT,
        generate_blame_list = args.DEFAULT,
        resultdb_enable = args.DEFAULT,
        resultdb_bigquery_exports = args.DEFAULT,
        resultdb_index_by_timestamp = args.DEFAULT,
        reclient_instance = args.DEFAULT,
        reclient_service = args.DEFAULT,
        reclient_jobs = args.DEFAULT,
        reclient_rewrapper_env = args.DEFAULT,
        reclient_bootstrap_env = args.DEFAULT,
        reclient_profiler_service = args.DEFAULT,
        reclient_publish_trace = args.DEFAULT,
        reclient_scandeps_server = args.DEFAULT,
        reclient_cache_silo = None,
        reclient_ensure_verified = None,
        omit_python2 = args.DEFAULT,
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
        bootstrap: a boolean indicating whether the builder should have its
            properties bootstrapped. If True, the builder's properties will be
            written to a separate file and its definition will be updated with
            new properties and executable that cause a bootstrapping binary to
            be used. The build's default values for properties will be taken
            from the properties file at the version that the build will check
            out.
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
        builder_group: a string with the group of the builder. Emits a property
            of the form 'builder_group:<builder_group>'. By default, considered
            None.
        builder_spec: The spec describing the configuration for the builder.
            Cannot be set if `mirrors` is set.
        mirrors: References to the builders that the builder should mirror.
            Cannot be set if `builder_spec` is set.
        try_settings: Try-builder-specific settings, can only be set if
            `mirrors` is set.
        cores: an int indicating the number of cores the builder requires for
            the machines that run it. Emits a dimension of the form
            'cores:<cores>' will be emitted. By default, considered None.
        cpu: a member of the `cpu` enum indicating the cpu the builder requires
            for the machines that run it. Emits a dimension of the form
            'cpu:<cpu>'. By default, considered None.
        pool: a string indicating the pool of the machines that run the builder.
            Emits a dimension of the form 'pool:<pool>'. By default, considered
            None. When running a builder that has no explicit pool dimension,
            buildbucket inserts one of the form 'pool:luci.<project>.<bucket>'.
        ssd: a boolean indicating whether the builder runs on machines with ssd.
            If True, emits a 'ssd:1' dimension. If False, emits a 'ssd:0'
            parameter. By default, considered False if builderless is considered
            True and otherwise None.
        sheriff_rotations: A string or list of strings identifying the sheriff
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
        goma_backend: a member of the `goma.backend` enum indicating the goma
            backend the builder should use. Will be incorporated into the
            '$build/goma' property. By default, considered None.
        goma_debug: a boolean indicating whether goma should be debugged. If
            True, the 'debug' field will be set in the '$build/goma' property.
            By default, considered False.
        goma_enable_ats: a boolean indicating whether ats should be enabled for
            goma or args.COMPUTE if ats should be enabled where it is needed.
            If True or False are explicitly set, the 'enable_ats' field will be
            set in the '$build/goma' property.  By default, args.COMPUTE is set
            and 'enable_ats' fields is set only if ats need to be enabled by
            default. The 'enable_ats' on Windows will control cross compiling in
            server side. cross compile if `enable_ats` is False.
            Note: if goma_enable_ats is not set, goma recipe modules sets
            GOMA_ARBITRARY_TOOLCHAIN_SUPPORT=true on windows by default.
        goma_jobs: a member of the `goma.jobs` enum indicating the number of
            jobs to be used by the builder. Sets the 'jobs' field of the
            '$build/goma' property will be set according to the enum member. By
            default, the 'jobs' considered None.
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
        resultdb_bigquery_exports: a list of resultdb.export_test_results(...)
            specifying parameters for exporting test results to BigQuery. By
            default, do not export.
        resultdb_index_by_timestamp: a boolean specifying whether ResultDB
            should index the results of the tests run on this builder by
            timestamp, i.e. for purposes of retrieving a test's history. If
            false, the results will not be searchable by timestamp on ResultDB's
            test history api.
        reclient_instance: a string indicating the GCP project hosting the RBE
            instance for re-client to use.
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
        omit_python2: If True, set luci.buildbucket.omit_python2 experiment.
            TODO(crbug.com/1362440): remove this after enabling this in all
            builders.
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
    if try_settings and not (builder_spec or mirrors):
        fail("try_settings can only be set if builder_spec or mirrors is set")

    dimensions = {}

    properties = kwargs.pop("properties", {})
    if "sheriff_rotations" in properties:
        fail('Setting "sheriff_rotations" property is not supported: ' +
             "use sheriff_rotations instead")
    if "$build/goma" in properties:
        fail('Setting "$build/goma" property is not supported: ' +
             "use goma_backend, goma_dbug, goma_enable_ats and goma_jobs instead")
    if "$build/code_coverage" in properties:
        fail('Setting "$build/code_coverage" property is not supported: ' +
             "use coverage_gs_bucket, use_clang_coverage, use_java_coverage, " +
             "use_javascript_coverage, coverage_exclude_sources, " +
             "coverage_test_types instead")
    if "$build/reclient" in properties:
        fail('Setting "$build/reclient" property is not supported: ' +
             "use reclient_instance and reclient_rewrapper_env instead")
    properties = dict(properties)

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

    if builder_cache_name:
        kwargs.setdefault("caches", []).append(swarming.cache(
            name = builder_cache_name,
            path = "builder",
            wait_for_warm_cache = 4 * time.minute,
        ))

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

    sheriff_rotations = defaults.get_value("sheriff_rotations", sheriff_rotations, merge = args.MERGE_LIST)
    if sheriff_rotations:
        properties["sheriff_rotations"] = sheriff_rotations

    ssd = defaults.get_value("ssd", ssd)
    if ssd == args.COMPUTE:
        ssd = None
        if (builderless and os != None and
            os.category not in _EXCLUDE_BUILDERLESS_SSD_OS_CATEGORIES):
            ssd = False
    if ssd != None:
        dimensions["ssd"] = str(int(ssd))

    goma_enable_ats = defaults.get_value("goma_enable_ats", goma_enable_ats)

    # Enable ATS on linux by default.
    if goma_enable_ats == args.COMPUTE:
        if os and os.category == os_category.LINUX:
            goma_enable_ats = True
        else:
            goma_enable_ats = None
    gp = _goma_property(
        goma_backend = goma_backend,
        goma_debug = goma_debug,
        goma_enable_ats = goma_enable_ats,
        goma_jobs = goma_jobs,
    )
    if gp != None:
        properties["$build/goma"] = gp

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

    reclient = _reclient_property(
        instance = reclient_instance,
        service = reclient_service,
        jobs = reclient_jobs,
        rewrapper_env = reclient_rewrapper_env,
        bootstrap_env = reclient_bootstrap_env,
        profiler_service = reclient_profiler_service,
        publish_trace = reclient_publish_trace,
        scandeps_server = reclient_scandeps_server,
        cache_silo = reclient_cache_silo,
        ensure_verified = reclient_ensure_verified,
    )
    if reclient != None:
        properties["$build/reclient"] = reclient

    kwargs = dict(kwargs)
    if bucket != args.COMPUTE:
        kwargs["bucket"] = bucket
    executable = defaults.get_value("executable", executable)
    if executable != args.COMPUTE:
        kwargs["executable"] = executable
    xcode = defaults.get_value("xcode", xcode)
    if xcode:
        kwargs["caches"] = (kwargs.get("caches") or []) + [swarming.cache(
            name = xcode.cache_name,
            path = xcode.cache_path,
        )]
        properties.setdefault("xcode_build_version", xcode.version)

    kwargs["notifies"] = defaults.get_value("notifies", notifies, merge = args.MERGE_LIST)

    triggered_by = defaults.get_value("triggered_by", triggered_by)
    if triggered_by != args.COMPUTE:
        kwargs["triggered_by"] = triggered_by

    experiments = kwargs.pop("experiments", None) or {}

    # TODO: remove this after this experiment is removed from
    # cr-buildbucket/settings.cfg (http://shortn/_cz2s9ql61X).
    if defaults.get_value("omit_python2", omit_python2):
        experiments["luci.buildbucket.omit_python2"] = 100
    elif "luci.buildbucket.omit_python2" not in experiments:
        experiments["luci.buildbucket.omit_python2"] = 0

    builder = branches.builder(
        name = name,
        branch_selector = branch_selector,
        dimensions = dimensions,
        properties = properties,
        resultdb_settings = _resultdb_settings(
            resultdb_enable = resultdb_enable,
            resultdb_bigquery_exports = resultdb_bigquery_exports,
            resultdb_index_by_timestamp = resultdb_index_by_timestamp,
        ),
        experiments = experiments,
        **kwargs
    )

    # builder will be None if the builder isn't being defined due to the project
    # settings and the branch selector
    if builder == None:
        return None

    register_sheriffed_builder(bucket, name, sheriff_rotations)

    register_recipe_experiments_ref(bucket, name, executable)

    register_builder_config(bucket, name, builder_group, builder_spec, mirrors, try_settings)

    register_bootstrap(bucket, name, bootstrap, executable)

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
    goma = goma,
    os = os,
    sheriff_rotations = sheriff_rotations,
    xcode = xcode,
    free_space = free_space,
)
