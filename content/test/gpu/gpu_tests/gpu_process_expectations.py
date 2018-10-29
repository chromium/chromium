# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class GpuProcessExpectations(GpuTestExpectations):
  def SetExpectations(self):
    self.Fail('GpuProcess_video', ['linux'], bug=257109)

    # Seems to have become flaky on Windows recently.
    self.Flaky('GpuProcess_one_extra_workaround', ['win'], bug=700522)
