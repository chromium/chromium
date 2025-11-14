#!/usr/bin/env lucicfg
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""One single file to execute all the rest starlark files in this directory.

This makes it easier for other lucicfg packages to execute all files in this
package.
"""

exec("//basic_suites.star")
exec("//binaries.star")
exec("//bundles.star")
exec("//compile_targets.star")
exec("//compound_suites.star")
exec("//matrix_compound_suites.star")
exec("//mixins.star")
exec("//tests.star")
exec("//variants.star")
