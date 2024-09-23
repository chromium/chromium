#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
"""Usage: find_patches.py [origin_branch] [> patch_file]

This will find all changes in |origin_branch| that are not part of upstream,
and print a report.  It tries to include deleted lines, though these are
heuristic at best.  If |origin_branch| is omitted, it will default to HEAD.

Changes in the working directory are ignored.

Output will be written to stdout, so you probably want to redirect it.

For example, to generate the patches file for origin/merge-m68:
find_patches.py origin/merge-m68 > patches.68
"""

import collections
import os
import re
import sys
import subprocess

from robo_lib import shell

# What directory will we look for patches in?
# TODO(liberato): Should we find the root of the ffmpeg tree?
PATH = "."


def log(msg):
    print(f"[{msg}]", file=sys.stderr)


class PatchInfo:
    """ Structure to keep track of one patch in a diff.

  This class encapsulates how to handle inserted / deleted lines in a patch,
  mostly so that we can decide if we should apply "deleted lines only"
  processing to any them, to find what commit deleted them.  Because deleted
  lines result in an approximate search, we want to be reasonably sure that
  any deleted lines aren't actually just changes ("delete old, add new").
  """

    def __init__(self):
        # Does a diff insert any lines?
        self._does_insert = False
        # Set of lines that a diff deletes.
        self._deleted_lines = set()
        # Is the patch a whole-file delete in origin of an upstream file?
        self._is_delete_of_file_in_origin = False

    def record_inserted_line(self, line):
        """ Records that |line| was inserted as part of the patch.

    |line| is a string from the patch, e.g., "+ foo that was added;"
    """
        self._does_insert = True

    def record_deleted_line(self, line):
        """ Records that |line| was deleted as part of the patch.

    |line| is a string from the patch, e.g., "- foo that was removed;"
    """
        self._deleted_lines.add(line)

    def interesting_deleted_lines(self):
        """ Return the (possibly empty) set of deleted lines that we should track.

    In general, things that remove but also add probably are changes, and
    can be ignored as noise.  While, with perfect deleted line tracking,
    this wouldn't actually change the result, we really just do a text
    search for deleted lines later.  So, avoiding noise is good.

    Note that this is approximate -- a diff could have deleted and
    inserted lines near each other, but from different patches.  In other
    words, patch A could delete lines and patch B could add / change them.
    If those changes end up in the same diff block, then we'll miss A
    because of this test.  However, in practice, checking for both seems
    to remove some noise.

    Also note that, if the patch is a whole-file deletion in origin, then none
    of the missing lines are interesting.  The whole file is gone, and it'll
    be handled specially elsewhere, rather than trying to 'git blame' specific
    deleted lines.
    """
        if self._deleted_lines and not self._does_insert and not self._is_delete_of_file_in_origin:
            return self._deleted_lines
        return set()

    def set_is_delete_of_file_in_origin(self):
        """ Records that this patch is a whole-file deletion."""
        self._is_delete_of_file_in_origin = True

    def is_delete_of_file_in_origin(self):
        return self._is_delete_of_file_in_origin


def main(argv):
    # Origin branch that contains the patches we want to find.
    # Can specify, for example "origin/merge-m68" to get the  patches file for
    # that revision, regardless of the state of the working tree.
    if len(argv) > 1:
        origin_branch = argv[1]
    else:
        origin_branch = "HEAD"

    # Make sure that upstream is up-to-date, else many things will likely not
    # be reachable from it.  We don't do this if run as part of a script.
    if subprocess.call(["git", "fetch", "upstream"]):
        raise Exception("Could not fetch from upstream")

    write_patches_file(origin_branch, sys.stdout)


