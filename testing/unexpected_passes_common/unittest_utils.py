# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for unittests."""

from __future__ import print_function

from unexpected_passes_common import data_types
from unexpected_passes_common import queries


def CreateStatsWithPassFails(passes, fails):
  stats = data_types.BuildStats()
  for _ in range(passes):
    stats.AddPassedBuild()
  for i in range(fails):
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


def GetArgsForMockCall(call_args_list, call_number):
  """Helper to more sanely get call args from a mocked method.

  Args:
    call_args_list: The call_args_list member from the mock in question.
    call_number: The call number to pull args from, starting at 0 for the first
        call to the method.

  Returns:
    A tuple (args, kwargs). |args| is a list of arguments passed to the method.
    |kwargs| is a dict containing the keyword arguments padded to the method.
  """
  args = call_args_list[call_number][0]
  kwargs = call_args_list[call_number][1]
  return args, kwargs


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

  def __init__(self, returncode=None, stdout=None, stderr=None, finish=True):
    if finish:
      self.returncode = returncode or 0
    else:
      self.returncode = None
    self.stdout = stdout or ''
    self.stderr = stderr or ''
    self.finish = finish

  def communicate(self, _):
    return self.stdout, self.stderr

  def terminate(self):
    if self.finish:
      raise OSError('Tried to terminate a finished process')
