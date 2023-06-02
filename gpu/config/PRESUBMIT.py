# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces workaround list is alphabetically sorted.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

import difflib
import os.path
import io

def _CheckGPUWorkaroundListSorted(input_api, output_api):
    """Check: gpu_workaround_list.txt feature list sorted alphabetically.
    """
    filename = os.path.join(input_api.PresubmitLocalPath(),
                            'gpu_workaround_list.txt')

    with io.open(filename, encoding='utf-8') as f:
      workaround_list = [line.rstrip('\n') for line in f]

    workaround_list_sorted = sorted(workaround_list, key=lambda s: s.lower())
    if workaround_list == workaround_list_sorted:
        return []
    # Diff the sorted/unsorted versions.
    differ = difflib.Differ()
    diff = differ.compare(workaround_list, workaround_list_sorted)
    return [output_api.PresubmitError(
        'gpu_workaround_list.txt features must be sorted alphabetically. '
        'Diff of feature order follows:', long_text='\n'.join(diff))]

def CheckChangeOnUpload(input_api, output_api):
  return _CheckGPUWorkaroundListSorted(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CheckGPUWorkaroundListSorted(input_api, output_api)
