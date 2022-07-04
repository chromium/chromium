# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for unittests."""

from __future__ import print_function

import typing
import unittest.mock as mock

from unexpected_passes_common import builders
from unexpected_passes_common import expectations
from unexpected_passes_common import data_types
from unexpected_passes_common import queries as queries_module

# pylint: disable=useless-object-inheritance,super-with-arguments


def CreateStatsWithPassFails(passes: int, fails: int) -> data_types.BuildStats:
  stats = data_types.BuildStats()
  for _ in range(passes):
    stats.AddPassedBuild()
  for i in range(fails):
    stats.AddFailedBuild('build_id%d' % i)
  return stats


def _CreateSimpleQueries(clauses: typing.Iterable[str]) -> typing.List[str]:
  queries = []
  # Not actually a valid query since we don't specify the table, but it works.
  for c in clauses:
    queries.append("""\
SELECT *
WHERE %s
""" % c)
  return queries


class SimpleFixedQueryGenerator(queries_module.FixedQueryGenerator):
  def GetQueries(self) -> typing.List[str]:
    return _CreateSimpleQueries(self.GetClauses())


class SimpleSplitQueryGenerator(queries_module.SplitQueryGenerator):
  def GetQueries(self) -> typing.List[str]:
    return _CreateSimpleQueries(self.GetClauses())


class SimpleBigQueryQuerier(queries_module.BigQueryQuerier):
  def _GetQueryGeneratorForBuilder(self, builder: data_types.BuilderEntry
                                   ) -> queries_module.BaseQueryGenerator:
    if not self._large_query_mode:
      return SimpleFixedQueryGenerator(builder, 'AND True')
    return SimpleSplitQueryGenerator(builder, ['test_id'], 200)

  def _GetRelevantExpectationFilesForQueryResult(self, _) -> None:
    return None

  def _StripPrefixFromTestId(self, test_id: str) -> str:
    return test_id.split('.')[-1]

  def _GetActiveBuilderQuery(self, _, __) -> str:
    return ''


def CreateGenericQuerier(
    suite: typing.Optional[str] = None,
    project: typing.Optional[str] = None,
    num_samples: typing.Optional[int] = None,
    large_query_mode: typing.Optional[bool] = None,
    cls: typing.Optional[typing.Type[queries_module.BigQueryQuerier]] = None
) -> queries_module.BigQueryQuerier:
  suite = suite or 'pixel'
  project = project or 'project'
  num_samples = num_samples or 5
  large_query_mode = large_query_mode or False
  cls = cls or SimpleBigQueryQuerier
  return cls(suite, project, num_samples, large_query_mode)


def GetArgsForMockCall(call_args_list: typing.List[tuple],
                       call_number: int) -> typing.Tuple[tuple, dict]:
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

  def map(self, f: typing.Callable[[typing.Any], typing.Any],
          inputs: typing.Iterable[typing.Any]) -> typing.List[typing.Any]:
    retval = []
    for i in inputs:
      retval.append(f(i))
    return retval

  def apipe(self, f: typing.Callable[[typing.Any], typing.Any],
            inputs: typing.Iterable[typing.Any]) -> 'FakeAsyncResult':
    return FakeAsyncResult(f(inputs))


class FakeAsyncResult(object):
  """A fake AsyncResult like the one from multiprocessing or pathos."""

  def __init__(self, result: typing.Any):
    self._result = result

  def ready(self) -> bool:
    return True

  def get(self) -> typing.Any:
    return self._result


class FakeProcess(object):
  """A fake subprocess Process object."""

  def __init__(self,
               returncode: typing.Optional[int] = None,
               stdout: typing.Optional[str] = None,
               stderr: typing.Optional[str] = None,
               finish: bool = True):
    if finish:
      self.returncode = returncode or 0
    else:
      self.returncode = None
    self.stdout = stdout or ''
    self.stderr = stderr or ''
    self.finish = finish

  def communicate(self, _) -> typing.Tuple[str, str]:
    return self.stdout, self.stderr

  def terminate(self) -> None:
    if self.finish:
      raise OSError('Tried to terminate a finished process')


class GenericBuilders(builders.Builders):
  #pylint: disable=useless-super-delegation
  def __init__(self,
               suite: typing.Optional[str] = None,
               include_internal_builders: bool = False):
    super(GenericBuilders, self).__init__(suite, include_internal_builders)
  #pylint: enable=useless-super-delegation

  def _BuilderRunsTestOfInterest(self, _test_map) -> bool:
    return True

  def GetIsolateNames(self) -> dict:
    return {}

  def GetFakeCiBuilders(self) -> dict:
    return {}

  def GetNonChromiumBuilders(self) -> dict:
    return {}


def RegisterGenericBuildersImplementation() -> None:
  builders.RegisterInstance(GenericBuilders())


class GenericExpectations(expectations.Expectations):
  def GetExpectationFilepaths(self) -> list:
    return []

  def _GetExpectationFileTagHeader(self, _) -> str:
    return """\
# tags: [ linux mac win ]
# results: [ Failure RetryOnFailure Skip Pass ]
"""


def CreateGenericExpectations() -> GenericExpectations:
  return GenericExpectations()
