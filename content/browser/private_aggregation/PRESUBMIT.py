# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for the content/browser/private_aggregation directory.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""


def CheckAggregationBudgetStorageSchemaModification(input_api, output_api):
  """ Checks that kCurrentSchemaVersion is modified when necessary.

  Whenever private_aggregation_budget_storage.cc changes and
  kCurrentSchemaVersion stays intact, this check returns a presubmit warning to
  make sure the value is updated if necessary.
  """

  database_file_changed = False
  database_version_changed = False

  for affected_file in input_api.AffectedFiles():
    basename = input_api.basename(affected_file.LocalPath())

    if basename == 'private_aggregation_budget_storage.cc':
      database_file_changed = True
      for (_, line) in affected_file.ChangedContents():
        if 'constexpr int kCurrentSchemaVersion ' in line:
          database_version_changed = True
          break

  out = []
  if database_file_changed and not database_version_changed:
    out.append(output_api.PresubmitPromptWarning(
        'Please make sure that the private aggregation api budget database is '
        'properly versioned and migrated when making changes to schema or table'
        ' contents. kCurrentSchemaVersion in '
        'private_aggregation_budget_storage.cc must be updated when doing a '
        'migration.' ))
  return out

def CheckChangeOnUpload(input_api, output_api):
  return CheckAggregationBudgetStorageSchemaModification(input_api, output_api)
