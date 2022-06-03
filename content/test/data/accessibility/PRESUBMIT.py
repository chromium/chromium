# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for content/test/data/accessibility*"""

PRESUBMIT_VERSION = '2.0.0'

USE_PYTHON3 = True

_ACCESSIBILITY_EVENTS_TEST_PATH = (
    r"^content[\\/]test[\\/]data[\\/]accessibility[\\/]event[\\/].*\.html",
)

_ACCESSIBILITY_ANDROID_EVENTS_TEST_PATH = (
    r"^.*[\\/]WebContentsAccessibilityEventsTest\.java",
)

def CheckAccessibilityEventsTestIncludesAndroid(input_api, output_api):
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
  if not any(input_api.AffectedFiles(include_deletes=True,
                                     file_filter=FilePathFilter)):
    return []

  # If the commit contains any change to the Android test file, ignore.
  if any(input_api.AffectedFiles(include_deletes=True,
                                 file_filter=AndroidFilePathFilter)):
    return []

  # Only consider changes that are adding/renaming or deleting a file
  message = []
  for f in input_api.AffectedFiles(include_deletes=True,
                                   file_filter=FilePathFilter):
    if f.Action()=='A' or f.Action()=='D':
      message = ("It appears that you are adding, renaming or deleting"
                 "\na dump_accessibility_events* test, but have not included"
                 "\na corresponding change for Android."
                 "\nPlease include (or remove) the test from:"
                 "\n    content/public/android/javatests/src/org/chromium/"
                 "content/browser/accessibility/"
                 "WebContentsAccessibilityEventsTest.java"
                 "\nIf this message is confusing or annoying, please contact"
                 "\nmembers of ui/accessibility/OWNERS.")

  # If no message was set, return empty.
  if not len(message):
    return []

  return [output_api.PresubmitPromptWarning(message)]