def write_patches_file(origin_branch, output_file):
    """Write the patches file for |origin_branch| to |output_file|."""
    # Get the latest upstream commit that's reachable from the origin branch.
    # We'll use that to compare against.
    upstream = shell.output_or_error(
        ["git", "merge-base", "upstream/master", origin_branch])  # nocheck
    if not upstream:
        raise Exception("Could not find upstream commit")

    # "Everything reachable from |origin_branch| but not |upstream|".  In other
    # words, all and only chromium changes.  Note that there are non-chromium
    # authors here, since it will include cherry-picks to origin.
    revision_range = "%s..%s" % (upstream, origin_branch)

    log("Origin is %s" % origin_branch)
    log("Upstream is %s" % upstream)

    # Find diffs between the versions, excluding all files that are only on
    # origin.  We explicitly exclude .gitignore, since it exists in both places.
    # Ask for no context, since we ignore it anyway.
    diff = shell.output_or_error([
        "git", "diff", "--diff-filter=a", "-U0", revision_range, PATH,
        ":!.gitignore"
    ])

    # Set of chromium patch sha1s we've seen.
    sha1s = set()
    # Map of sha1 to set of files that it affects.
    sha1ToFiles = collections.defaultdict(set)
    # Mapping of filename to set of lines that were deleted.
    files_to_deleted_lines = {}
    patch_info = PatchInfo()
    filename = None
    # Files that were deleted in origin but exist in upstream.
    files_deleted_in_origin = set()

    last_minus_file = None

    # Process each diff.  Include a dummy line to flush out the last diff.
    log("Scanning diffs between origin and upstream")
    for line in diff.splitlines() + [
            "+++ just to handle deleted lines properly"
    ]:
        if line.startswith("+++"):
            # If the previous patch was delete-only, then we need to search for it
            # differently, since we don't get blame entries for deleted lines.
            # Add the set of deleted lines to this filename.  Remember that whole-file
            # deletions have no interesting deleted lines, and are handled even more
            # differently than deleted lines inside the file.
            deleted_lines = patch_info.interesting_deleted_lines()
            if deleted_lines:
                files_to_deleted_lines[filename] = deleted_lines

            # Start of a new diff.  We don't know if it inserts / deletes lines.
            patch_info = PatchInfo()

            # Update to the new filename.
            # If the line is "+++ /dev/null", then it means that chromium deleted the
            # file, while upstream has it.  Note that it does not contain the "a/"
            # or "b/" that we'd expect, so line[6:] would grab "ev/null" if we didn't
            # handle it specially.
            if "/dev/null" in line:
                files_deleted_in_origin.add(last_minus_file)
                patch_info.set_is_delete_of_file_in_origin()
                log("File was deleted in origin: %s" % last_minus_file)
            else:
                filename = line[6:]
                log("Checking diffs in %s" % filename)
        elif line.startswith(
                "@@") and not patch_info.is_delete_of_file_in_origin():
            # @@ -linespec +linespec @@
            # linespec is either "line_number,number_of_lines" or "line_number".
            # Extract the "+linespec", which is what was added by |origin|.
            # If the number of lines is specified as 0, then it's a deletion only.
            # If the number of lines is unspecified, then it's 1.
            # If the diff is a whole-file delete, then don't do any of this, since
            # it's not going to work anyway.  We'll look up the whole file later.
            added_linespec = re.sub(r"^.*\+(.*) @@.*", r"\1", line)
            # Figure out the lines to blame.  This is just "starting_line,+number".
            if "," in added_linespec:
                # linespec is "line_number,number_of_lines"
                added_parts = added_linespec.split(",")
                # Skip if this is a deletion.
                if added_parts[1] == "0":
                    continue
                blame_range = "%s,+%s" % (added_parts[0], added_parts[1])
            else:
                # One-line change
                blame_range = "%s,+1" % added_linespec

            blame = shell.output_or_error([
                "git", "blame", "-l",
                "-L %s" % blame_range, revision_range, "--", filename
            ])

            # Collect sha1 lines, and create a mapping of files that is changed by
            # each sha1.
            for blame_line in blame.splitlines():
                sha1 = blame_line.split(" ", 1)[0]
                if sha1:
                    sha1s.add(sha1)
                    sha1ToFiles[sha1].add(filename)
        elif line.startswith("---"):
            # Do nothing.  Just avoid matching "---" when we check for "-"
            # Record the filename, though, in case we deleted it in origin.  We won't
            # get the filename in the upcoming +++; it'll be "+++ /dev/null".
            last_minus_file = line[6:]
            pass
        elif line.startswith("-"):
            # This diff does delete lines.
            patch_info.record_deleted_line(line[1:])
        elif line.startswith("+"):
            # This diff does insert lines.
            patch_info.record_inserted_line(line[1:])

    # For all files that have deleted lines, look for the sha1 that deleted them.
    # This is heuristic only; we're looking for "commits that contain some text".
    for filename, deleted_lines in files_to_deleted_lines.items():
        for deleted_line in deleted_lines:
            # Make sure that the deleted line is long enough to provide context.
            if len(deleted_line) < 4:
                continue

            # git log freaks out if you search for a line starting with #, remove it.
            while deleted_line.startswith('#'):
                deleted_line = deleted_line[1:]

            log("Checking for deleted lines in %s" % filename)
            # Specify "--first-parent" so that we find commits on (presumably) origin.
            sha1 = shell.output_or_error([
                "git", "log", "-1", revision_range, "--format=%H", "-S",
                deleted_line, origin_branch, "--", filename
            ])

            # Add the sha1 to the sets
            sha1s.add(sha1)
            sha1ToFiles[sha1].add(filename)

    # Find which commit deleted each file in origin.
    for filename in files_deleted_in_origin:
        log("Finding commit that deleted %s" % filename)
        sha1 = shell.output_or_error(
            ["git", "log", "-1", origin_branch, "--format=%H", "--", filename])
        sha1s.add(sha1)
        sha1ToFiles[sha1].add(filename)

    # Look up dates from sha1 hashes.  We want to output them in a canonical order
    # so that we can diff easier.  Date order seems more convenient that sha1.
    log("Looking up sha1 dates to sort them")
    sha1_to_date = {}
    for sha1 in sha1s:
        date = shell.output_or_error(
            ["git", "log", "-1", "--format=%at",
             "%s" % sha1])
        sha1_to_date[sha1] = date

    # Print the patches file.
    log("Writing patch file")
    print(
        "---------------------------------------------------------------------",
        file=output_file)
    print("-- Chromium Patches. Autogenerated by " +
          os.path.basename(__file__) + ", do not edit --",
          file=output_file)
    print(
        "---------------------------------------------------------------------",
        file=output_file)
    print("\n", file=output_file)
    wd = os.getcwd()
    for sha1, date in sorted(sha1_to_date.items(), key=lambda kv: kv[1]):
        print(
            "------------------------------------------------------------------",
            file=output_file)
        loglines = shell.output_or_error(["git", "log", "-1", "%s" % sha1])
        for line in loglines.splitlines():
            print(line.rstrip(), file=output_file)
        print("\nAffects:", file=output_file)
        # TODO(liberato): maybe add the lines that were affected.
        for file in sorted(sha1ToFiles[sha1]):
            print("    " + os.path.relpath(file, wd), file=output_file)
        print(file=output_file)

    log("Done")


if __name__ == "__main__":
    main(sys.argv)
