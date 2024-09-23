#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Verify HEAD against upstream to see if anything bad has been added to
# HEAD that we would not want to roll.  For example, if HEAD introduces any
# UNCHECKED_BITSTREAM_READERs, then we definitely want to fix that before
# including HEAD in Chromium.
#
# Usage: check_merge.py [new_branch]
# where |new_branch| is the branch that we'd like to compare to upstream.

import os
import re
import sys
import subprocess

# The current good ffmpeg + config that we're diffing against.
EXISTING_COMMIT = "origin/master"  # nocheck

# If any of these regexes match, it indicates failure.  Generally, we want to
# catch any change in how these are defined.  These are checked against things
# that are added with respect to |EXISTING_COMMIT| by the commit we're checking.
#
# Remember that we're going to check ffmpeg files and Chromium files (e.g.,
# the build configs for each platform, ffmpeg_generated.gni, etc.).
# TODO(liberato): consider making these a dictionary, with the value as some
# descriptive text (e.g., the bug number) about why it's not allowed.
INSERTION_TRIPWIRES = [
    # In Chromium, all codecs should use the safe bitstream reader. Roller must
    # undo any new usage of the unchecked reader.
    "^.define.*UNCHECKED_BITSTREAM_READER.*1",

    # In Chromium, explicitly skipping some matroskadec code blocks for
    # the following CONFIG variables (which should remain defined as 0) is
    # necessary to remove code that may be a security risk. Discuss with cevans@
    # before removing these explicit skips or enabling these variables.
    "^.define.*CONFIG_LZO.*1",
    "^.define.*CONFIG_SIPR_DECODER.*1",
    "^.define.*CONFIG_RA_288_DECODER.*1",
    "^.define.*CONFIG_COOK_DECODER.*1",
    "^.define.*CONFIG_ATRAC3_DECODER.*1",

    # Miscellaneous tripwires.
    "^.define.*CONFIG_SPDIF_DEMUXER.*1",
    "^.define.*CONFIG_W64_DEMUXER.*1",
    "^.define.*[Vv]4[Ll]2.*1",
    "^.define.*CONFIG_PRORES_.*1",
]

# Filenames that will be excluded from the regex matching.
# Note: chromium/scripts can be removed once the scripts move out of ffmpeg.
EXCLUDED_FILENAMES = [
    r"^configure$",
    r"^chromium/scripts/",
    r"^chromium/patches/",
]


def search_regexps(text, regexps):
    return [r for r in regexps if re.search(r, text)]


def main(argv):
    # What we're considering merging, and would like to check.  Normally, this
    # is HEAD, but you might want to verify some previous merge.
    if len(argv) > 1:
        new_commit = argv[1]
    else:
        new_commit = "HEAD"

    print(f"Comparing {new_commit} to baseline {EXISTING_COMMIT}...")

    diff = subprocess.Popen(
        ["git", "diff", "-U0", EXISTING_COMMIT, new_commit],
        stdout=subprocess.PIPE).communicate()[0].decode(sys.stdout.encoding)
    filename = None
    skip = False
    files_encountered = 0
    files_skipped = 0
    failures = set()
    for line in diff.splitlines():
        if line.startswith("+++"):
            # +++ b/filename => filename
            filename = line.split("/", 1)[1]
            skip = False
            files_encountered += 1
            if search_regexps(filename, EXCLUDED_FILENAMES):
                skip = True
                files_skipped += 1
        elif line.startswith("+") and not skip:
            # |line| is an insertion into |new_commit|.
            # Drop the leading "+" from the string being searched.
            tripwire = search_regexps(line[1:], INSERTION_TRIPWIRES)
            if tripwire:
                failures.add("Tripwire '%s' found in %s" %
                             (tripwire, filename))

    # If we have failures, then print them and fail.
    if failures:
        for failure in failures:
            print(f"Failure: {failure}")
        sys.exit(2)

    checked = files_encountered - files_skipped
    print(f"No problems found! Checked {checked}, skipped {files_skipped}.")


if __name__ == '__main__':
    main(sys.argv)
