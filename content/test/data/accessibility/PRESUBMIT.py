# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chromium accessibility-related test code.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts/
for more details about the presubmit API built into depot_tools.
"""

import os

PRESUBMIT_VERSION = '2.0.0'

_ACCESSIBILITY_EVENTS_TEST_PATH = (
    r"^content/test/data/accessibility/event/.*\.html",
)

_ACCESSIBILITY_TREE_TEST_PATH = (
    r"^content/test/data/accessibility/accname/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/aria/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/css/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/event/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
    r"^content/test/data/accessibility/html/"
      ".*-expected-(mac|win|uia-win|auralinux).txt",
)

_ACCESSIBILITY_TREE_TEST_PATH_BLINK = (
    r"^content/test/data/accessibility/accname/"
      ".*-expected-blink.txt",
    r"^content/test/data/accessibility/aria/"
      ".*-expected-blink.txt",
    r"^content/test/data/accessibility/css/"
      ".*-expected-blink.txt",
    r"^content/test/data/accessibility/event/"
      ".*-expected-blink.txt",
    r"^content/test/data/accessibility/html/"
      ".*-expected-blink.txt",
)


_ACCESSIBILITY_ANDROID_EVENTS_TEST_PATH = (
    r"^.*/WebContentsAccessibilityEventsTest\.java",
)

_ACCESSIBILITY_ANDROID_TREE_TEST_PATH = (
    r"^.*/WebContentsAccessibilityTreeTest\.java",
)

def CheckAccessibilityEventsTestsAreIncludedForAndroid(input_api, output_api):
    """Checks that commits that include a newly added, renamed/moved, or deleted
    test in the DumpAccessibilityEventsTest suite also includes a corresponding
    change to the Android test."""

    def FilePathFilter(affected_file):
        paths = _ACCESSIBILITY_EVENTS_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    def AndroidFilePathFilter(affected_file):
        paths = _ACCESSIBILITY_ANDROID_EVENTS_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    # Only consider changes in the events test data path with html type.
    if not any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=FilePathFilter)):
        return []

    # If the commit contains any change to the Android test file, ignore.
    if any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=AndroidFilePathFilter)):
        return []

    # Only consider changes that are adding/renaming or deleting a file
    message = []
    for f in input_api.AffectedFiles(include_deletes=True,
                                     file_filter=FilePathFilter):
        if f.Action() == 'A':
            message = (
                "It appears that you are adding platform expectations for a"
                "\ndump_accessibility_events* test, but have not included"
                "\na corresponding change for Android."
                "\nPlease include the test from:"
                "\n    content/public/android/javatests/src/org/chromium/"
                "content/browser/accessibility/"
                "WebContentsAccessibilityEventsTest.java"
                "\nIf this message is confusing or annoying, please contact"
                "\nmembers of ui/accessibility/OWNERS.")

    # If no message was set, return empty.
    if not len(message):
        return []

    return [output_api.PresubmitPromptWarning(message)]


def CheckAccessibilityTreeTestsAreIncludedForAndroid(input_api, output_api):
    """Checks that commits that include a newly added, renamed/moved, or deleted
    test in the DumpAccessibilityTreeTest suite also includes a corresponding
    change to the Android test."""

    def FilePathFilter(affected_file):
        paths = _ACCESSIBILITY_TREE_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    def FilePathFilterBlink(affected_file):
        paths = _ACCESSIBILITY_TREE_TEST_PATH_BLINK
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    def AndroidFilePathFilter(affected_file):
        paths = _ACCESSIBILITY_ANDROID_TREE_TEST_PATH
        return input_api.FilterSourceFile(affected_file, files_to_check=paths)

    # Only consider changes in the various tree test data paths with txt type
    # and a specific platform expectation.
    if not any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=FilePathFilter)):
        return []

    # For a meaningful change on Android, there should also be a blink change.
    if not any(
        input_api.AffectedFiles(include_deletes=True,
                                file_filter=FilePathFilterBlink)):
        return []

    # If the commit contains any change to the Android test file, ignore.
    if any(
            input_api.AffectedFiles(include_deletes=True,
                                    file_filter=AndroidFilePathFilter)):
        return []

    # Only consider changes that are adding/renaming or deleting a file
    message = []
    for f in input_api.AffectedFiles(include_deletes=True,
                                     file_filter=FilePathFilter):
        if f.Action() == 'A':
            message = (
                "It appears that you are adding platform expectations for a"
                "\ndump_accessibility_tree* test, but have not included"
                "\na corresponding change for Android."
                "\nPlease include (or remove) the test from:"
                "\n    content/public/android/javatests/src/org/chromium/"
                "content/browser/accessibility/"
                "WebContentsAccessibilityTreeTest.java"
                "\nIf this message is confusing or annoying, please contact"
                "\nmembers of ui/accessibility/OWNERS.")

    # If no message was set, return empty.
    if not len(message):
        return []

    return [output_api.PresubmitPromptWarning(message)]


def CheckAccessibilityTestExpectationFilenames(input_api, output_api):
    """Checks that commits that include any text file use the correct
    naming convention for expectation files to prevent shadow failures."""

    def FileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[r"content/test/data/accessibility/(?!(mac|win)/).+\.txt"],
        )

    valid_suffixes = [
        "-expected-android-external.txt",
        "-expected-android.txt",
        "-expected-android-assist-data.txt",
        "-expected-auralinux.txt",
        "-expected-auralinux-2.txt",
        "-expected-auralinux-xenial.txt",
        "-expected-blink.txt",
        "-expected-blink-cros.txt",
        "-expected-fuchsia.txt",
        "-expected-mac.txt",
        "-expected-mac-before-11.txt",
        "-expected-uia-win.txt",
        "-expected-win.txt",

        # TODO(accessibility) Temporary while Android API experiment is running
        "-expected-android-exp.txt",
    ]
    problems = []

    for f in input_api.AffectedFiles(file_filter=FileFilter):
      if not any(f.LocalPath().endswith(suffix) for suffix in valid_suffixes):
        problems.append(f.LocalPath())

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                "Accessibility platform expectation filenames should follow the"
                "\npattern [name]-expected-[platform].txt. Accepted formats"
                " are:\n  [name]-expected-android.txt"
                "\n  [name]-expected-android-assist-data.txt"
                "\n  [name]-expected-android-external.txt"
                "\n  [name]-expected-auralinux.txt"
                "\n  [name]-expected-blink.txt"
                "\n  [name]-expected-fuchsia.txt"
                "\n  [name]-expected-mac.txt"
                "\n  [name]-expected-uia-win.txt"
                "\n  [name]-expected-win.txt"
                "\nInvalid filenames:\n",
                problems,
            )
        ]
    return []


def CheckAccessibilityHtmlSvgPair(input_api, output_api):
    """Checks that .html and .svg files with the same base name
    are in the same directory."""

    def FileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file,
            files_to_check=[r"content/test/data/accessibility/.+\.(html|svg)"]
        )

    files_by_basename = {}
    problems = []

    for f in input_api.AffectedFiles(file_filter=FileFilter):
        path = f.LocalPath()
        basename = input_api.os_path.basename(path)
        name, ext = input_api.os_path.splitext(basename)
        files_by_basename.setdefault(name, []).append((path, ext))

    for name, file_list in files_by_basename.items():
        has_html = any(ext == ".html" for _, ext in file_list)
        has_svg = any(ext == ".svg" for _, ext in file_list)

        # Check for mismatched directories
        if has_html and has_svg and len(file_list) > 1:
            paths = [path for path, _ in file_list]
            if any(input_api.os_path.dirname(path1) !=
                   input_api.os_path.dirname(path2)
                   for path1 in paths for path2 in paths):
                # Report as a single "pair" item
                problems.append(f"pair {paths[0]} and {paths[1]}")

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                ".html and .svg files with the same base name should"
                " located in the same directory.\n"
                "Problematic pairs:\n",
                problems,
            )
        ]
    return []


def CheckAccessibilityHtmlFileTest(input_api, output_api):
    """Checks that HTML accessibility test files have corresponding test references."""

    def FileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file, files_to_check=[r"content/test/data/accessibility/.+\.(html|txt)"]
        )

    html_files = {}  # Store HTML files and their base names
    android_txt_files = {} # Store android txt files
    problems = []

    for f in input_api.AffectedFiles(file_filter=FileFilter):
        if f.LocalPath().endswith(".html"):
          html_files[input_api.os_path.basename(f.LocalPath())] = f.LocalPath()
        if f.LocalPath().endswith(".txt"):
            if "-expected-android-" in f.LocalPath():
                android_txt_files[input_api.os_path.basename(f.LocalPath())] = f.LocalPath()

    # If any Android txt files were found, check for Java file changes
    for txt_file_name, txt_file_path in android_txt_files.items():
        ################
        # txt_file_name: example-expected-android-external.txt
        # txt_file_path: content/test/data/accessibility/html/example-expected-android-external.txt
        # name:          example
        # html_path:     content/test/data/accessibility/html/example.html
        ################
        name = txt_file_name.split("-expected-android")[0]
        html_path = txt_file_path.split("-expected-android")[0] + ".html"

        java_test_suite_file = None
        readable_file_name = None

        if "/event/" in txt_file_path:
            readable_file_name = "//content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityEventsTest.java"
            java_test_suite_file = os.path.join(input_api.change.RepositoryRoot(),
                "content", "public", "android", "javatests", "src", "org", "chromium",
                "content", "browser", "accessibility", "WebContentsAccessibilityEventsTest.java")
        elif any(subdir in txt_file_path for subdir in ["/accname/", "/html/", "/aria/", "/css/"]):
            # We do not want to include tree tests with certain element that flake on Android.
            try:
                html_file_contents = input_api.ReadFile(os.path.join(input_api.change.RepositoryRoot(), html_path))
                exclusion_rules = ["<script>", "<video>", "<noscript>"]
                if any(rule in html_file_contents for rule in exclusion_rules):
                    continue
            except Exception as e:
                problems.append(f"Error reading {html_path}: {e}")

            readable_file_name = "//content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityTreeTest.java"
            java_test_suite_file = os.path.join(input_api.change.RepositoryRoot(),
                "content", "public", "android", "javatests", "src", "org", "chromium",
                "content", "browser", "accessibility", "WebContentsAccessibilityTreeTest.java")

        if java_test_suite_file is not None:
            try:
                test_contents = input_api.ReadFile(os.path.join(input_api.change.RepositoryRoot(), java_test_suite_file))
                expected_addition = f"{name}.html"
                if expected_addition not in test_contents:
                    problems.append(f"{expected_addition} (missing reference in {readable_file_name})")
            except Exception as e:
                problems.append(f"Error reading {java_test_suite_file}: {e}")

    # Check for cc changes for the remaining files
    for basename, html_path in html_files.items():
        test_file = None
        readable_file_name = None

        if "/html/frame/" in html_path or "/aria/frames/" in html_path:
            continue

        if "/accname/" in html_path or "/mathml/" in html_path:
            readable_file_name = "//content/browser/accessibility/dump_accessibility_node_browsertest.cc"
            test_file = os.path.join(input_api.change.RepositoryRoot(),
                          "content", "browser", "accessibility", "dump_accessibility_node_browsertest.cc")
        elif "/event/" in html_path:
            readable_file_name = "//content/browser/accessibility/dump_accessibility_events_browsertest.cc"
            test_file = os.path.join(input_api.change.RepositoryRoot(),
                          "content", "browser", "accessibility", "dump_accessibility_events_browsertest.cc")
        elif "/mac/" in html_path or "/win/ia2/" in html_path:
            readable_file_name = "//content/browser/accessibility/dump_accessibility_scripts_browsertest.cc"
            test_file = os.path.join(input_api.change.RepositoryRoot(),
                          "content", "browser", "accessibility", "dump_accessibility_scripts_browsertest.cc")
        else:
            readable_file_name = "//content/browser/accessibility/dump_accessibility_tree_browsertest.cc"
            test_file = os.path.join(input_api.change.RepositoryRoot(),
                          "content", "browser", "accessibility", "dump_accessibility_tree_browsertest.cc")

        try:
            test_contents = input_api.ReadFile(test_file)
            if basename not in test_contents:
                problems.append(f"{html_path} (missing reference in {readable_file_name})")
        except Exception as e:
            problems.append(f"Error reading {test_file}: {e}")

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                "All HTML accessibility test files should have a corresponding"
                "\nreference in the associated browsertest file.\n"
                "Problems found:\n",
                problems,
            )
        ]
    return []


# TODO(accessibility) Check the current directory for the html file,
# instead of just added/modified files (if possible).
def CheckAccessibilityHtmlExpectationsPair(input_api, output_api):
    """Checks that HTML accessibility test files have corresponding
    expectation files (and vice-versa)."""

    def FileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file, files_to_check=[r"content/test/data/accessibility/(?!(mac|win)/).+\.(html|txt)"]
        )

    problems = []
    html_files = {}
    txt_files = {}

    for f in input_api.AffectedFiles(file_filter=FileFilter):
        if f.Action() != 'A':
            continue
        path = f.LocalPath()
        if "/html/frame/" in path or "/aria/frames/" in path:
            continue
        basename = input_api.os_path.basename(path)
        name, ext = input_api.os_path.splitext(basename)
        if ext == ".html":
            html_files[name] = path
        elif ext == ".txt" and "-expected-" in basename:
            txt_files[name] = path

    # Check HTML files for corresponding expectations
    for name, html_path in html_files.items():
        hasMatch = False
        for key, _ in txt_files.items():
            if name in key:
                hasMatch = True
                break
        if not hasMatch:
            problems.append(f"{html_path} (missing corresponding -expected-*.txt file)")

    # Check expectation files for corresponding HTML
    for name, txt_path in txt_files.items():
        hasMatch = False
        for key, _ in html_files.items():
            if key in name:
                hasMatch = True
                break
        if not hasMatch:
            problems.append(f"{txt_path} (missing corresponding .html file)")

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                "HTML accessibility test files must have a corresponding"
                "\nexpectation file (and vice-versa). Note this may be a"
                "\nfalse positive if the html file already existed."
                "\nProblems found:\n",
                problems,
            )
        ]
    return []


def CheckFrameHtmlFilesDontHaveExpectations(input_api, output_api):
    """Checks that HTML accessibility test files that are frames to be
    used in other tests do not have expectation files."""

    def AriaFramesFileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file, files_to_check=[r"content/test/data/accessibility/aria/frames/.+\.(txt)"]
        )

    def HtmlFramesFileFilter(affected_file):
        return input_api.FilterSourceFile(
            affected_file, files_to_check=[r"content/test/data/accessibility/html/frame/.+\.(txt)"]
        )

    problems = []

    for f in input_api.AffectedFiles(file_filter=AriaFramesFileFilter):
        path = f.LocalPath()
        basename = input_api.os_path.basename(path)
        name, ext = input_api.os_path.splitext(basename)
        if ext == ".txt" and "-expected-" in basename:
            problems.append(f"{basename}")

    for f in input_api.AffectedFiles(file_filter=HtmlFramesFileFilter):
        path = f.LocalPath()
        basename = input_api.os_path.basename(path)
        name, ext = input_api.os_path.splitext(basename)
        if ext == ".txt" and "-expected-" in basename:
            problems.append(f"{basename}")

    if problems:
        return [
            output_api.PresubmitPromptWarning(
                "The /frame(s) sub-directories should only be used for html"
                "\nfiles used in other tests (e.g. iframes) that are not"
                "\ndirectly tested themselves. There should be no"
                "\nexpectation files for these."
                "\nProblems found:\n",
                problems,
            )
        ]
    return []