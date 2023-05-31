# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for content/renderer

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


import re

def _FilterFile(affected_file):
  """Return true if the file could contain code requiring a presubmit check."""
  return affected_file.LocalPath().endswith(
      ('.h', '.cc', '.cpp', '.cxx', '.mm'))

def _CheckForUseOfGlobalTaskRunnerGetter(input_api, output_api):
  """Check that base::ThreadTaskRunner::GetCurrentDefault() or
  base::SequencedTaskRunner::GetCurrentDefault() is not used."""

  problems = []
  getter_re = input_api.re.compile(
      r'(^|\b)base::(Thread|Sequenced)TaskRunner::GetCurrentDefault\(\)')
  for f in input_api.AffectedSourceFiles(_FilterFile):
    for line_number, line in f.ChangedContents():
      if getter_re.search(line):
        problems.append('%s:%d' % (f, line_number))

  if problems:
    return [output_api.PresubmitPromptWarning(
      'base::ThreadTaskRunnerHandle::GetCurrentDefault() and'
      ' base::SequencedTaskRunnerHandle::GetCurrentDefault() are deprecated in'
      ' renderer; please use RenderFrame::GetTaskRunner for production code and'
      ' blink::scheduler::Get*TaskRunnerForTesting for tests. Please reach'
      ' out to scheduler-dev@ if you have any questions.', problems)]
  return []

def _CommonCheck(input_api, output_api):
  results = []
  results.extend(_CheckForUseOfGlobalTaskRunnerGetter(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CommonCheck(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonCheck(input_api, output_api)
