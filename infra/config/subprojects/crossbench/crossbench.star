# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Library containing builder definitions for crossbench subproject
"""

load("//lib/consoles.star", "consoles")
load("//lib/builders.star", "builders", "defaults", "os")

consoles.list_view(
    name = "crossbench.try",
)

defaults.set(
    executable = "recipe:perf/crossbench",
    bucket = "try",
    pool = "luci.flex.try",
    list_view = "crossbench.try",
    builderless = True,
)

builders.builder(
    name = "Crossbench CBB Mac Try",
    os = os.MAC_ANY,
)
