# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from typing import List, Tuple, Iterable

from pyfakefs import fake_filesystem_unittest

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import expectations as expectations_module
from flake_suppressor_common import queries
from flake_suppressor_common import results as results_module
from flake_suppressor_common import tag_utils


CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
RELATIVE_EXPECTATION_FILE_DIRECTORY = os.path.join('content', 'test', 'gpu',
                                                   'gpu_tests',
                                                   'test_expectations')
ABSOLUTE_EXPECTATION_FILE_DIRECTORY = os.path.join(
    CHROMIUM_SRC_DIR, RELATIVE_EXPECTATION_FILE_DIRECTORY)

TAG_HEADER = """\
# OS
# tags: [ android android-lollipop android-marshmallow android-nougat
#             android-pie android-r android-s android-t
#         chromeos
#         fuchsia
#         linux ubuntu
#         mac highsierra mojave catalina bigsur monterey
#         win win8 win10 ]
# Browser
# tags: [ android-chromium android-webview-instrumentation
#         debug debug-x64
#         release release-x64
#         fuchsia-chrome web-engine-shell ]
# results: [ Failure RetryOnFailure Pass Skip Slow ]
"""


def CreateFile(test: fake_filesystem_unittest.TestCase, *args,
               **kwargs) -> None:
  # TODO(crbug.com/40160566): Remove this and just use fs.create_file() when
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

  def GetFailingBuildCulpritFromCiQuery(self) -> str:
    raise NotImplementedError()


class UnitTestResultProcessor(results_module.ResultProcessor):
  def GetTestSuiteAndNameFromResultDbName(self, result_db_name: str
                                          ) -> Tuple[str, str]:
    _, suite, __, test_name = result_db_name.split('.', 3)
    return suite, test_name


class UnitTestTagUtils(tag_utils.BaseTagUtils):
  def RemoveIgnoredTags(self, tags: Iterable[str]) -> ct.TagTupleType:
    tags = list(set(tags) - set(['win-laptop']))
    tags.sort()
    return tuple(tags)


# pylint: disable=unused-argument
class UnitTestExpectationProcessor(expectations_module.ExpectationProcessor):
  def GetExpectationFileForSuite(self, suite: str,
                                 typ_tags: ct.TagTupleType) -> str:
    filename = suite.replace('integration_test', 'expectations.txt')
    return os.path.join(ABSOLUTE_EXPECTATION_FILE_DIRECTORY, filename)

  def IsSuiteUnsupported(self, suite) -> bool:
    return False

  def GetExpectedResult(self, fraction: float, flaky_threshold: float) -> str:
    if fraction < flaky_threshold:
      return 'RetryOnFailure'
    return 'Failure'

  def ListLocalCheckoutExpectationFiles(self) -> List[str]:
    raise NotImplementedError()

  def ListOriginExpectationFiles(self) -> List[str]:
    raise NotImplementedError()

# pylint: enable=unused-argument
