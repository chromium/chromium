#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Produce a list of directories that contain files with instances of
# non-inclusive language (e.g. whitelist, blacklist, master, slave). Each
# line is formatted as the relative path of the directory.
#
# The intended consumer of this list is the CheckInclusiveLanguage function
# in ../PRESUBMIT.py. It does not perform its check on files directly under
# the paths listed here.
#
# See https://bugs.chromium.org/p/chromium/issues/detail?id=1177609 for more
# context.
#
# To update the list so the PRESUBMIT will use it in prod, write the output to
# ./inclusive_language_presubmit_exempt_dirs in a new CL, and land it. Example:
#
#  % infra/update_inclusive_language_presubmit_exempt_dirs.sh > \
#    infra/inclusive_language_presubmit_exempt_dirs.txt
#
# Note: This script produces the list relative to the current working dir.
# To generate the list of exempted legacy directories for chromium/src for
# instance, you should run this from the parent of the current directory
# rather than from this directory.

git grep --ignore-case -l -E '\b((black|white)list|master|slave)\b' | \
  xargs -n1 dirname | \
  sort -u