#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import unittest

from unexpected_passes import gpu_expectations


class GetExpectationFilepathsUnittest(unittest.TestCase):
  def testGetExpectationFilepathsFindsSomething(self):
    """Tests that the _GetExpectationFilepaths finds something in the dir."""
    expectations = gpu_expectations.GpuExpectations()
    self.assertTrue(len(expectations.GetExpectationFilepaths()) > 0)


if __name__ == '__main__':
  unittest.main(verbosity=2)
