# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for GPU unittests."""

from __future__ import print_function

from unexpected_passes import gpu_queries
from unexpected_passes_common import unittest_utils as uu


def CreateGenericGpuQuerier(*args, **kwargs):
  return uu.CreateGenericQuerier(cls=gpu_queries.GpuBigQueryQuerier,
                                 *args,
                                 **kwargs)
