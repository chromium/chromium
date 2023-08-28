# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for unittests."""

from __future__ import print_function

from typing import Any, Callable, Iterable, List, Optional, Set, Tuple, Type
import unittest.mock as mock

from unexpected_passes_common import builders
from unexpected_passes_common import expectations
from unexpected_passes_common import data_types
from unexpected_passes_common import queries as queries_module


def CreateStatsWithPassFails(passes: int, fails: int) -> data_types.BuildStats:
  stats = data_types.BuildStats()
  for _ in range(passes):
    stats.AddPassedBuild(frozenset())
  for i in range(fails):
    stats.AddFailedBuild('build_id%d' % i, frozenset())
  return stats


def _CreateSimpleQueries(clauses: Iterable[str]) -> List[str]:
  queries = []
  # Not actually a valid query since we don't specify the table, but it works.
  for c in clauses:
    queries.append("""\
SELECT *
WHERE %s
""" % c)
  return queries


class SimpleFixedQueryGenerator(queries_module.FixedQueryGenerator):
  def GetQueries(self) -> List[str]:
    return _CreateSimpleQueries(self.GetClauses())


class SimpleSplitQueryGenerator(queries_module.SplitQueryGenerator):
  def GetQueries(self) -> List[str]:
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
    suite: Optional[str] = None,
    project: Optional[str] = None,
    num_samples: Optional[int] = None,
    large_query_mode: Optional[bool] = None,
    num_jobs: Optional[int] = None,
    use_batching: bool = True,
    cls: Optional[Type[queries_module.BigQueryQuerier]] = None
) -> queries_module.BigQueryQuerier:
  suite = suite or 'pixel'
  project = project or 'project'
  num_samples = num_samples or 5
  large_query_mode = large_query_mode or False
  cls = cls or SimpleBigQueryQuerier
  return cls(suite, project, num_samples, large_query_mode, num_jobs,
             use_batching)


def GetArgsForMockCall(call_args_list: List[tuple],
                       call_number: int) -> Tuple[tuple, dict]:
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


class FakePool():
  """A fake pathos.pools.ProcessPool instance.

  Real pools don't like being given MagicMocks, so this allows testing of
  code that uses pathos.pools.ProcessPool by returning this from
  multiprocessing_utils.GetProcessPool().
  """

  def map(self, f: Callable[[Any], Any], inputs: Iterable[Any]) -> List[Any]:
    retval = []
    for i in inputs:
      retval.append(f(i))
    return retval

  def apipe(self, f: Callable[[Any], Any],
            inputs: Iterable[Any]) -> 'FakeAsyncResult':
    return FakeAsyncResult(f(inputs))

  def close(self) -> None:
    pass

  def join(self) -> None:
    pass


class FakeAsyncResult():
  """A fake AsyncResult like the one from multiprocessing or pathos."""

  def __init__(self, result: Any):
    self._result = result

  def ready(self) -> bool:
    return True

  def get(self) -> Any:
    return self._result


class FakeProcess():
  """A fake subprocess Process object."""

  def __init__(self,
               returncode: Optional[int] = None,
               stdout: Optional[str] = None,
               stderr: Optional[str] = None,
               finish: bool = True):
    if finish:
      self.returncode = returncode or 0
    else:
      self.returncode = None
    self.stdout = stdout or ''
    self.stderr = stderr or ''
    self.finish = finish

  def communicate(self, _) -> Tuple[str, str]:
    return self.stdout, self.stderr

  def terminate(self) -> None:
    if self.finish:
      raise OSError('Tried to terminate a finished process')


class GenericBuilders(builders.Builders):
  #pylint: disable=useless-super-delegation
  def __init__(self,
               suite: Optional[str] = None,
               include_internal_builders: bool = False):
    super().__init__(suite, include_internal_builders)
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
# tags: [ amd intel nvidia ]
# results: [ Failure RetryOnFailure Skip Pass ]
"""

  def _GetKnownTags(self) -> Set[str]:
    return set(['linux', 'mac', 'win', 'amd', 'intel', 'nvidia'])


def CreateGenericExpectations() -> GenericExpectations:
  return GenericExpectations()


def RegisterGenericExpectationsImplementation() -> None:
  expectations.RegisterInstance(CreateGenericExpectations())
