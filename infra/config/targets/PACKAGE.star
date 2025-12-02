# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Package declaration for the chromium-targets library."""

pkg.declare(
    name = "@chromium-targets",
    lucicfg = "1.46.1",
)

# declarations.star is a top-level script that executes all of the rest starlark
# files within the directory so that other packages can just executes this file
# to execute all the files.
pkg.entrypoint("declarations.star")

pkg.options.lint_checks([
    "all",
    # Let dicts be not sorted, we can use go/keep-sorted for dicts that we want
    # sorted
    "-unsorted-dict-items",
])

pkg.depend(
    name = "@chromium-luci",
    source = pkg.source.googlesource(
        host = "chromium",
        repo = "infra/chromium",
        ref = "refs/heads/main",
        path = "starlark-libs/chromium-luci",
        revision = "b859f278471bfa7328a74494c7d9db377ae3e931",
    ),
)
