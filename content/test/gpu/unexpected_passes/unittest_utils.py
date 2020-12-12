# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for unittests."""

from unexpected_passes import data_types


def CreateStatsWithPassFails(passes, fails):
  stats = data_types.BuildStats()
  for _ in xrange(passes):
    stats.AddPassedBuild()
  for i in xrange(fails):
    stats.AddFailedBuild('build_id%d' % i)
  return stats
