# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains suite definitions that can be used in
# //testing/buildbot/waterfalls.pyl and will also be usable for builders that
# set their tests in starlark (once that is ready). The legacy_ prefix on the
# declarations indicates the capability to be used in //testing/buildbot. Once a
# suite is no longer needed in //testing/buildbot, targets.bundle (which does
# not yet exist) can be used for grouping tests in a more flexible manner.

load("//lib/targets.star", "targets")

# TODO(gbeaty) - Make the resultdb information for tests using the same binaries
# consistent and move the information onto the binaries

targets.legacy_basic_suite(
    name = "aura_gtests",
    tests = {
        "aura_unittests": targets.legacy_test_config(),
        "compositor_unittests": targets.legacy_test_config(),
        "wm_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "blink_unittests_suite",
    tests = {
        "blink_unit_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "blink_web_tests_ppapi_isolated_scripts",
    tests = {
        "ppapi_blink_web_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chrome_android_finch_smoke_tests",
    tests = {
        "variations_android_smoke_tests": targets.legacy_test_config(),
        "variations_webview_smoke_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chrome_finch_smoke_tests",
    tests = {
        "variations_desktop_smoke_tests": targets.legacy_test_config(
            chromeos_args = [
                "--target-platform=cros",
            ],
            lacros_args = [
                "--target-platform=lacros",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_isolated_script_tests",
    tests = {
        "chrome_sizes": targets.legacy_test_config(),
        "variations_smoke_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chrome_private_code_test_isolated_scripts",
    tests = {
        "chrome_private_code_test": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chrome_sizes_suite",
    tests = {
        "chrome_sizes": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chromedriver_py_tests_isolated_scripts",
    tests = {
        "chromedriver_py_tests": targets.legacy_test_config(
            args = [
                "--test-type=integration",
            ],
        ),
        "chromedriver_py_tests_headless_shell": targets.legacy_test_config(
            args = [
                "--test-type=integration",
            ],
        ),
        "chromedriver_replay_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_chrome_all_tast_tests",
    tests = {
        "chrome_all_tast_tests": targets.legacy_test_config(
            # `tast_expr` must be a non-empty string to run the tast tests. But the value of
            # would be overridden by `tast_arrt_expr` defined in chromeos/BUILD.gn, so that we
            # put the stub string here.
            tast_expr = "STUB_STRING_TO_RUN_TAST_TESTS",
            # Temporary increases the maximum retries due to the unstable cloudbots (b/377616158)
            test_level_retries = 2,
            # Timeout including DUT privisioning.
            timeout_sec = 14400,
            # Number of shards. Might be overriden for slower boards.
            shards = 15,
        ),
    },
)

# Test suite for running critical Tast tests.
targets.legacy_basic_suite(
    name = "chromeos_chrome_criticalstaging_tast_tests",
    tests = {
        "chrome_criticalstaging_tast_tests": targets.legacy_test_config(
            # `tast_expr` must be a non-empty string to run the tast tests. But the value of
            # would be overridden by `tast_arrt_expr` defined in chromeos/BUILD.gn, so that we
            # put the stub string here.
            tast_expr = "STUB_STRING_TO_RUN_TAST_TESTS",
            test_level_retries = 2,
            ci_only = True,
            timeout_sec = 14400,
            experiment_percentage = 100,
            shards = 3,
        ),
    },
)

# Test suite for running disabled Tast tests to collect data to re-enable
# them. The test suite should not be critical to builders.
targets.legacy_basic_suite(
    name = "chromeos_chrome_disabled_tast_tests",
    tests = {
        "chrome_disabled_tast_tests": targets.legacy_test_config(
            # `tast_expr` must be a non-empty string to run the tast tests. But the value of
            # would be overridden by `tast_arrt_expr` defined in chromeos/BUILD.gn, so that we
            # put the stub string here.
            tast_expr = "STUB_STRING_TO_RUN_TAST_TESTS",
            test_level_retries = 1,
            ci_only = True,
            timeout_sec = 14400,
            experiment_percentage = 100,
            shards = 2,
        ),
    },
)

# GTests to run on Chrome OS devices, but not Chrome OS VMs. Any differences
# between this and chromeos_system_friendly_gtests below should only be due
# to resource constraints (ie: not enough devices).
targets.legacy_basic_suite(
    name = "chromeos_device_only_gtests",
    tests = {
        "base_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chromeos_integration_tests_suite",
    tests = {
        "chromeos_integration_tests": targets.legacy_test_config(),
    },
)

# Tests that run on Chrome OS systems (ie: VMs, Chromebooks), *not*
# linux-chromeos.
# NOTE: We only want a small subset of test suites here, because most
# suites assume that they stub out the underlying device hardware.
# https://crbug.com/865693
targets.legacy_basic_suite(
    name = "chromeos_system_friendly_gtests",
    tests = {
        "aura_unittests": targets.legacy_test_config(
            args = [
                "--ozone-platform=headless",
            ],
        ),
        "base_unittests": targets.legacy_test_config(),
        "capture_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-jobs=1",
                # Don't run CaptureMJpeg tests on ChromeOS VM because vivid,
                # which is the virtual video capture device, doesn't support MJPEG.
                "--gtest_filter=-*UsingRealWebcam_CaptureMjpeg*",
            ],
        ),
        "cc_unittests": targets.legacy_test_config(),
        "crypto_unittests": targets.legacy_test_config(),
        "display_unittests": targets.legacy_test_config(),
        "video_decode_accelerator_tests_fake_vaapi_vp9": targets.legacy_test_config(
            ci_only = True,
        ),
        "video_decode_accelerator_tests_fake_vaapi_vp8": targets.legacy_test_config(
            ci_only = True,
            experiment_percentage = 100,
        ),
        "video_decode_accelerator_tests_fake_vaapi_av1": targets.legacy_test_config(
            ci_only = True,
            experiment_percentage = 100,
        ),
        "fake_libva_driver_unittest": targets.legacy_test_config(
            experiment_percentage = 100,
        ),
        "google_apis_unittests": targets.legacy_test_config(),
        "ipc_tests": targets.legacy_test_config(),
        "latency_unittests": targets.legacy_test_config(),
        "libcups_unittests": targets.legacy_test_config(),
        "media_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.media_unittests.filter",
            ],
        ),
        "midi_unittests": targets.legacy_test_config(),
        "mojo_unittests": targets.legacy_test_config(),
        # net_unittests has a test-time dependency on vpython. So add a CIPD'ed
        # vpython of the right arch to the task, and tell the test runner to copy
        # it over to the VM before the test runs.
        "net_unittests": targets.legacy_test_config(
            args = [
                "--vpython-dir=../../vpython_dir_linux_amd64",
                # PythonUtils.PythonRunTime (as opposed to Python3RunTime) requires a
                # copy of Python 2, but it's testing test helpers that are only used
                # outside of net_unittests. This bot runs out of space if trying to
                # ship two vpythons, so we exclude Python 2 and the one test which
                # uses it.
                "--gtest_filter=-PythonUtils.PythonRunTime",
            ],
            swarming = targets.swarming(
                shards = 3,
                cipd_packages = [
                    targets.cipd_package(
                        package = "infra/3pp/tools/cpython3/linux-amd64",
                        location = "vpython_dir_linux_amd64",
                        revision = "version:2@3.8.10.chromium.34",
                    ),
                    targets.cipd_package(
                        package = "infra/tools/luci/vpython3/linux-amd64",
                        location = "vpython_dir_linux_amd64",
                        revision = "git_revision:6ee2ba6ba03b09d8d8763f524aa77edf1945ca92",
                    ),
                    # required by vpython3
                    targets.cipd_package(
                        package = "infra/tools/cipd/linux-amd64",
                        location = "vpython_dir_linux_amd64",
                        revision = "git_revision:200dbdf0e967e81388359d3f85f095d39b35db67",
                    ),
                ],
            ),
        ),
        "ozone_gl_unittests": targets.legacy_test_config(
            args = [
                "--stop-ui",
            ],
        ),
        "ozone_unittests": targets.legacy_test_config(),
        "pdf_unittests": targets.legacy_test_config(),
        "printing_unittests": targets.legacy_test_config(),
        "profile_provider_unittest": targets.legacy_test_config(
            args = [
                "--stop-ui",
                "--test-launcher-jobs=1",
            ],
        ),
        "rust_gtest_interop_unittests": targets.legacy_test_config(),
        "sql_unittests": targets.legacy_test_config(),
        "url_unittests": targets.legacy_test_config(),
    },
)

# TODO: merge back into chromeos_system_friendly_gtests once everything is fixed.
targets.legacy_basic_suite(
    name = "chromeos_system_friendly_gtests_vmlab",
    tests = {
        "aura_unittests": targets.legacy_test_config(
            args = [
                "--ozone-platform=headless",
            ],
        ),
        "base_unittests": targets.legacy_test_config(),
        "capture_unittests": targets.legacy_test_config(
            args = [
                "--test-launcher-jobs=1",
                # Don't run CaptureMJpeg tests on ChromeOS VM because vivid,
                # which is the virtual video capture device, doesn't support MJPEG.
                "--gtest_filter=-*UsingRealWebcam_CaptureMjpeg*",
            ],
        ),
        "cc_unittests": targets.legacy_test_config(),
        "crypto_unittests": targets.legacy_test_config(),
        "display_unittests": targets.legacy_test_config(),
        "google_apis_unittests": targets.legacy_test_config(),
        "ipc_tests": targets.legacy_test_config(),
        "latency_unittests": targets.legacy_test_config(),
        "libcups_unittests": targets.legacy_test_config(),
        "media_unittests": targets.legacy_test_config(
            args = [
                # TODO(b/351276191): Switch to gerneral chromeos.betty.media_unittests.filter
                "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.betty.media_unittests.filter",
            ],
        ),
        "midi_unittests": targets.legacy_test_config(),
        "mojo_unittests": targets.legacy_test_config(),
        "net_unittests": targets.legacy_test_config(
            args = [
                # TODO(b/352673853): These tests require vpython on DUT.
                "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.betty.net_unittests.filter",
            ],
        ),
        "ozone_gl_unittests": targets.legacy_test_config(
            args = [
                "--stop-ui",
            ],
        ),
        "ozone_unittests": targets.legacy_test_config(),
        "pdf_unittests": targets.legacy_test_config(),
        "printing_unittests": targets.legacy_test_config(),
        "profile_provider_unittest": targets.legacy_test_config(
            args = [
                "--stop-ui",
                "--test-launcher-jobs=1",
            ],
        ),
        "rust_gtest_interop_unittests": targets.legacy_test_config(),
        "sql_unittests": targets.legacy_test_config(),
        "url_unittests": targets.legacy_test_config(),
    },
)

# TODO: merge back into chromeos_system_friendly_gtests once everything is fixed.
targets.legacy_basic_suite(
    name = "chromeos_system_friendly_gtests_fails_vmlab",
    tests = {
        "video_decode_accelerator_tests_fake_vaapi_vp9": targets.legacy_test_config(
            ci_only = True,
        ),
        # TODO(b/370554776): Promote following tests out of experimental
        "video_decode_accelerator_tests_fake_vaapi_vp8": targets.legacy_test_config(
            ci_only = True,
            experiment_percentage = 100,
        ),
        "video_decode_accelerator_tests_fake_vaapi_av1": targets.legacy_test_config(
            ci_only = True,
            experiment_percentage = 100,
        ),
        "fake_libva_driver_unittest": targets.legacy_test_config(
            experiment_percentage = 100,
        ),
    },
)

# vaapi_unittest needs to run with fake driver in some builders but others with real driver.
# Therefore these were isolated from chromeos_system_friendly_gtests.
targets.legacy_basic_suite(
    name = "chromeos_vaapi_gtests",
    tests = {
        "vaapi_unittest": targets.legacy_test_config(
            mixins = [
                "vaapi_unittest_args",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests",
    tests = {
        "absl_hardening_tests": targets.legacy_test_config(),
        "angle_unittests": targets.legacy_test_config(
            android_args = [
                "-v",
            ],
            use_isolated_scripts_api = True,
        ),
        "base_unittests": targets.legacy_test_config(),
        "blink_common_unittests": targets.legacy_test_config(),
        "blink_heap_unittests": targets.legacy_test_config(),
        "blink_platform_unittests": targets.legacy_test_config(),
        "boringssl_crypto_tests": targets.legacy_test_config(),
        "boringssl_ssl_tests": targets.legacy_test_config(),
        "capture_unittests": targets.legacy_test_config(
            args = [
                "--gtest_filter=-*UsingRealWebcam*",
            ],
        ),
        "cast_unittests": targets.legacy_test_config(),
        "components_browsertests": targets.legacy_test_config(),
        "components_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
            android_swarming = targets.swarming(
                shards = 6,
            ),
        ),
        "content_browsertests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 8,
            ),
            android_swarming = targets.swarming(
                shards = 15,
            ),
        ),
        "content_unittests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "crashpad_tests": targets.legacy_test_config(),
        "crypto_unittests": targets.legacy_test_config(),
        "env_chromium_unittests": targets.legacy_test_config(
            ci_only = True,
        ),
        "events_unittests": targets.legacy_test_config(),
        "fuzzing_unittests": targets.legacy_test_config(),
        "gcm_unit_tests": targets.legacy_test_config(),
        "gin_unittests": targets.legacy_test_config(),
        "google_apis_unittests": targets.legacy_test_config(),
        "gpu_unittests": targets.legacy_test_config(),
        "gwp_asan_unittests": targets.legacy_test_config(),
        "ipc_tests": targets.legacy_test_config(),
        "latency_unittests": targets.legacy_test_config(),
        "leveldb_unittests": targets.legacy_test_config(
            ci_only = True,
        ),
        "libjingle_xmpp_unittests": targets.legacy_test_config(),
        "liburlpattern_unittests": targets.legacy_test_config(),
        "media_unittests": targets.legacy_test_config(),
        "midi_unittests": targets.legacy_test_config(),
        "mojo_unittests": targets.legacy_test_config(),
        "net_unittests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "perfetto_unittests": targets.legacy_test_config(),
        # TODO(crbug.com/40274401): Enable this.
        # "rust_gtest_interop_unittests": None,
        "services_unittests": targets.legacy_test_config(),
        "shell_dialogs_unittests": targets.legacy_test_config(),
        "skia_unittests": targets.legacy_test_config(),
        "sql_unittests": targets.legacy_test_config(),
        "storage_unittests": targets.legacy_test_config(),
        "ui_base_unittests": targets.legacy_test_config(),
        "ui_touch_selection_unittests": targets.legacy_test_config(),
        "url_unittests": targets.legacy_test_config(),
        "webkit_unit_tests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 6,
            ),
        ),
        "wtf_unittests": targets.legacy_test_config(),
        "zlib_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_devices_with_graphical_output",
    tests = {
        "cc_unittests": targets.legacy_test_config(),
        "device_unittests": targets.legacy_test_config(),
        "display_unittests": targets.legacy_test_config(),
        "gfx_unittests": targets.legacy_test_config(),
        "unit_tests": targets.legacy_test_config(
            android_swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "viz_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_linux_and_chromeos_only",
    tests = {
        "dbus_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "chromium_gtests_for_win_and_linux_only",
    tests = {
        # pthreadpool is only built on Windows and Linux platforms, that is
        # determined by `build_tflite_with_xnnpack` defined in
        # third_party/tflite/features.gni.
        "pthreadpool_unittests": targets.legacy_test_config(
            ci_only = True,
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_web_tests_high_dpi_isolated_scripts",
    tests = {
        # high_dpi_blink_web_tests provides coverage for
        # running Layout Tests with forced device scale factor.
        "high_dpi_blink_web_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
        # high_dpi_blink_wpt_tests provides coverage for
        # running Layout Tests with forced device scale factor.
        "high_dpi_blink_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        # high_dpi_headless_shell_wpt_tests provides coverage for
        # running WPTs with forced device scale factor.
        "high_dpi_headless_shell_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "chromium_webkit_isolated_scripts",
    tests = {
        "blink_web_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "blink_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 7,
            ),
        ),
        "chrome_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
        "headless_shell_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "clang_tot_gtests",
    tests = {
        "base_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "client_v8_chromium_gtests",
    tests = {
        "app_shell_unittests": targets.legacy_test_config(),
        "browser_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "chrome_app_unittests": targets.legacy_test_config(),
        "chromedriver_unittests": targets.legacy_test_config(),
        "components_browsertests": targets.legacy_test_config(),
        "components_unittests": targets.legacy_test_config(),
        "compositor_unittests": targets.legacy_test_config(),
        "content_browsertests": targets.legacy_test_config(),
        "content_unittests": targets.legacy_test_config(),
        "device_unittests": targets.legacy_test_config(),
        "extensions_browsertests": targets.legacy_test_config(),
        "extensions_unittests": targets.legacy_test_config(),
        "gcm_unit_tests": targets.legacy_test_config(),
        "gin_unittests": targets.legacy_test_config(),
        "google_apis_unittests": targets.legacy_test_config(),
        "gpu_unittests": targets.legacy_test_config(),
        "headless_browsertests": targets.legacy_test_config(),
        "headless_unittests": targets.legacy_test_config(),
        "interactive_ui_tests": targets.legacy_test_config(),
        "net_unittests": targets.legacy_test_config(),
        "pdf_unittests": targets.legacy_test_config(),
        "remoting_unittests": targets.legacy_test_config(),
        "services_unittests": targets.legacy_test_config(),
        "sync_integration_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "unit_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "client_v8_chromium_isolated_scripts",
    tests = {
        "content_shell_crash_test": targets.legacy_test_config(),
        "telemetry_gpu_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/549140
            ),
        ),
        "telemetry_perf_unittests": targets.legacy_test_config(
            args = [
                "--xvfb",
                # TODO(crbug.com/40129085): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
            ],
            swarming = targets.swarming(
                shards = 12,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "telemetry_unittests": targets.legacy_test_config(
            args = [
                "--jobs=1",
                # Disable GPU compositing, telemetry_unittests runs on VMs.
                # https://crbug.com/871955
                "--extra-browser-args=--disable-gpu",
            ],
            swarming = targets.swarming(
                shards = 4,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "components_perftests_isolated_scripts",
    tests = {
        "components_perftests": targets.legacy_test_config(
            args = [
                "--gtest-benchmark-name=components_perftests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "desktop_chromium_isolated_scripts",
    tests = {
        "blink_python_tests": targets.legacy_test_config(),
        "blink_web_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "blink_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 7,
            ),
        ),
        "chrome_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
        "headless_shell_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 4,
            ),
        ),
        "content_shell_crash_test": targets.legacy_test_config(),
        "flatbuffers_unittests": targets.legacy_test_config(),
        "grit_python_unittests": targets.legacy_test_config(),
        "telemetry_gpu_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                idempotent = False,  # https://crbug.com/549140
            ),
        ),
        "telemetry_unittests": targets.legacy_test_config(
            args = [
                "--jobs=1",
                # Disable GPU compositing, telemetry_unittests runs on VMs.
                # https://crbug.com/871955
                "--extra-browser-args=--disable-gpu",
            ],
            swarming = targets.swarming(
                shards = 8,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
        "views_perftests": targets.legacy_test_config(
            args = [
                "--gtest-benchmark-name=views_perftests",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "devtools_browser_tests_suite",
    tests = {
        "devtools_browser_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "devtools_web_isolated_scripts",
    tests = {
        "blink_web_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "finch_smoke_tests",
    tests = {
        # TODO(crbug.com/40189140): Change this to the actual finch smoke test
        # once it exists.
        "base_unittests": targets.legacy_test_config(),
    },
)

# BEGIN tests which run on the GPU bots

targets.legacy_basic_suite(
    name = "gpu_common_and_optional_telemetry_tests",
    tests = {
        "info_collection_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                targets.magic_args.GPU_EXPECTED_VENDOR_ID,
                targets.magic_args.GPU_EXPECTED_DEVICE_ID,
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--force_high_performance_gpu",
            ],
        ),
        "trace_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

# GPU gtests which run on both the main and FYI waterfalls.
targets.legacy_basic_suite(
    name = "gpu_common_gtests_passthrough",
    tests = {
        "gl_tests_passthrough": targets.legacy_test_config(
            args = [
                "--use-gl=angle",
            ],
            chromeos_args = [
                "--stop-ui",
                targets.magic_args.CROS_GTEST_FILTER_FILE,
            ],
            desktop_args = [
                "--use-gpu-in-tests",
            ],
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "gl_unittests": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
            ],
            chromeos_args = [
                "--stop-ui",
                "--test-launcher-filter-file=../../testing/buildbot/filters/chromeos.gl_unittests.filter",
            ],
            desktop_args = [
                "--use-gpu-in-tests",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

# GPU gtests that test only Dawn
targets.legacy_basic_suite(
    name = "gpu_dawn_gtests",
    tests = {
        "dawn_end2end_implicit_device_sync_tests": targets.legacy_test_config(
            linux_args = [
                "--no-xvfb",
            ],
            ci_only = True,  # https://crbug.com/dawn/1749
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "dawn_end2end_skip_validation_tests": targets.legacy_test_config(
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "dawn_end2end_tests": targets.legacy_test_config(
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "dawn_end2end_wire_tests": targets.legacy_test_config(
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

# GPU gtests that test only Dawn with backend validation layers
targets.legacy_basic_suite(
    name = "gpu_dawn_gtests_with_validation",
    tests = {
        "dawn_end2end_validation_layers_tests": targets.legacy_test_config(
            linux_args = [
                "--no-xvfb",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_dawn_webgpu_cts",
    tests = {
        "webgpu_cts_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            swarming = targets.swarming(
                shards = 14,
            ),
            android_swarming = targets.swarming(
                shards = 36,
            ),
        ),
        "webgpu_cts_service_worker_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
            android_swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "webgpu_cts_dedicated_worker_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
            android_swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "webgpu_cts_shared_worker_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
            android_swarming = targets.swarming(
                shards = 2,
            ),
        ),
        "webgpu_cts_with_validation_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
                "webgpu_telemetry_cts",
                "linux_vulkan",
            ],
            args = [
                "--enable-dawn-backend-validation",
            ],
            swarming = targets.swarming(
                shards = 14,
            ),
            android_swarming = targets.swarming(
                shards = 36,
            ),
        ),
        # We intentionally do not have validation + worker tests since
        # no validation + worker should provide sufficient coverage.
    },
)

targets.legacy_basic_suite(
    name = "gpu_gl_passthrough_ganesh_telemetry_tests",
    tests = {
        "context_lost_gl_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
            ],
        ),
        "expected_color_pixel_gl_passthrough_ganesh_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "pixel_skia_gold_gl_passthrough_ganesh_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
            ],
        ),
        "screenshot_sync_gl_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=gl --disable-features=SkiaGraphite",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_metal_passthrough_ganesh_telemetry_tests",
    tests = {
        "context_lost_metal_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
            ],
        ),
        "expected_color_pixel_metal_passthrough_ganesh_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "pixel_skia_gold_metal_passthrough_ganesh_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
            ],
        ),
        "screenshot_sync_metal_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --disable-features=SkiaGraphite",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_metal_passthrough_graphite_telemetry_tests",
    tests = {
        "context_lost_metal_passthrough_graphite_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
            ],
        ),
        "expected_color_pixel_metal_passthrough_graphite_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "pixel_skia_gold_metal_passthrough_graphite_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
            ],
        ),
        "screenshot_sync_metal_passthrough_graphite_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --use-angle=metal --enable-features=SkiaGraphite",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_noop_sleep_telemetry_test",
    tests = {
        # The former GPU-specific generator script contained logic to
        # detect whether the so-called "experimental" GPU bots, which test
        # newer driver versions, were identical to the "stable" versions
        # of the bots, and if so to mirror their configurations. We prefer
        # to keep this new script simpler and to just configure this by
        # hand in waterfalls.pyl.
        "noop_sleep_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

# This is esentially a copy of gpu_passthrough_telemetry_tests running with
# Graphite. Initially limited to just the tests that pass on Android.
targets.legacy_basic_suite(
    name = "gpu_passthrough_graphite_telemetry_tests",
    tests = {
        "context_lost_passthrough_graphite_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --enable-features=SkiaGraphite",
            ],
        ),
        "expected_color_pixel_passthrough_graphite_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --enable-features=SkiaGraphite",
            ],
            android_args = [
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
        "pixel_skia_gold_passthrough_graphite_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --enable-features=SkiaGraphite",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
        "screenshot_sync_passthrough_graphite_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle --enable-features=SkiaGraphite",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_passthrough_telemetry_tests",
    tests = {
        "context_lost_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
            ],
        ),
        "expected_color_pixel_passthrough_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
            ],
            android_args = [
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "pixel_skia_gold_passthrough_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
        "screenshot_sync_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=passthrough --use-gl=angle",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_skia_renderer_vulkan_passthrough_telemetry_tests",
    tests = {
        "vulkan_pixel_skia_gold_test": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-vulkan=native --disable-vulkan-fallback-to-gl-for-testing --enable-features=Vulkan --use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_validating_telemetry_tests",
    tests = {
        "context_lost_validating_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--extra-browser-args=--use-cmd-decoder=validating",
            ],
        ),
        "expected_color_pixel_validating_test": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=validating",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
        "gpu_process_launch_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "hardware_accelerated_feature_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
        "pixel_skia_gold_validating_test": targets.legacy_test_config(
            mixins = [
                "skia_gold_test",
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--test-machine-name",
                "${buildername}",
                "--extra-browser-args=--use-cmd-decoder=validating",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
        "screenshot_sync_validating_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--dont-restore-color-profile-after-test",
                "--extra-browser-args=--use-cmd-decoder=validating",
            ],
            android_args = [
                # TODO(crbug.com/40134877): Remove this once we fix the tests.
                "--extra-browser-args=--force-online-connection-state-for-indicator",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webcodecs_telemetry_test",
    tests = {
        "webcodecs_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webcodecs_gl_passthrough_ganesh_telemetry_test",
    tests = {
        "webcodecs_gl_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webcodecs_metal_passthrough_ganesh_telemetry_test",
    tests = {
        "webcodecs_metal_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webcodecs_metal_passthrough_graphite_telemetry_test",
    tests = {
        "webcodecs_metal_passthrough_graphite_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_d3d11_passthrough_telemetry_tests",
    tests = {
        "webgl2_conformance_d3d11_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--webgl-conformance-version=2.0.1",
                targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=d3d11 --use-cmd-decoder=passthrough --force_high_performance_gpu",
            ],
            swarming = targets.swarming(
                # These tests currently take about an hour and fifteen minutes
                # to run. Split them into roughly 5-minute shards.
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_gl_passthrough_ganesh_telemetry_tests",
    tests = {
        "webgl2_conformance_gl_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            swarming = targets.swarming(
                # These tests currently take about an hour and fifteen minutes
                # to run. Split them into roughly 5-minute shards.
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_gl_passthrough_telemetry_tests",
    tests = {
        "webgl2_conformance_gl_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_gles_passthrough_telemetry_tests",
    tests = {
        "webgl2_conformance_gles_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                "--webgl-conformance-version=2.0.1",
                targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=gles --use-cmd-decoder=passthrough --force_high_performance_gpu",
            ],
            swarming = targets.swarming(
                # These tests currently take about an hour and fifteen minutes
                # to run. Split them into roughly 5-minute shards.
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl2_conformance_metal_passthrough_graphite_telemetry_tests",
    tests = {
        "webgl2_conformance_metal_passthrough_graphite_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            swarming = targets.swarming(
                shards = 20,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_d3d11_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_d3d11_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=d3d11 --use-cmd-decoder=passthrough --force_high_performance_gpu",
                targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_d3d9_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_d3d9_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=d3d9 --use-cmd-decoder=passthrough --force_high_performance_gpu",
                targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_gl_passthrough_ganesh_telemetry_tests",
    tests = {
        "webgl_conformance_gl_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_gl_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_gl_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=gl --use-cmd-decoder=passthrough --force_high_performance_gpu",
                targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_gles_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_gles_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=gles --use-cmd-decoder=passthrough --force_high_performance_gpu",
                targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
            ],
            swarming = targets.swarming(
                shards = 6,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_metal_passthrough_ganesh_telemetry_tests",
    tests = {
        "webgl_conformance_metal_passthrough_ganesh_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_metal_passthrough_graphite_telemetry_tests",
    tests = {
        "webgl_conformance_metal_passthrough_graphite_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_swangle_passthrough_representative_telemetry_tests",
    tests = {
        "webgl_conformance_swangle_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-gl=angle --use-angle=swiftshader --use-cmd-decoder=passthrough --force_high_performance_gpu",
                # We are only interested in running a 'smoketest' to test swangle
                # integration, not the full conformance suite.
                "--test-filter=conformance/rendering/gl-drawelements.html",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_validating_telemetry_tests",
    tests = {
        "webgl_conformance_validating_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-cmd-decoder=validating --force_high_performance_gpu",
                targets.magic_args.GPU_WEBGL_RUNTIME_FILE,
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
            android_swarming = targets.swarming(
                shards = 6,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "gpu_webgl_conformance_vulkan_passthrough_telemetry_tests",
    tests = {
        "webgl_conformance_vulkan_passthrough_tests": targets.legacy_test_config(
            mixins = [
                "gpu_integration_test_common_args",
            ],
            args = [
                # On dual-GPU devices we want the high-performance GPU to be active
                "--extra-browser-args=--use-angle=vulkan --use-cmd-decoder=passthrough --force_high_performance_gpu",
            ],
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

# END tests which run on the GPU bots

targets.legacy_basic_suite(
    name = "linux_chromeos_lacros_gtests",
    tests = {
        # Chrome OS (Ash) and Lacros only.
        "chromeos_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "linux_chromeos_specific_gtests",
    tests = {
        # Chrome OS only.
        "ash_components_unittests": targets.legacy_test_config(),
        "ash_unittests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 5,
            ),
        ),
        "ash_webui_unittests": targets.legacy_test_config(),
        "aura_unittests": targets.legacy_test_config(),
        "chromeos_components_unittests": targets.legacy_test_config(),
        "exo_unittests": targets.legacy_test_config(),
        "gl_unittests_ozone": targets.legacy_test_config(),
        "keyboard_unittests": targets.legacy_test_config(),
        "ozone_gl_unittests": targets.legacy_test_config(
            args = [
                "--ozone-platform=headless",
            ],
        ),
        "ozone_unittests": targets.legacy_test_config(),
        "ozone_x11_unittests": targets.legacy_test_config(),
        "shell_encryption_unittests": targets.legacy_test_config(),
        "ui_chromeos_unittests": targets.legacy_test_config(),
        "usage_time_limit_unittests": targets.legacy_test_config(
            experiment_percentage = 100,
        ),
        "wayland_client_perftests": targets.legacy_test_config(),
        "wayland_client_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "linux_flavor_specific_chromium_gtests",
    tests = {
        # Android, Chrome OS and Linux
        "sandbox_linux_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "linux_specific_chromium_isolated_scripts",
    tests = {
        # not_site_per_process_blink_web_tests provides coverage for
        # running Layout Tests without site-per-process.  This is the mode used
        # on Android and Android bots currently do not run the full set of
        # layout tests.  Running in this mode on linux compensates for lack of
        # direct Android coverage.
        "not_site_per_process_blink_web_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 8,
            ),
        ),
        # not_site_per_process_blink_wpt_tests provides coverage for
        # running WPTs without site-per-process.  This is the mode used
        # on Android and Android bots currently do not run the full set of
        # layout tests.  Running in this mode on linux compensates for lack of
        # direct Android coverage.
        "not_site_per_process_blink_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "not_site_per_process_headless_shell_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "webdriver_wpt_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 2,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "mac_specific_isolated_scripts",
    tests = {
        "mac_signing_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "model_validation_tests_light_suite",
    tests = {
        "model_validation_tests_light": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--out_dir=.",
            ],
            linux_args = [
                "--use-xvfb",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "model_validation_tests_suite",
    tests = {
        "model_validation_tests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            args = [
                "--out_dir=.",
            ],
            linux_args = [
                "--chromedriver",
                "chromedriver",
                "--binary",
                "chrome",
                "--use-xvfb",
            ],
            mac_args = [
                "--chromedriver",
                "chromedriver",
                "--binary",
                "Google Chrome.app/Contents/MacOS/Google Chrome",
            ],
            win_args = [
                "--chromedriver",
                "chromedriver.exe",
                "--binary",
                "Chrome.exe",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "mojo_python_unittests_isolated_scripts",
    tests = {
        "mojo_python_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "non_android_chromium_gtests",
    tests = {
        "accessibility_unittests": targets.legacy_test_config(),
        "app_shell_unittests": targets.legacy_test_config(),
        "blink_fuzzer_unittests": targets.legacy_test_config(),
        "browser_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 10,
            ),
        ),
        "chrome_app_unittests": targets.legacy_test_config(),
        "chromedriver_unittests": targets.legacy_test_config(),
        "extensions_browsertests": targets.legacy_test_config(),
        "extensions_unittests": targets.legacy_test_config(),
        "filesystem_service_unittests": targets.legacy_test_config(),  # https://crbug.com/862712
        "interactive_ui_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "message_center_unittests": targets.legacy_test_config(),
        "nacl_loader_unittests": targets.legacy_test_config(),
        "native_theme_unittests": targets.legacy_test_config(),
        "pdf_unittests": targets.legacy_test_config(),
        "ppapi_unittests": targets.legacy_test_config(),
        "printing_unittests": targets.legacy_test_config(),
        "remoting_unittests": targets.legacy_test_config(),
        "snapshot_unittests": targets.legacy_test_config(),
        "sync_integration_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                shards = 3,
            ),
        ),
        "ui_unittests": targets.legacy_test_config(),
        "views_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "ondevice_stability_tests_suite",
    tests = {
        "ondevice_stability_tests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            linux_args = [
                "--chromedriver",
                "chromedriver",
                "--binary",
                "chrome",
                "--no-xvfb",
            ],
            mac_args = [
                "--chromedriver",
                "chromedriver",
                "--binary",
                "Google Chrome.app/Contents/MacOS/Google Chrome",
            ],
            win_args = [
                "--chromedriver",
                "chromedriver.exe",
                "--binary",
                "Chrome.exe",
            ],
        ),
        "ondevice_stability_tests_light": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
            ],
            linux_args = [
                "--no-xvfb",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "chrome_ai_wpt_tests_suite",
    tests = {
        "chrome_ai_wpt_tests": targets.legacy_test_config(
            mixins = [
                "has_native_resultdb_integration",
                "blink_tests_write_run_histories",
            ],
            mac_args = [
                "--driver-name",
                "Google Chrome",
            ],
            swarming = targets.swarming(
                shards = 1,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "optimization_guide_android_gtests",
    tests = {
        "optimization_guide_components_unittests": targets.legacy_test_config(),
        # TODO(mgeorgaklis): Add optimization_guide_unittests when they become Android compatible.
    },
)

targets.legacy_basic_suite(
    name = "optimization_guide_cros_gtests",
    tests = {
        "optimization_guide_browser_tests": targets.legacy_test_config(),
        "optimization_guide_components_unittests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "optimization_guide_ios_unittests",
    tests = {
        "optimization_guide_unittests": targets.legacy_test_config(),
        "optimization_guide_gpu_unittests": targets.legacy_test_config(
            args = [
                "--ui-test-action-timeout=30000",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "optimization_guide_nogpu_gtests",
    tests = {
        "chrome_ml_unittests": targets.legacy_test_config(
            linux_args = [
                "--use-xvfb",
            ],
        ),
        "optimization_guide_browser_tests": targets.legacy_test_config(
            linux_args = [
                "--use-xvfb",
            ],
        ),
        "optimization_guide_components_unittests": targets.legacy_test_config(
            linux_args = [
                "--use-xvfb",
            ],
        ),
        "optimization_guide_unittests": targets.legacy_test_config(
            linux_args = [
                "--use-xvfb",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "optimization_guide_gpu_gtests",
    tests = {
        "optimization_guide_gpu_unittests": targets.legacy_test_config(
            args = [
                "--ui-test-action-timeout=30000",
            ],
            linux_args = [
                "-use-xvfb",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "telemetry_perf_unittests_isolated_scripts",
    tests = {
        "telemetry_perf_unittests": targets.legacy_test_config(
            args = [
                # TODO(crbug.com/40129085): Remove this once Crashpad is the default.
                "--extra-browser-args=--enable-crashpad",
            ],
            swarming = targets.swarming(
                shards = 12,
                idempotent = False,  # https://crbug.com/549140
            ),
            resultdb = targets.resultdb(
                enable = True,
            ),
        ),
    },
)

targets.legacy_basic_suite(
    name = "vulkan_swiftshader_isolated_scripts",
    tests = {
        "vulkan_swiftshader_blink_web_tests": targets.legacy_test_config(),
    },
)

targets.legacy_basic_suite(
    name = "webrtc_chromium_wpt_tests",
    tests = {
        "blink_wpt_tests": targets.legacy_test_config(
            args = [
                "-t",
                "Release",
                "external/wpt/webrtc",
                "external/wpt/webrtc-encoded-transform",
                "external/wpt/webrtc-extensions",
                "external/wpt/webrtc-priority",
                "external/wpt/webrtc-stats",
                "external/wpt/webrtc-svc",
            ],
        ),
        "headless_shell_wpt_tests": targets.legacy_test_config(
            args = [
                "-t",
                "Release",
                "external/wpt/webrtc",
                "external/wpt/webrtc-encoded-transform",
                "external/wpt/webrtc-extensions",
                "external/wpt/webrtc-priority",
                "external/wpt/webrtc-stats",
                "external/wpt/webrtc-svc",
            ],
        ),
    },
)

targets.legacy_basic_suite(
    name = "win_specific_isolated_scripts",
    tests = {
        "mini_installer_tests": targets.legacy_test_config(
            swarming = targets.swarming(
                dimensions = {
                    "integrity": "high",
                },
            ),
        ),
        "polymer_tools_python_unittests": targets.legacy_test_config(
            experiment_percentage = 0,
        ),
    },
)
