# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for GPU unittests."""

from __future__ import print_function

import typing

from unexpected_passes import gpu_queries
from unexpected_passes_common import unittest_utils as uu


def CreateGenericGpuQuerier(*args, **kwargs) -> gpu_queries.GpuBigQueryQuerier:
  return typing.cast(
      gpu_queries.GpuBigQueryQuerier,
      uu.CreateGenericQuerier(cls=gpu_queries.GpuBigQueryQuerier,
                              *args,
                              **kwargs))
