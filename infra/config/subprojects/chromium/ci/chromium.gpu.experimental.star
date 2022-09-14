# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Definitions of builders in the chromium.gpu.experimental builder group."""

load("//lib/consoles.star", "consoles")

# This view is intended for parking inactive experimental testers until they are
# needed again in order to declutter chromium.gpu.fyi.
consoles.list_view(
    name = "chromium.gpu.experimental",
)
