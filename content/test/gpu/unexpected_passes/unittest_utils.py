# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for unittests."""

from unexpected_passes import data_types
from unexpected_passes import queries


def CreateStatsWithPassFails(passes, fails):
  stats = data_types.BuildStats()
  for _ in xrange(passes):
    stats.AddPassedBuild()
  for i in xrange(fails):
    stats.AddFailedBuild('build_id%d' % i)
  return stats


def CreateGenericQuerier(suite=None,
                         project=None,
                         num_samples=None,
                         large_query_mode=None):
  suite = suite or 'pixel'
  project = project or 'project'
  num_samples = num_samples or 5
  large_query_mode = large_query_mode or False
  return queries.BigQueryQuerier(suite, project, num_samples, large_query_mode)


class FakePool(object):
  """A fake pathos.pools.ProcessPool instance.

  Real pools don't like being given MagicMocks, so this allows testing of
  code that uses pathos.pools.ProcessPool by returning this from
  multiprocessing_utils.GetProcessPool().
  """

  def map(self, f, inputs):
    retval = []
    for i in inputs:
      retval.append(f(i))
    return retval

  def apipe(self, f, inputs):
    return FakeAsyncResult(f(inputs))


class FakeAsyncResult(object):
  """A fake AsyncResult like the one from multiprocessing or pathos."""

  def __init__(self, result):
    self._result = result

  def ready(self):
    return True

  def get(self):
    return self._result


class FakeProcess(object):
  """A fake subprocess Process object."""

  def __init__(self, returncode=None, stdout=None, stderr=None):
    self.returncode = returncode or 0
    self.stdout = stdout or ''
    self.stderr = stderr or ''

  def communicate(self, _):
    return self.stdout, self.stderr
