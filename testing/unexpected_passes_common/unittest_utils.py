# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper methods for unittests."""

from typing import Generator, Iterable, List, Optional, Set, Tuple, Type

import pandas

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


# id_ is used instead of id since id is a python built-in.
def FakeQueryResult(builder_name: str, id_: str, test_id: str, status: str,
                    typ_tags: Iterable[str], step_name: str) -> pandas.Series:
  return pandas.Series(
      data={
          'builder_name': builder_name,
          'id': id_,
          'test_id': test_id,
          'status': status,
          'typ_tags': list(typ_tags),
          'step_name': step_name,
      })


class SimpleBigQueryQuerier(queries_module.BigQueryQuerier):

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self.query_results = []

  def _GetSeriesForQuery(self, _) -> Generator[pandas.Series, None, None]:
    for r in self.query_results:
      yield r

  def _GetRelevantExpectationFilesForQueryResult(self, _) -> None:
    return None

  def _StripPrefixFromTestId(self, test_id: str) -> str:
    return test_id.split('.')[-1]

  def _GetPublicCiQuery(self) -> str:
    return 'public_ci'

  def _GetInternalCiQuery(self) -> str:
    return 'internal_ci'

  def _GetPublicTryQuery(self) -> str:
    return 'public_try'

  def _GetInternalTryQuery(self) -> str:
    return 'internal_try'


def CreateGenericQuerier(
    suite: Optional[str] = None,
    project: Optional[str] = None,
    num_samples: Optional[int] = None,
    keep_unmatched_results: bool = False,
    cls: Optional[Type[queries_module.BigQueryQuerier]] = None
) -> queries_module.BigQueryQuerier:
  suite = suite or 'pixel'
  project = project or 'project'
  num_samples = num_samples or 5
  cls = cls or SimpleBigQueryQuerier
  return cls(suite, project, num_samples, keep_unmatched_results)


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


class GenericBuilders(builders.Builders):
  #pylint: disable=useless-super-delegation
  def __init__(self,
               suite: Optional[str] = None,
               include_internal_builders: bool = False):
    super().__init__(suite, include_internal_builders)
  #pylint: enable=useless-super-delegation

  def _BuilderRunsTestOfInterest(self, _test_map) -> bool:
    return True

  def GetIsolateNames(self) -> Set[str]:
    return set()

  def GetFakeCiBuilders(self) -> dict:
    return {}

  def GetNonChromiumBuilders(self) -> Set[data_types.BuilderEntry]:
    return set()


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
