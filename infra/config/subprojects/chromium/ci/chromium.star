# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium builder group."""

load("//lib/args.star", "args")
load("//lib/builder_config.star", "builder_config")
load("//lib/builder_health_indicators.star", "health_spec")
load("//lib/builders.star", "cpu", "gardener_rotations", "os", "siso")
load("//lib/branches.star", "branches")
load("//lib/ci.star", "ci")
load("//lib/consoles.star", "consoles")
load("//lib/gn_args.star", "gn_args")
load("//lib/targets.star", "targets")

# Take care when changing the GN args of any of these builders to ensure that
# you do not include a configuration with 'chrome_with_codecs' since these
# builders generate publicly advertised non-Official builds which are not
# allowed to have proprietary codecs enabled.

ci.defaults.set(
    executable = ci.DEFAULT_EXECUTABLE,
    builder_group = "chromium",
    pool = ci.DEFAULT_POOL,
    os = os.LINUX_DEFAULT,
    gardener_rotations = gardener_rotations.CHROMIUM,
    tree_closing = True,
    main_console_view = "main",
    execution_timeout = ci.DEFAULT_EXECUTION_TIMEOUT,
    health_spec = health_spec.DEFAULT,
    service_account = ci.DEFAULT_SERVICE_ACCOUNT,
    shadow_service_account = ci.DEFAULT_SHADOW_SERVICE_ACCOUNT,
    siso_enabled = True,
    siso_project = siso.project.DEFAULT_TRUSTED,
    siso_remote_jobs = siso.remote_jobs.DEFAULT,
)

consoles.console_view(
    name = "chromium",
    branch_selector = [
        branches.selector.ANDROID_BRANCHES,
        branches.selector.DESKTOP_BRANCHES,
        branches.selector.FUCHSIA_BRANCHES,
    ],
    ordering = {
        "*type*": consoles.ordering(short_names = ["dbg", "rel", "off"]),
        "android": "*type*",
        "fuchsia": "*type*",
        "linux": "*type*",
        "mac": "*type*",
        "win": "*type*",
    },
    include_experimental_builds = True,
)

ci.builder(
    name = "android-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "arm",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "rel",
    ),
    contact_team_email = "clank-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "android-archive-rel.json",
            ],
        },
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-arm64-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "android|arm",
        short_name = "arm64",
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "android-arm64-archive-rel.json",
            ],
        },
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-desktop-x64-archive-rel",
    description_html = "Archive builder for Android desktop x64.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.INTEL,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "x64_builder",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_desktop",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    cores = 32,
    # TODO(b/350585060): Enable gardening and tree closing when stable.
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "android|desktop",
        short_name = "x64",
    ),
    contact_team_email = "clank-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "android-desktop-x64-archive-rel.json",
            ],
        },
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-desktop-arm64-archive-rel",
    description_html = "Archive builder for Android desktop arm64.",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "android_builder_without_codecs",
            "android_desktop",
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "strip_debug_info",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    cores = 32,
    # TODO(b/350585060): Enable gardening and tree closing when stable.
    gardener_rotations = args.ignore_default(None),
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "android|desktop",
        short_name = "arm64",
    ),
    contact_team_email = "clank-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "android-desktop-arm64-archive-rel.json",
            ],
        },
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "android-official",
    branch_selector = branches.selector.ANDROID_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "android",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "android",
            apply_configs = [
                "mb",
            ],
            target_arch = builder_config.target_arch.ARM,
            target_platform = builder_config.target_platform.ANDROID,
        ),
        android_config = builder_config.android_config(
            config = "main_builder",
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "remoteexec",
            "android_builder_without_codecs",
            "full_symbols",
            "arm",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    builderless = False,
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "android",
        short_name = "off",
    ),
    contact_team_email = "clank-engprod@google.com",
    # See https://crbug.com/1153349#c22, as we update symbol_level=2, build
    # needs longer time to complete.
    execution_timeout = 7 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-chromeos-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "chromeos",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.CHROMEOS,
        ),
        build_gs_bucket = "chromium-chromiumos-archive",
    ),
    gn_args = gn_args.config(
        configs = [
            "chromeos",
            "release_builder",
            "remoteexec",
            "use_cups",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = [
            "base_unittests",
            "browser_tests",
            "chromeos_unittests",
            "components_unittests",
            "compositor_unittests",
            "content_browsertests",
            "content_unittests",
            "crypto_unittests",
            "dbus_unittests",
            "device_unittests",
            "gcm_unit_tests",
            "google_apis_unittests",
            "gpu_unittests",
            "interactive_ui_tests",
            "ipc_tests",
            "media_unittests",
            "message_center_unittests",
            "nacl_loader_unittests",
            "net_unittests",
            "ppapi_unittests",
            "printing_unittests",
            "remoting_unittests",
            "sandbox_linux_unittests",
            "sql_unittests",
            "ui_base_unittests",
            "unit_tests",
            "url_unittests",
            "views_unittests",
        ],
    ),
    cores = 8,
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "cros",
        short_name = "lnx",
    ),
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "linux-chromiumos-full.json",
            ],
        },
    },
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "linux-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "updater",
            "linux",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    cores = 32,
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "rel",
    ),
    contact_team_email = "chrome-browser-infra-team@google.com",
    notifies = ["linux-archive-rel"],
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "linux-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "linux-official",
    branch_selector = branches.selector.LINUX_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 64,
            target_platform = builder_config.target_platform.LINUX,
        ),
    ),
    gn_args = gn_args.config(
        configs = ["official_optimize", "remoteexec", "linux", "x64"],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    builderless = False,
    cores = 32,
    gardener_rotations = args.ignore_default(None),
    console_view_entry = consoles.console_view_entry(
        category = "linux",
        short_name = "off",
    ),
    execution_timeout = 7 * time.hour,
    health_spec = health_spec.modified_default({
        "Unhealthy": health_spec.unhealthy_thresholds(
            build_time = struct(
                p50_mins = 240,
            ),
        ),
    }),
)

