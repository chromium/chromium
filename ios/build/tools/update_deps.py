#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Updates BUILD.gn files to add missing direct dependencies when the
transitive dependency already exists.
The script is intentionally conservative and will not work in many
scenarios (e.g it does not handle target names with variables). These
cases will require manual roll (as today).
"""

import argparse
from collections import defaultdict
import os
import re
import subprocess
import sys

# The output of gn check when some direct dependencies are missing is expected
# to contain these lines.
TRANSITIVE_PATTERNS = [
    (3, "The target:"), (5, "is including a file from the target:"),
    (8, "It's usually best to depend directly on the destination target."),
    (9, "In some cases, the destination target is considered a subcomponent"),
    (10, "of an intermediate target. In this case, the intermediate target"),
    (11, "should depend publicly on the destination to forward the ability"),
    (12, "to include headers."),
    (14, "Dependency chain (there may also be others):")
]
# The line containing the target in the error message.
TRANSITIVE_DEPENDENT_LINE_NUMBER = 4
# The line containing the dependence in the error message.
TRANSITIVE_DEPENDEE_LINE_NUMBER = 6

MISSING_PATTERNS = [
    (3, "It is not in any dependency of"),
    (5, "The include file is in the target(s):"),
]
MISSING_DEPENDENT_LINE_NUMBER = 4
MISSING_DEPENDEE_LINE_NUMBER = 6

# Character to set colors in terminal.
TERMINAL_ERROR_COLOR = "\033[91m"
TERMINAL_WARNING_COLOR = "\033[93m"
TERMINAL_RESET_COLOR = "\033[0m"

# Separator between GN errors.
GN_ERROR_SEPARATOR = "___________________\n"

# The error message to be used when the include file is in multiple GN targets.
MULTIPLE_DEPENDEES_ERROR = "Cannot handle includes in multiple targets:"
# The error message to be used when several "deps" are present for the target.
MULTIPLE_DEPS_ERROR = "Multiple deps variables in:"
# Warning message when automatically removing a target ending with
# "_strings_grit".
GRIT_TARGET_MESSAGE = "Grit target found!"
GRIT_TARGET_MESSAGE_DETAILS = "Automatically replacing:\n  %s\nby:\n  %s\n"

# Array to handle special cases for canonical public target. This avoids having
# dependencies on internal target when there is a canonical target with public
# deps on those internal targets.
CANONICAL_PUBLIC_TARGETS = {
    "//ios/chrome/app/strings:ios_strings_grit":
    "//ios/chrome/app/strings:strings",
    "//ios/chrome/app/strings:ios_branded_strings_grit":
    "//ios/chrome/app/strings:strings",
    "//components/strings:components_strings_grit":
    "//components/strings:strings",
    "//components/sessions:shared":
    "//components/sessions:sessions",
    "//base/numerics:base_numerics":
    "//base:base",
    "//third_party/abseil-cpp/absl/types:optional":
    "//base:base",
}


def gn_format(gn_files):
    """Format the file `gn_files`."""
    subprocess.check_call(["gn", "format"] + gn_files)


def remove_redundant_target_name(dependant, target_name):
    """Canonicalize target_name to be used as a dep of dependant."""
    if target_name in CANONICAL_PUBLIC_TARGETS:
        target_name = CANONICAL_PUBLIC_TARGETS[target_name]
    (dependant_folder, dependant_target) = dependant.split(":")
    (folder, target) = target_name.split(":")
    if dependant_folder == folder:
        return ":%s" % target
    last_folder = os.path.basename(folder)
    if last_folder == target:
        return folder
    if target.endswith("_strings_grit"):
        warning_message = GRIT_TARGET_MESSAGE_DETAILS % (target_name, folder)
        print_warning(GRIT_TARGET_MESSAGE, warning_message)
        return folder
    return target_name


def cleanup_redundant_deps(deps):
    """If the list of `deps` contains only targets redundant with each
    others, this method will return only one of them. Otherwise, return
    all targets.
    """
    if len(deps) != 2:
        """Currently only handle the string grit case where gn finds the
        the string header in <path>:<target>_strings and
        <path>:<target>_string_grit, so don't check if there is not
        exactly two targets.
        """
        return deps
    if (deps[1].endswith("_strings") and
        deps[0] == deps[1] + "_grit"):
        return [deps[0]]
    if (deps[0].endswith("_strings") and
        deps[1] == deps[0] + "_grit"):
        return [deps[1]]
    return deps


def extract_missing_dependency(error, prefix, patterns, dependant_line,
                             dependee_line):
    """Parse gn error message for missing direct dependency."""
    lines = error.splitlines()

    if len(lines) <= patterns[-1][0]:
        return False, None
    for line_number, pattern in patterns:
        if lines[line_number] != pattern:
            return False, None
    dependant = lines[dependant_line].strip()
    if prefix and not dependant.startswith(prefix):
        return False, None

    dependees_with_target = []
    index = dependee_line
    while lines[dependee_line].strip().startswith("//"):
        dependees_with_target.append(lines[dependee_line].strip())
        dependee_line += 1
    dependees_with_target = cleanup_redundant_deps(dependees_with_target)

    dependees = []
    for dependee in dependees_with_target:
        dependees.append(remove_redundant_target_name(dependant, dependee))

    return True, (dependant, dependees)


def get_missing_deps(builddir, prefix):
    """Extracts missing direct dependencies from gn."""
    missing_deps = defaultdict(set)
    process = subprocess.Popen(["gn", "check", builddir],
                               stdout=subprocess.PIPE)
    lines, errs = process.communicate()
    errors = lines.decode("ascii").split(GN_ERROR_SEPARATOR)
    for error in errors:
        has_missing_dep, info = extract_missing_dependency(
            error, prefix, TRANSITIVE_PATTERNS,
            TRANSITIVE_DEPENDENT_LINE_NUMBER, TRANSITIVE_DEPENDEE_LINE_NUMBER)
        if not has_missing_dep:
            has_missing_dep, info = extract_missing_dependency(
                error, prefix, MISSING_PATTERNS, MISSING_DEPENDENT_LINE_NUMBER,
                MISSING_DEPENDEE_LINE_NUMBER)
        if has_missing_dep:
            dependant, dependees = info
            if len(dependees) == 1:
                missing_deps[dependant].add(dependees[0])
            else:
                print_error(MULTIPLE_DEPENDEES_ERROR, error)
    return missing_deps


def add_missing_deps(srcdir, target, deps):
    """Adds the missing deps to the BUILD.gn file and run gn format on it."""
    (dir, target_name) = target.split(":")
    build_gn_file = os.path.join(srcdir, dir[2:], "BUILD.gn")
    content = []
    in_target = False
    changed = False
    first_deps_variable_line_index = -1
    target_name = target_name.replace("+", "\\+")

    # Handles the internal targets from ios_app_bundle and
    # ios_framework_bundle templates.
    for suffix in ('_executable', '_shared_library'):
        if target_name.endswith(suffix):
            target_name = target_name[:-len(suffix)]
            break

    target_rule = re.compile(r"\s*[a-z_]*\(\"%s\"\) {" % target_name)
    with open(build_gn_file, "r") as build_gn:
        all_lines = build_gn.readlines()
        for line_index, line in enumerate(all_lines):
            content += [line]
            if target_rule.search(line):
                indent = len(line) - len(line.lstrip(" "))
                in_target = True
            if in_target and line == (" " * indent + "}\n"):
                in_target = False
            if (in_target and first_deps_variable_line_index != -1 and
                     line.strip().startswith("deps ")):
                error_detail = (f"At lines:\n"
                  f"* {build_gn_file}:{first_deps_variable_line_index}:\n"
                  f"  {all_lines[first_deps_variable_line_index]}\n"
                  f"* {build_gn_file}:{line_index}:\n"
                  f"  {line}\n")
                error_detail = f"{target}\n{error_detail}"
                print_error(MULTIPLE_DEPS_ERROR, error_detail)
                # Multiple deps, abort
                return False, None
            if (in_target and first_deps_variable_line_index == -1 and
                    line.strip().startswith("deps ")):
                first_deps_variable_line_index = line_index
                onelinedeps = False
                if "[" in line and line[-2] == "]":
                    onelinedeps = True
                    content[-1] = line.replace("]", ",")
                for dep in deps:
                    content += ["\"%s\",\n" % dep]
                    changed = True
                if onelinedeps:
                    content += ["]\n"]
    if changed:
        with open(build_gn_file, "w") as build_gn:
            build_gn.write("".join(content))
        return True, build_gn_file
    return False, None


def main(args):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("--prefix",
                        help="Only fix subtargets of prefix",
                        default="")
    parser.add_argument("builddir", help="The build dir")
    parser.add_argument("srcdir", help="The src dir")
    args = parser.parse_args()
    deps = get_missing_deps(args.builddir, args.prefix)
    changed_build_gn_files = []
    for target in deps:
        changed, build_gn_file = add_missing_deps(args.srcdir, target,
                                                deps[target])
        if changed:
            changed_build_gn_files.append(build_gn_file)
    if changed_build_gn_files:
        gn_format(changed_build_gn_files)


def print_error(error_message, error_info):
    """ Print the `error_message` with additional `error_info` """
    color_start, color_end = adapted_color_for_output(TERMINAL_ERROR_COLOR,
                                                   TERMINAL_RESET_COLOR)

    error_message = color_start + "ERROR: " + error_message + color_end
    if len(error_info) > 0:
        error_message = error_message + "\n" + error_info
    print(error_message + "\n" + GN_ERROR_SEPARATOR)


def print_warning(warning_message, warning_info):
    """ Print the `warning_message` with additional `warning_info` """
    color_start, color_end = adapted_color_for_output(TERMINAL_WARNING_COLOR,
                                                   TERMINAL_RESET_COLOR)

    warning_message = color_start + "WARNING: " + warning_message + color_end
    if len(warning_info) > 0:
        warning_message = warning_message + "\n" + warning_info
    print(warning_message + "\n" + GN_ERROR_SEPARATOR)


def adapted_color_for_output(color_start, color_end):
    """ Returns a the `color_start`, `color_end` tuple if the output is a
    terminal, or empty strings otherwise """
    if not sys.stdout.isatty():
        return "", ""
    return color_start, color_end


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
