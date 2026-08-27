# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("./ci_constants.star", "ci_constants")

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
        SERVICE_ACCOUNT = "chromium-ci-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        SHADOW_SERVICE_ACCOUNT = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        TREE_CLOSING_NOTIFIERS = ci_constants.DEFAULT_TREE_CLOSING_NOTIFIERS + ["gpu-tree-closer-email"],
    ),
    try_ = struct(
        SERVICE_ACCOUNT = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
        optional_trybot_location_filters = _optional_trybot_location_filters,
    ),
)
