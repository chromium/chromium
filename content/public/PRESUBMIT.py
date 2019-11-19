# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Content public presubmit script

See https://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

def _CheckConstInterfaces(input_api, output_api):
  # Matches 'virtual...const = 0;', 'virtual...const;' or 'virtual...const {}'
  pattern = input_api.re.compile(r'virtual[^;]*const\s*(=\s*0)?\s*({}|;)',
                                 input_api.re.MULTILINE)

  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if not f.LocalPath().endswith('.h'):
      continue

    contents = input_api.ReadFile(f)
    if pattern.search(contents):
      files.append(f)

  if len(files):
      return [output_api.PresubmitError(
          'Do not add const to content/public '
          'interfaces. See '
          'https://www.chromium.org/developers/content-module/content-api',
          files) ]

  return []

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CheckConstInterfaces(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CheckConstInterfaces(input_api, output_api))
  return results
