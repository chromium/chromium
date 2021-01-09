# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "cpu", "os")
load("//lib/try.star", "try_")

try_.defaults.set(
    bucket = "try",
    build_numbers = True,
    caches = [
        swarming.cache(
            name = "win_toolchain",
            path = "win_toolchain",
        ),
    ],
    configure_kitchen = True,
    cores = 8,
    cpu = cpu.X86_64,
    cq_group = "cq",
    executable = "recipe:chromium_trybot",
    execution_timeout = 2 * time.hour,
    # Max. pending time for builds. CQ considers builds pending >2h as timed
    # out: http://shortn/_8PaHsdYmlq. Keep this in sync.
    expiration_timeout = 2 * time.hour,
    os = os.LINUX_DEFAULT,
    pool = "luci.chromium.try",
    service_account = "chromium-try-gpu-builder@chops-service-accounts.iam.gserviceaccount.com",
    subproject_list_view = "luci.chromium.try",
    swarming_tags = ["vpython:native-python-wrapper"],
    task_template_canary_percentage = 5,
)

try_.chromium_angle_ios_builder(
    name = "ios-angle-try-intel",
    pool = "luci.chromium.gpu.mac.mini.intel.uhd630.try",
)
