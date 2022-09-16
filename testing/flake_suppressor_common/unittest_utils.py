# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pyfakefs import fake_filesystem_unittest  # pylint: disable=import-error
from typing import Tuple, Iterable

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import queries
from flake_suppressor_common import results as results_module
from flake_suppressor_common import tag_utils


def CreateFile(test: fake_filesystem_unittest.TestCase, *args,
               **kwargs) -> None:
  # TODO(crbug.com/1156806): Remove this and just use fs.create_file() when
  # Catapult is updated to a newer version of pyfakefs that is compatible with
  # Chromium's version.
  if hasattr(test.fs, 'create_file'):
    test.fs.create_file(*args, **kwargs)
  else:
    test.fs.CreateFile(*args, **kwargs)


class FakeProcess():
  def __init__(self, stdout: str):
    self.stdout = stdout or ''


class UnitTest_BigQueryQuerier(queries.BigQueryQuerier):
  def GetResultCountCIQuery(self) -> str:
    return """SELECT * FROM foo"""

  def GetResultCountTryQuery(self) -> str:
    return """submitted_builds SELECT * FROM bar"""

  def GetFlakyOrFailingCiQuery(self) -> str:
    return """SELECT * FROM foo"""

  def GetFlakyOrFailingTryQuery(self) -> str:
    return """submitted_builds SELECT * FROM bar"""


class UnitTestResultProcessor(results_module.ResultProcessor):
  def GetTestSuiteAndNameFromResultDbName(self, result_db_name: str
                                          ) -> Tuple[str, str]:
    _, suite, __, test_name = result_db_name.split('.', 3)
    return suite, test_name


class UnitTestTagUtils(tag_utils.BaseTagUtils):
  def RemoveMostIgnoredTags(self, tags: Iterable[str]) -> ct.TagTupleType:
    tags = list(set(tags) - set(['win-laptop']))
    tags.sort()
    return tuple(tags)
