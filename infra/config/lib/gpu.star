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
        optional_tests_builder = _gpu_try_optional_tests_builder,
    ),
)
