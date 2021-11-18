# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' expectations module."""

from __future__ import print_function

import os

import validate_tag_consistency

from unexpected_passes_common import expectations

EXPECTATIONS_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', 'gpu_tests',
                 'test_expectations'))


class GpuExpectations(expectations.Expectations):
  def GetExpectationFilepaths(self):
    filepaths = []
    for f in os.listdir(EXPECTATIONS_DIR):
      if f.endswith('_expectations.txt'):
        filepaths.append(os.path.join(EXPECTATIONS_DIR, f))
    return filepaths

  def _GetExpectationFileTagHeader(self, _):
    return validate_tag_consistency.TAG_HEADER
