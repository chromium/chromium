# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains bundle definitions, which are groupings of targets that can
# be referenced by other bundles or by builders. Bundles cannot be used in
# //testing/buildbot

load("//lib/targets.star", "targets")

# TODO(dpranke): These are run on the p/chromium waterfall; they should
# probably be run on other builders, and we should get rid of the p/chromium
# waterfall.
targets.bundle(
    name = "public_build_scripts",
    targets = [
        "checkbins",
    ],
)
