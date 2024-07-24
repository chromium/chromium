# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for defining builder health indicator thresholds.

See //docs/infra/builder_health_indicators.md for more info.
"""

load("@stdlib//internal/graph.star", "graph")
load("@stdlib//internal/luci/common.star", "keys")
load("./nodes.star", "nodes")
load("./structs.star", "structs")

_HEALTH_SPEC = nodes.create_bucket_scoped_node_type("health_spec")

# See https://source.chromium.org/chromium/infra/infra/+/main:go/src/infra/cr_builder_health/src_config.go
# for all configurable thresholds.
_default_specs = {
    "Unhealthy": struct(
        score = 5,
        period_days = 7,
        # If any of these thresholds are exceeded, the builder will be deemed
        # unhealthy.
        # Setting a value of None will ignore that threshold
        infra_fail_rate = struct(
            average = 0.05,
        ),
        fail_rate = struct(
            average = 0.2,
        ),
        build_time = struct(
            p50_mins = None,
        ),
        pending_time = struct(
            p50_mins = 20,
        ),
    ),
    "Low Value": struct(
        score = 1,
        period_days = 90,
        # If any of these thresholds are met, the builder will be deemed
        # low-value and will be considered for deletion.
        # Setting a value of None will ignore that threshold
        fail_rate = struct(
            average = 0.99,
        ),
    ),
}

_blank_unhealthy_thresholds = struct(
    infra_fail_rate = struct(
        average = None,
    ),
    fail_rate = struct(
        average = None,
    ),
    build_time = struct(
        p50_mins = None,
    ),
    pending_time = struct(
        p50_mins = None,
    ),
)

blank_low_value_thresholds = struct(
    fail_rate = struct(
        average = None,
    ),
)

DEFAULT = {
    "Unhealthy": struct(
        score = 5,
        period_days = 7,
        _default = "_default",
    ),
    "Low Value": struct(
        score = 1,
        period_days = 90,
        _default = "_default",
    ),
}

# Users define the specs as {problem_name -> problem_spec} for aesthetic reasons
# So all user-exposed functions expect a dictionary.
# We then convert that into a list of [problem_specs] so the object encapsulates
# its own name, for ease of processing
def unhealthy_thresholds(
        fail_rate = struct(),
        infra_fail_rate = struct(),
        build_time = struct(),
        pending_time = struct()):
    thresholds = {"fail_rate": fail_rate, "infra_fail_rate": infra_fail_rate, "build_time": build_time, "pending_time": pending_time}
    fail_if_any_none_val(thresholds)

    return structs.evolve(_blank_unhealthy_thresholds, **thresholds)

def low_value_thresholds(
        fail_rate = struct()):
    thresholds = {"fail_rate": fail_rate}
    fail_if_any_none_val(thresholds)

    return structs.evolve(blank_low_value_thresholds, **thresholds)

def fail_if_any_none_val(vals):
    for k, v in vals.items():
        if v == None:
            fail(k + " threshold was None. Thresholds can't be None. Use an empty struct() instead")

def modified_default(modifications):
    return _merge_mods(_default_specs, modifications)

def _merge_mods(base, modifications):
    spec = dict(base)

    for mod_name, mod in modifications.items():
        mods_proto = structs.to_proto_properties(mod)
        if len(mods_proto) == 0:
            fail("Modifications for health spec \"{}\" were empty.".format(mod_name))

        if mod_name not in spec:
            spec[mod_name] = mod
        else:
            spec[mod_name] = structs.evolve(spec[mod_name], **mods_proto)

    return spec

def _exempted_from_contact(bucket, builder):
    return builder in _exempted_from_contact_builders.get(bucket, [])

def register_health_spec(bucket, name, specs, contact_team_email):
    if not contact_team_email and not _exempted_from_contact(bucket, name):
        fail("Builder " + name + " must have a contact_team_email. All new builders must specify a team email for contact in case the builder stops being healthy or providing value.")
    elif contact_team_email and _exempted_from_contact(bucket, name):
        fail("Need to remove builder " + bucket + "/" + name + " from _exempted_from_contact_builders")

    if specs:
        spec = struct(
            problem_specs = _convert_specs(specs),
            contact_team_email = contact_team_email,
        )
        health_spec_key = _HEALTH_SPEC.add(
            bucket,
            name,
            props = structs.to_proto_properties(spec),
            idempotent = True,
        )

        graph.add_edge(keys.project(), health_spec_key)

def _convert_specs(specs):
    """Users define the specs as {problem_name -> problem_spec} for aesthetic reasons,

    So all user-exposed functions expect a dictionary.
    We then convert that into a list of [problem_specs] so the object encapsulates its own name, for ease of processing
    """
    converted_specs = []
    for name, spec in specs.items():
        thresholds_spec = structs.to_proto_properties(spec)
        thresholds_spec.pop("score")
        thresholds_spec.pop("period_days")
        converted_specs.append(struct(
            name = name,
            score = spec.score,
            period_days = spec.period_days,
            thresholds = thresholds_spec,
        ))

    return converted_specs

def _generate_health_specs(ctx):
    specs = {}

    for node in graph.children(keys.project(), _HEALTH_SPEC.kind):
        bucket = node.key.container.id
        builder = node.key.id
        specs.setdefault(bucket, {})[builder] = node.props

    result = {
        "_default_specs": _convert_specs(_default_specs),
        "specs": specs,
    }

    ctx.output["health-specs/health-specs.json"] = json.indent(json.encode(result), indent = "  ")

# This dict should NOT be added to. It contains a list of builders that are
# exempted from needing a contact_team_email field.
# It's intended as a stopgap for older builders. All new builders should have a
# contact_team_email field for the good of our code and CI system.
# Builders should be removed from here once their contact is assigned.
_exempted_from_contact_builders = {
    "ci": [
        "3pp-linux-amd64-packager",
        "3pp-mac-amd64-packager",
        "ASAN Release Media",
        "Blink Unexpected Pass Finder",
        "Comparison Android (reclient)",
        "Comparison Android (reclient) (reproxy cache)",
        "Comparison Android (reclient)(CQ)",
        "Comparison Mac (reclient)",
        "Comparison Mac (reclient)(CQ)",
        "Comparison Mac arm64 (reclient)",
        "Comparison Mac arm64 on arm64 (reclient)",
        "Comparison Simple Chrome (reclient)",
        "Comparison Simple Chrome (reclient)(CQ)",
        "Comparison Windows (8 cores) (reclient)",
        "Comparison Windows (reclient)",
        "Comparison Windows (reclient)(CQ)",
        "Comparison ios (reclient)",
        "Comparison ios (reclient)(CQ)",
        "Leak Detection Linux",
        "Linux Builder (reclient compare)",
        "Linux Viz",
        "Mac ASAN Release Media",
        "Mac Builder (reclient compare)",
        "Mac Builder Next",
        "Mac deterministic",
        "Mac deterministic (dbg)",
        "Mac13 Tests",
        "Mac13 Tests (dbg)",
        "Oreo Phone Tester",
        "Site Isolation Android",
        "Win 10 Fast Ring",
        "Win x64 Builder (reclient compare)",
        "Win x64 Builder (reclient)",
        "android-11-x86-fyi-rel",
        "android-11-x86-rel",
        "android-12-x64-dbg-tests",
        "android-12-x64-fyi-rel",
        "android-12l-x64-fyi-dbg",
        "android-13-x64-fyi-rel",
        "android-annotator-rel",
        "android-arm64-archive-rel",
        "android-avd-packager",
        "android-build-perf-developer",
        "android-chrome-pie-x86-wpt-fyi-rel",
        "android-code-coverage",
        "android-code-coverage-native",
        "android-device-flasher",
        "android-fieldtrial-rel",
        "android-perfetto-rel",
        "android-pie-arm64-rel-dev",
        "android-pie-x86-fyi-rel",
        "android-rust-arm32-rel",
        "android-rust-arm64-dbg",
        "android-rust-arm64-rel",
        "android-sdk-packager",
        "android-webview-12-x64-dbg-tests",
        "android-webview-13-x64-dbg-tests",
        "android-webview-pie-x86-wpt-fyi-rel",
        "android-x86-code-coverage",
        "chromeos-js-code-coverage",
        "fuchsia-code-coverage",
        "fuchsia-x64-accessibility-rel",
        "ios-blink-dbg-fyi",
        "ios-catalyst",
        "ios-device",
        "ios-fieldtrial-rel",
        "ios-simulator",
        "ios-simulator-code-coverage",
        "ios-simulator-full-configs",
        "ios-simulator-multi-window",
        "ios-simulator-noncq",
        "ios-webkit-tot",
        "ios-wpt-fyi-rel",
        "ios17-beta-simulator",
        "ios17-sdk-device",
        "ios17-sdk-simulator",
        "ios18-beta-simulator",
        "ios18-sdk-simulator",
        "lacros-amd64-generic-binary-size-rel",
        "lacros-amd64-generic-rel-fyi",
        "lacros-amd64-generic-rel-skylab",
        "lacros-amd64-generic-rel-skylab-fyi",
        "lacros-arm-generic-rel-skylab-fyi",
        "lacros-arm64-archive-rel",
        "lacros-arm64-generic-rel",
        "lacros-arm64-generic-rel-skylab-fyi",
        "linux-annotator-rel",
        "linux-ash-chromium-generator-rel",
        "linux-blink-heap-verification",
        "linux-blink-web-tests-force-accessibility-rel",
        "linux-blink-wpt-reset-rel",
        "linux-build-perf-developer",
        "linux-chromeos-annotator-rel",
        "linux-chromeos-archive-rel",
        "linux-chromeos-build-perf",
        "linux-chromeos-build-perf-siso",
        "linux-chromeos-code-coverage",
        "linux-code-coverage",
        "linux-fieldtrial-rel",
        "linux-fuzz-coverage",
        "linux-headless-shell-rel",
        "linux-js-code-coverage",
        "linux-lacros-archive-rel",
        "linux-lacros-asan-lsan-rel",
        "linux-lacros-builder-fyi-rel",
        "linux-lacros-builder-rel (reclient)",
        "linux-lacros-code-coverage",
        "linux-lacros-dbg-fyi",
        "linux-lacros-dbg-tests-fyi",
        "linux-lacros-tester-fyi-rel",
        "linux-lacros-version-skew-fyi",
        "linux-local-ssd-rel-dev",
        "linux-network-sandbox-rel",
        "linux-official",
        "linux-perfetto-rel",
        "linux-rel-jammy-dev",
        "linux-rel-no-external-ip",
        "linux-remote-ssd-rel-dev",
        "linux-rust-x64-dbg",
        "linux-rust-x64-rel",
        "linux-ubsan-fyi-rel",
        "linux-updater-builder-dbg",
        "linux-updater-builder-rel",
        "linux-updater-tester-dbg",
        "linux-updater-tester-rel",
        "linux-upload-perfetto",
        "linux-v4l2-codec-rel",
        "mac-arm-rel-dev",
        "mac-build-perf",
        "mac-build-perf-developer",
        "mac-build-perf-siso",
        "mac-code-coverage",
        "mac-fieldtrial-tester",
        "mac-intel-on-arm64-rel",
        "mac-lsan-fyi-rel",
        "mac-osxbeta-rel",
        "mac-perfetto-rel",
        "mac-rel-dev",
        "mac-rust-x64-dbg",
        "mac-ubsan-fyi-rel",
        "mac-updater-builder-arm64-dbg",
        "mac-updater-builder-arm64-rel",
        "mac-updater-builder-asan-dbg",
        "mac-updater-builder-dbg",
        "mac-updater-builder-rel",
        "mac-upload-perfetto",
        "mac10.15-updater-tester-dbg",
        "mac10.15-updater-tester-rel",
        "mac11-arm64-updater-tester-dbg",
        "mac11-arm64-updater-tester-rel",
        "mac11-x64-updater-tester-dbg",
        "mac11-x64-updater-tester-rel",
        "mac12-arm64-updater-tester-rel",
        "mac12-x64-updater-tester-asan-dbg",
        "mac13-arm64-updater-tester-dbg",
        "mac13-x64-updater-tester-rel",
        "metadata-exporter",
        "rts-model-packager",
        "rts-suite-analysis",
        "win-annotator-rel",
        "win-build-perf-developer",
        "win-celab-builder-rel",
        "win-fieldtrial-rel",
        "win-local-ssd-rel-dev",
        "win-network-sandbox-tester",
        "win-perfetto-rel",
        "win-rel-dev",
        "win-rust-x64-dbg",
        "win-rust-x64-rel",
        "win-updater-builder-dbg",
        "win-updater-builder-rel",
        "win-upload-perfetto",
        "win10-32-on-64-updater-tester-dbg",
        "win10-32-on-64-updater-tester-rel",
        "win10-code-coverage",
        "win10-rel-no-external-ip",
        "win10-updater-tester-dbg",
        "win10-updater-tester-dbg-uac",
        "win10-updater-tester-rel",
        "win10-updater-tester-rel-uac",
        "win11-rel-dev",
        "win11-updater-tester-dbg-uac",
        "win11-updater-tester-rel",
        "win32-arm64-rel",
        "win32-updater-builder-dbg",
        "win32-updater-builder-rel",
    ],
    "try": [
        "3pp-linux-amd64-packager",
        "3pp-mac-amd64-packager",
        "Crossbench CBB Mac Try",
        "android-10-arm64-rel",
        "android-11-x86-rel",
        "android-12-x64-dbg",
        "android-12-x64-rel-compilator",
        "android-12l-x64-dbg",
        "android-angle-chromium-try",
        "android-arm-compile-dbg",
        "android-arm64-all-targets-dbg",
        "android-arm64-rel",
        "android-arm64-rel-compilator",
        "android-asan-compile-dbg",
        "android-bfcache-rel",
        "android-binary-size",
        "android-chrome-pie-x86-wpt-fyi-rel",
        "android-clang-tidy-rel",
        "android-code-coverage",
        "android-code-coverage-native",
        "android-deterministic-dbg",
        "android-deterministic-rel",
        "android-fieldtrial-rel",
        "android-oreo-arm64-dbg",
        "android-oreo-x86-rel",
        "android-perfetto-rel",
        "android-pie-arm64-dbg",
        "android-pie-x86-rel",
        "android-rust-arm32-rel",
        "android-rust-arm64-dbg",
        "android-rust-arm64-rel",
        "android-webview-10-x86-rel-tests",
        "android-webview-12-x64-dbg",
        "android-webview-13-x64-dbg",
        "android-webview-oreo-arm64-dbg",
        "android-webview-pie-arm64-dbg",
        "android-webview-pie-x86-wpt-fyi-rel",
        "android-x64-cast",
        "android_arm64_dbg_recipe",
        "android_blink_rel",
        "android_compile_dbg",
        "android_compile_x64_dbg",
        "android_compile_x86_dbg",
        "android_optional_gpu_tests_rel",
        "branch-config-verifier",
        "builder-config-verifier",
        "chromeos-amd64-generic-asan-rel",
        "chromeos-amd64-generic-cfi-thin-lto-rel",
        "chromeos-amd64-generic-dbg",
        "chromeos-amd64-generic-lacros-dbg",
        "chromeos-amd64-generic-rel-compilator",
        "chromeos-arm-generic-dbg",
        "chromeos-arm-generic-rel",
        "chromeos-arm64-generic-rel",
        "chromeos-js-code-coverage",
        "chromeos-js-coverage-rel",
        "chromium_presubmit",
        "fuchsia-angle-try",
        "fuchsia-arm64-cast-receiver-rel",
        "fuchsia-binary-size",
        "fuchsia-clang-tidy-rel",
        "fuchsia-code-coverage",
        "fuchsia-compile-x64-dbg",
        "fuchsia-deterministic-dbg",
        "fuchsia-fyi-arm64-dbg",
        "fuchsia-x64-accessibility-rel",
        "fuchsia-x64-cast-receiver-rel",
        "fuchsia-x64-cast-receiver-rel-compilator",
        "gpu-fyi-cq-android-arm64",
        "gpu-fyi-try-chromeos-kevin",
        "gpu-fyi-try-chromeos-skylab-kevin",
        "ios-angle-try-intel",
        "ios-asan",
        "ios-blink-dbg-fyi",
        "ios-catalyst",
        "ios-device",
        "ios-fieldtrial-rel",
        "ios-m1-simulator",
        "ios-simulator",
        "ios-simulator-code-coverage",
        "ios-simulator-compilator",
        "ios-simulator-full-configs",
        "ios-simulator-multi-window",
        "ios-simulator-noncq",
        "ios-wpt-fyi-rel",
        "ios17-beta-simulator",
        "ios17-sdk-simulator",
        "ios18-beta-simulator",
        "ios18-sdk-simulator",
        "lacros-amd64-generic-rel",
        "lacros-amd64-generic-rel-skylab",
        "lacros-amd64-generic-rel-skylab-fyi",
        "lacros-arm-generic-rel",
        "lacros-arm64-generic-rel",
        "layout_test_leak_detection",
        "leak_detection_linux",
        "linux-afl-asan-rel",
        "linux-angle-chromium-try",
        "linux-annotator-rel",
        "linux-arm64-rel-cft",
        "linux-asan-dbg",
        "linux-asan-media-rel",
        "linux-asan-media-v8-arm-rel",
        "linux-asan-rel",
        "linux-asan-v8-arm-dbg",
        "linux-asan-v8-arm-rel",
        "linux-bfcache-rel",
        "linux-blink-heap-verification-try",
        "linux-blink-rel",
        "linux-blink-web-tests-force-accessibility-rel",
        "linux-centipede-asan-rel",
        "linux-cfm-rel",
        "linux-chromeos-annotator-rel",
        "linux-chromeos-asan-rel",
        "linux-chromeos-clang-tidy-rel",
        "linux-chromeos-code-coverage",
        "linux-chromeos-compile-dbg",
        "linux-chromeos-dbg",
        "linux-chromeos-rel",
        "linux-chromeos-rel-compilator",
        "linux-clang-tidy-rel",
        "linux-code-coverage",
        "linux-dcheck-off-rel",
        "linux-extended-tracing-rel",
        "linux-fieldtrial-rel",
        "linux-gcc-rel",
        "linux-headless-shell-rel",
        "linux-js-code-coverage",
        "linux-js-coverage-rel",
        "linux-lacros-asan-lsan-rel",
        "linux-lacros-clang-tidy-rel",
        "linux-lacros-code-coverage",
        "linux-lacros-dbg",
        "linux-lacros-fyi-rel",
        "linux-lacros-rel",
        "linux-lacros-rel-compilator",
        "linux-lacros-version-skew-fyi",
        "linux-layout-tests-edit-ng",
        "linux-libfuzzer-asan-rel",
        "linux-mbi-mode-per-render-process-host-rel",
        "linux-msan-chained-origins-rel",
        "linux-msan-no-origins-rel",
        "linux-official",
        "linux-perfetto-rel",
        "linux-rel",
        "linux-rel-cft",
        "linux-rel-compilator",
        "linux-rust-x64-dbg",
        "linux-rust-x64-rel",
        "linux-swangle-chromium-try-x64",
        "linux-swangle-chromium-try-x64-exp",
        "linux-swangle-try-tot-swiftshader-x64",
        "linux-swangle-try-x64",
        "linux-swangle-try-x64-exp",
        "linux-tsan-dbg",
        "linux-tsan-rel",
        "linux-ubsan-fyi-rel",
        "linux-ubsan-rel",
        "linux-ubsan-vptr",
        "linux-ubsan-vptr-rel",
        "linux-updater-try-builder-dbg",
        "linux-updater-try-builder-rel",
        "linux-v4l2-codec-rel",
        "linux-viz-rel",
        "linux-wayland-rel",
        "linux-wayland-rel-compilator",
        "linux-webkit-asan-rel",
        "linux-webkit-msan-rel",
        "linux_chromium_archive_rel_ng",
        "linux_chromium_asan_rel_ng",
        "linux_chromium_asan_rel_ng-compilator",
        "linux_chromium_cfi_rel_ng",
        "linux_chromium_chromeos_asan_rel_ng",
        "linux_chromium_chromeos_msan_rel_ng",
        "linux_chromium_clobber_deterministic",
        "linux_chromium_clobber_rel_ng",
        "linux_chromium_compile_dbg_ng",
        "linux_chromium_compile_rel_ng",
        "linux_chromium_dbg_ng",
        "linux_chromium_msan_rel_ng",
        "linux_chromium_tsan_rel_ng",
        "linux_chromium_tsan_rel_ng-compilator",
        "linux_optional_gpu_tests_rel",
        "linux_upload_clang",
        "linux_upload_rust",
        "mac-angle-chromium-try",
        "mac-arm64-clobber-rel",
        "mac-arm64-on-arm64-rel",
        "mac-asan-media-rel",
        "mac-asan-rel",
        "mac-builder-next",
        "mac-clang-tidy-rel",
        "mac-clobber-rel",
        "mac-code-coverage",
        "mac-fieldtrial-tester",
        "mac-intel-on-arm64-rel",
        "mac-lsan-fyi-rel",
        "mac-official",
        "mac-osxbeta-rel",
        "mac-perfetto-rel",
        "mac-rel",
        "mac-rel-cft",
        "mac-rel-compilator",
        "mac-rust-x64-dbg",
        "mac-swangle-chromium-try-x64",
        "mac-ubsan-fyi-rel",
        "mac-updater-try-builder-dbg",
        "mac-updater-try-builder-rel",
        "mac11-arm64-rel",
        "mac11.0-blink-rel",
        "mac11.0.arm64-blink-rel",
        "mac12-arm64-rel",
        "mac12-tests",
        "mac12.0-blink-rel",
        "mac12.0.arm64-blink-rel",
        "mac13-arm64-rel",
        "mac13-arm64-rel-compilator",
        "mac13-blink-rel",
        "mac13-tests",
        "mac13.arm64-blink-rel",
        "mac13.arm64-skia-alt-blink-rel",
        "mac_chromium_10.15_rel_ng",
        "mac_chromium_11.0_rel_ng",
        "mac_chromium_asan_rel_ng",
        "mac_chromium_compile_dbg_ng",
        "mac_chromium_compile_rel_ng",
        "mac_chromium_dbg_ng",
        "mac_optional_gpu_tests_rel",
        "mac_upload_clang",
        "mac_upload_clang_arm",
        "mac_upload_rust",
        "mac_upload_rust_arm",
        "network_service_linux",
        "reclient-config-deployment-verifier",
        "requires-testing-checker",
        "targets-config-verifier",
        "tricium-clang-tidy",
        "tricium-metrics-analysis",
        "tricium-oilpan-analysis",
        "tricium-simple",
        "win-angle-chromium-x64-try",
        "win-angle-chromium-x86-try",
        "win-annotator-rel",
        "win-asan",
        "win-asan-media-rel",
        "win-asan-rel",
        "win-celab-try-rel",
        "win-fieldtrial-rel",
        "win-libfuzzer-asan-rel",
        "win-official",
        "win-perfetto-rel",
        "win-presubmit",
        "win-rel",
        "win-rel-cft",
        "win-rel-compilator",
        "win-rust-x64-dbg",
        "win-rust-x64-rel",
        "win-swangle-chromium-try-x86",
        "win-swangle-try-tot-swiftshader-x64",
        "win-swangle-try-tot-swiftshader-x86",
        "win-swangle-try-x64",
        "win-swangle-try-x86",
        "win-updater-try-builder-dbg",
        "win-updater-try-builder-rel",
        "win10-clang-tidy-rel",
        "win10-code-coverage",
        "win10-dbg",
        "win10.20h2-blink-rel",
        "win11-arm64-blink-rel",
        "win11-blink-rel",
        "win32-official",
        "win_chromium_compile_dbg_ng",
        "win_chromium_compile_rel_ng",
        "win_chromium_x64_rel_ng",
        "win_optional_gpu_tests_rel",
        "win_upload_clang",
        "win_upload_rust",
    ],
    "infra": [
        "autosharder",
    ],
    "codesearch": [
        "gen-android-try",
        "gen-chromiumos-try",
        "gen-fuchsia-try",
        "gen-ios-try",
        "gen-lacros-try",
        "gen-linux-try",
        "gen-mac-try",
        "gen-webview-try",
        "gen-win-try",
    ],
    "findit": [
        "gofindit-culprit-verification",
        "test-single-revision",
    ],
    "flaky-reproducer": [
        "runner",
    ],
    "reclient": [
        "Comparison Linux (reclient vs reclient remote links)",
        "Comparison Linux (reclient)",
        "Comparison Linux (reclient)(CQ)",
        "Linux Builder (canonical wd) (reclient compare)",
        "Linux Builder reclient staging",
        "Linux Builder reclient staging untrusted",
        "Linux Builder reclient test",
        "Linux Builder reclient test (casng)",
        "Linux Builder reclient test (casng) untrusted",
        "Linux Builder reclient test (unified uploads)",
        "Linux Builder reclient test (unified uploads) untrusted",
        "Linux Builder reclient test untrusted",
        "Mac Builder reclient staging",
        "Mac Builder reclient staging untrusted",
        "Mac Builder reclient test",
        "Mac Builder reclient test untrusted",
        "Simple Chrome Builder reclient staging",
        "Simple Chrome Builder reclient staging untrusted",
        "Simple Chrome Builder reclient test",
        "Simple Chrome Builder reclient test untrusted",
        "Win x64 Builder reclient staging",
        "Win x64 Builder reclient staging untrusted",
        "Win x64 Builder reclient test",
        "Win x64 Builder reclient test untrusted",
        "Win x64 Cross Builder (reclient compare)",
        "Windows Cross deterministic",
        "ios-simulator reclient staging",
        "ios-simulator reclient staging untrusted",
        "ios-simulator reclient test",
        "ios-simulator reclient test untrusted",
        "mac-arm64-rel reclient staging",
        "mac-arm64-rel reclient staging untrusted",
        "mac-arm64-rel reclient test",
        "mac-arm64-rel reclient test untrusted",
    ],
    "reviver": [
        "android-coverage-launcher",
        "android-device-launcher",
        "android-launcher",
        "android-x64-launcher",
        "coverage-runner",
        "fuchsia-coordinator",
        "lacros-coordinator",
        "linux-launcher",
        "mac-launcher",
        "runner",
        "win-launcher",
    ],
    "webrtc": [
        "WebRTC Chromium Android Builder",
        "WebRTC Chromium Android Tester",
        "WebRTC Chromium Linux Builder",
        "WebRTC Chromium Linux Tester",
        "WebRTC Chromium Mac Builder",
        "WebRTC Chromium Mac Tester",
        "WebRTC Chromium Win Builder",
        "WebRTC Chromium Win10 Tester",
    ],
    "webrtc.fyi": [
        "WebRTC Chromium FYI Android Builder",
        "WebRTC Chromium FYI Android Builder (dbg)",
        "WebRTC Chromium FYI Android Builder ARM64 (dbg)",
        "WebRTC Chromium FYI Android Tests (dbg)",
        "WebRTC Chromium FYI Android Tests ARM64 (dbg)",
        "WebRTC Chromium FYI Linux Builder",
        "WebRTC Chromium FYI Linux Builder (dbg)",
        "WebRTC Chromium FYI Linux Tester",
        "WebRTC Chromium FYI Mac Builder",
        "WebRTC Chromium FYI Mac Builder (dbg)",
        "WebRTC Chromium FYI Mac Tester",
        "WebRTC Chromium FYI Win Builder",
        "WebRTC Chromium FYI Win Builder (dbg)",
        "WebRTC Chromium FYI Win10 Tester",
        "WebRTC Chromium FYI ios-device",
        "WebRTC Chromium FYI ios-simulator",
    ],
}

health_spec = struct(
    DEFAULT = DEFAULT,
    unhealthy_thresholds = unhealthy_thresholds,
    low_value_thresholds = low_value_thresholds,
    modified_default = modified_default,
)

lucicfg.generator(_generate_health_specs)
