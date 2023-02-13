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
    bucket = "try",
    executable = "recipe:perf/crossbench",
    pool = "luci.flex.try",
    builderless = True,
    list_view = "crossbench.try",
)

builders.builder(
    name = "Crossbench CBB Mac Try",
    os = os.MAC_ANY,
)
