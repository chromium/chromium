# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//builders.star", "cpu", "os")
load("@chromium-luci//ci.star", "ci")
load("@chromium-luci//try.star", "try_")
load("./ci_constants.star", "ci_constants")

def _gpu_ci_linux_builder(*, name, **kwargs):
    """Defines a GPU-related linux builder.

    This sets linux-specific defaults that are common to GPU-related builder
    groups.
    """
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.LINUX_DEFAULT)
    return ci.builder(name = name, **kwargs)

def _gpu_ci_mac_builder(*, name, **kwargs):
    """Defines a GPU-related mac builder.

    This sets mac-specific defaults that are common to GPU-related builder
    groups.
    """
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", None)
    kwargs.setdefault("cpu", cpu.ARM64)
    kwargs.setdefault("os", os.MAC_ANY)
    return ci.builder(name = name, **kwargs)

def _gpu_ci_windows_builder(*, name, **kwargs):
    """Defines a GPU-related windows builder.

    This sets windows-specific defaults that are common to GPU-related builder
    groups.
    """
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", 8)
    kwargs.setdefault("os", os.WINDOWS_ANY)
    kwargs.setdefault("ssd", None)
    kwargs.setdefault("free_space", None)
    return ci.builder(name = name, **kwargs)

def _gpu_try_optional_tests_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", False)
    kwargs.setdefault("execution_timeout", 6 * time.hour)
    kwargs.setdefault("service_account", gpu.try_.SERVICE_ACCOUNT)
    return try_.builder(name = name, **kwargs)

_common_location_filters = [
    # Inclusion filters.
    cq.location_filter(path_regexp = "chrome/browser/vr/.+"),
    cq.location_filter(path_regexp = "content/browser/xr/.+"),
    cq.location_filter(path_regexp = "content/test/data/gpu/.+"),
    cq.location_filter(path_regexp = "content/test/gpu/.+"),
    cq.location_filter(path_regexp = "gpu/.+"),
    cq.location_filter(path_regexp = "media/audio/.+"),
    cq.location_filter(path_regexp = "media/base/.+"),
    cq.location_filter(path_regexp = "media/capture/.+"),
    cq.location_filter(path_regexp = "media/filters/.+"),
    cq.location_filter(path_regexp = "media/gpu/.+"),
    cq.location_filter(path_regexp = "media/mojo/.+"),
    cq.location_filter(path_regexp = "media/renderers/.+"),
    cq.location_filter(path_regexp = "media/video/.+"),
    cq.location_filter(path_regexp = "testing/trigger_scripts/.+"),
    cq.location_filter(path_regexp = "third_party/blink/renderer/modules/mediastream/.+"),
    cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webcodecs/.+"),
    cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgl/.+"),
    cq.location_filter(path_regexp = "third_party/blink/renderer/modules/webgpu/.+"),
    cq.location_filter(path_regexp = "third_party/blink/renderer/platform/graphics/gpu/.+"),
    cq.location_filter(path_regexp = "tools/clang/scripts/update.py"),
    cq.location_filter(path_regexp = "ui/gl/.+"),

    # Exclusion filters.
    cq.location_filter(exclude = True, path_regexp = ".*\\.md"),
]

_android_specific_location_filters = [
    # Inclusion filters.
    cq.location_filter(path_regexp = "cc/.+"),
    cq.location_filter(path_regexp = "components/viz/.+"),
    cq.location_filter(path_regexp = "services/viz/.+"),
]

_linux_specific_location_filters = []

_mac_specific_location_filters = [
    # Inclusion filters.
    cq.location_filter(path_regexp = "services/shape_detection/.+"),
]

_windows_specific_location_filters = [
    # Inclusion filters.
    cq.location_filter(path_regexp = "chrome/browser/media/.+"),
    cq.location_filter(path_regexp = "components/cdm/renderer/.+"),
    cq.location_filter(path_regexp = "device/vr/.+"),
    cq.location_filter(path_regexp = "media/cdm/.+"),
    cq.location_filter(path_regexp = "services/on_device_model/.+"),
    cq.location_filter(path_regexp = "services/webnn/.+"),
    cq.location_filter(path_regexp = "third_party/blink/renderer/modules/vr/.+"),
    cq.location_filter(path_regexp = "third_party/blink/renderer/modules/xr/.+"),
]

def _location_filter_to_sort_key(location_filter):
    # Put inclusion filters before exclusion filters.
    prefix = "a"
    if location_filter.exclude:
        prefix = "b"
    return prefix + location_filter.path_regexp

def _append_to_common_filters_and_sort(additional_location_filters):
    return sorted(_common_location_filters + additional_location_filters, key = _location_filter_to_sort_key)

_optional_trybot_location_filters = struct(
    ANDROID = _append_to_common_filters_and_sort(_android_specific_location_filters),
    LINUX = _append_to_common_filters_and_sort(_linux_specific_location_filters),
    MAC = _append_to_common_filters_and_sort(_mac_specific_location_filters),
    WINDOWS = _append_to_common_filters_and_sort(_windows_specific_location_filters),
)

gpu = struct(
    ci = struct(
        POOL = "luci.chromium.gpu.ci",
        SERVICE_ACCOUNT = "chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        SHADOW_SERVICE_ACCOUNT = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        TREE_CLOSING_NOTIFIERS = ci_constants.DEFAULT_TREE_CLOSING_NOTIFIERS + ["gpu-tree-closer-email"],
        linux_builder = _gpu_ci_linux_builder,
        mac_builder = _gpu_ci_mac_builder,
        windows_builder = _gpu_ci_windows_builder,
    ),
    try_ = struct(
        SERVICE_ACCOUNT = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        optional_trybot_location_filters = _optional_trybot_location_filters,
        optional_tests_builder = _gpu_try_optional_tests_builder,
    ),
)
