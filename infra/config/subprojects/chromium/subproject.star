# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/consoles.star", "consoles")
load("//console-header.star", "HEADER")
load("//project.star", "settings")

consoles.defaults.set(
    header = HEADER,
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = [settings.ref],
)

exec("./ci.star")
exec("./try.star")
exec("./infra.star")

# TODO(gbeaty) Move the builders in these files into the per-builder group
# files, this can't be done during the freeze because it changes the grace
# period
exec("./gpu.try.star")
exec("./angle.try.star")
exec("./swangle.try.star")
