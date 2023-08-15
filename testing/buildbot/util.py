# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def get_dimension_sets(test):
  swarming = test.get('swarming', {})
  if 'dimension_sets' in swarming:
    return swarming['dimension_sets']
  if 'dimensions' in swarming:
    return [swarming['dimensions']]
  return []