ci.builder(
    name = "mac-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "mac_strip",
            "minimal_symbols",
            "mac",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "rel",
    ),
    contact_team_email = "bling-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "mac-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "mac-arm64-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "mac_strip",
            "minimal_symbols",
            "mac",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    builderless = True,
    cores = None,
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac|arm",
        short_name = "rel",
    ),
    contact_team_email = "bling-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "mac-arm64-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "mac-official",
    branch_selector = branches.selector.MAC_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 64,
            target_platform = builder_config.target_platform.MAC,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "remoteexec",
            "mac",
            "arm64",
            "save_lld_reproducers",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    builderless = False,
    os = os.MAC_ANY,
    cpu = cpu.ARM64,
    console_view_entry = consoles.console_view_entry(
        category = "mac",
        short_name = "off",
    ),
    contact_team_email = "bling-engprod@google.com",
    # TODO(crbug.com/40208487) builds with PGO change take long time.
    # Keep in sync with mac-official in try/chromium.star.
    execution_timeout = 15 * time.hour,
)

ci.builder(
    name = "win-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        targets = "public_build_scripts",
        additional_compile_targets = "all",
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "win-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "win-arm64-archive-rel",
    description_html = "Chromium snapshot archive builder for win-arm64",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_arch = builder_config.target_arch.ARM,
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "minimal_symbols",
            "win",
            "arm64",
        ],
    ),
    targets = targets.bundle(
        targets = "public_build_scripts",
        additional_compile_targets = "all",
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    # TODO(crbug.com/335863313): Enable when verified.
    gardener_rotations = args.ignore_default(None),
    # TODO(crbug.com/335863313): Enable when verified.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "arm64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "win-arm64-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "win-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 64,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "remoteexec",
            "minimal_symbols",
            "win",
            "x64",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    # TODO(crbug.com/346263463): Enable tree-closing when the builder no
    # longer flakily fails compile.
    tree_closing = False,
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "64",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    # TODO(crbug.com/40735404) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)

ci.builder(
    name = "win32-archive-rel",
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "clobber",
                "mb",
            ],
            build_config = builder_config.build_config.RELEASE,
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "release_builder",
            "remoteexec",
            "x86",
            "minimal_symbols",
            "win",
        ],
    ),
    targets = targets.bundle(
        targets = "public_build_scripts",
        additional_compile_targets = "all",
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win|rel",
        short_name = "32",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    properties = {
        # The format of these properties is defined at archive/properties.proto
        "$build/archive": {
            "source_side_spec_path": [
                "src",
                "infra",
                "archive_config",
                "win32-archive-rel.json",
            ],
        },
    },
)

ci.builder(
    name = "win32-official",
    branch_selector = branches.selector.WINDOWS_BRANCHES,
    builder_spec = builder_config.builder_spec(
        gclient_config = builder_config.gclient_config(
            config = "chromium",
            apply_configs = [
                "checkout_pgo_profiles",
            ],
        ),
        chromium_config = builder_config.chromium_config(
            config = "chromium",
            apply_configs = [
                "mb",
            ],
            target_bits = 32,
            target_platform = builder_config.target_platform.WIN,
        ),
    ),
    gn_args = gn_args.config(
        configs = [
            "official_optimize",
            "remoteexec",
            "win",
            "x86",
        ],
    ),
    targets = targets.bundle(
        additional_compile_targets = "all",
    ),
    builderless = False,
    cores = 32,
    os = os.WINDOWS_DEFAULT,
    console_view_entry = consoles.console_view_entry(
        category = "win|off",
        short_name = "32",
    ),
    contact_team_email = "chrome-desktop-engprod@google.com",
    # TODO(crbug.com/40735404) builds with PGO change take long time.
    execution_timeout = 7 * time.hour,
    siso_remote_jobs = siso.remote_jobs.HIGH_JOBS_FOR_CI,
)
