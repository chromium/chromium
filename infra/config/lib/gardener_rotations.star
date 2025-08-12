# Copyright 20205 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("@chromium-luci//gardener_rotations.star", _gardener_rotations = "gardener_rotations")

# Gardener rotations that a builder can be added to (only takes effect on trunk)
# New rotations can be added, but won't automatically show up in SoM without
# changes to SoM code.
gardener_rotations = struct(
    ANDROID = _gardener_rotations.rotation("android", "android rotation", "android tree closers"),
    ANGLE = _gardener_rotations.rotation("angle", "angle rotation", None),
    CHROMIUM = _gardener_rotations.rotation("chromium", "chromium rotation", "chromium tree closers"),
    CFT = _gardener_rotations.rotation("cft", "cft rotation", None),
    DAWN = _gardener_rotations.rotation("dawn", "dawn rotation", None),
    FUCHSIA = _gardener_rotations.rotation("fuchsia", "fuchsia rotation", None),
    CHROMIUM_CLANG = _gardener_rotations.rotation("chromium.clang", "chromium.clang rotation", None),
    CHROMIUM_GPU = _gardener_rotations.rotation("chromium.gpu", "chromium.gpu rotation", "chromium.gpu tree closers"),
    IOS = _gardener_rotations.rotation("ios", "ios rotation", "ios tree closers"),
    CHROMIUMOS = _gardener_rotations.rotation("chromiumos", "chromiumos rotation", "chromiumos tree closers"),  # This group is not on SoM.
    CRONET = _gardener_rotations.rotation("cronet", "cronet rotation", None),
)
