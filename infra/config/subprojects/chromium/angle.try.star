# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "os", "siso")
load("//lib/builder_config.star", "builder_config")
load("//lib/gn_args.star", "gn_args")
load("//lib/try.star", "try_")
load("//lib/xcode.star", "xcode")

try_.defaults.set(
    bucket = "try",
    executable = "recipe:angle_chromium_trybot",
    builder_group = "tryserver.chromium.angle",
    pool = "luci.chromium.try",
    cores = 8,
    os = os.LINUX_DEFAULT,
    cpu = cpu.X86_64,
    build_numbers = True,
    cq_group = "cq",
    execution_timeout = 2 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    siso_project = siso.project.DEFAULT_UNTRUSTED,
    siso_remote_jobs = siso.remote_jobs.LOW_JOBS_FOR_CQ,
    subproject_list_view = "luci.chromium.try",
    task_template_canary_percentage = 5,
)

def angle_mac_builder(*, name, **kwargs):
    kwargs.setdefault("builderless", True)
    kwargs.setdefault("cores", None)
    kwargs.setdefault("os", os.MAC_ANY)
    kwargs.setdefault("ssd", None)
    return try_.builder(name = name, **kwargs)

def angle_ios_builder(*, name, **kwargs):
    kwargs.setdefault("xcode", xcode.xcode_default)
    return angle_mac_builder(name = name, **kwargs)

angle_ios_builder(
    name = "ios-angle-try-intel",
    mirrors = [
        "ci/ios-angle-builder",
        "ci/ios-angle-intel",
    ],
    builder_config_settings = builder_config.try_settings(
        retry_failed_shards = False,
    ),
    gn_args = gn_args.config(
        configs = [
            "ci/ios-angle-builder",
            "no_symbols",
        ],
    ),
    pool = "luci.chromium.gpu.mac.mini.intel.try",
)
