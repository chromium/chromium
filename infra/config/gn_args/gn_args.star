# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Execute this file to set up some common GN arg configs for Chromium builders.

load("//lib/gn_args.star", "gn_args")

gn_args.config(
    name = "official_optimize",
    args = {
        "is_official_build": True,
    },
)

gn_args.config(
    name = "reclient",
    args = {
        "use_remoteexec": True,
    },
)

gn_args.config(
    name = "minimal_symbols",
    args = {
        "symbol_level": 1,
    },
)

gn_args.config(
    name = "dcheck_always_on",
    args = {
        "dcheck_always_on": True,
    },
)

gn_args.config(
    name = "debug",
    args = {
        "is_debug": True,
    },
)

gn_args.config(
    name = "shared",
    args = {
        "is_component_build": True,
    },
)

gn_args.config(
    name = "goma",
    args = {
        "use_goma": True,
    },
)

gn_args.config(
    name = "try_builder",
    configs = [
        "minimal_symbols",
        "dcheck_always_on",
    ],
)

gn_args.config(
    name = "debug_build",
    configs = [
        "debug",
        "shared",
        "minimal_symbols",
    ],
)
