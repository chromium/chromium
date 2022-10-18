# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from typing import List, Optional

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import expectations as expectations_module

from blinkpy.common.path_finder import get_chromium_src_dir

CHROMIUM_SRC_DIR = get_chromium_src_dir()
RELATIVE_EXPECTATION_FILE_DIRECTORY = os.path.join('third_party', 'blink',
                                                   'web_tests')
ABSOLUTE_EXPECTATION_FILE_DIRECTORY = os.path.join(
    CHROMIUM_SRC_DIR, RELATIVE_EXPECTATION_FILE_DIRECTORY)

UNSUPPORTED_SUITES = {
    # TODO(crbug/1358735): For web_tests this will be populated when we
    # expand the query to all. At this time we query for regular
    # blink_web_tests and blink_wpt_tests.
}


class WebTestsExpectationProcessor(expectations_module.ExpectationProcessor):
    def IsSuiteUnsupported(self, suite: str) -> bool:
        return suite in UNSUPPORTED_SUITES

    def ListOriginExpectationFiles(self) -> List[str]:
        origin_dir_posix = RELATIVE_EXPECTATION_FILE_DIRECTORY.replace(
            os.sep, '/')
        files = self.ListGitilesDirectory(origin_dir_posix)

        efs = []
        for f in (f for f in files if f == 'TestExpectations'):
            origin_file_path = os.path.join(
                RELATIVE_EXPECTATION_FILE_DIRECTORY, f)
            efs.append(origin_file_path)

        return efs

    def ListLocalCheckoutExpectationFiles(self) -> List[str]:
        files = os.listdir(ABSOLUTE_EXPECTATION_FILE_DIRECTORY)
        efs = []
        for f in (f for f in files if f == 'TestExpectations'):
            efs.append(os.path.join(RELATIVE_EXPECTATION_FILE_DIRECTORY, f))
        return efs

    def GetExpectationFileForSuite(self,
                                   suite: str,
                                   typ_tags: ct.TagTupleType = ()) -> str:
        # TODO(crbug.com/1358735): Handling only TestExpectations in the
        # beginning. This should handle flag_specific expectations in future.
        expectation_file = 'TestExpectations'
        expectation_file = os.path.join(ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                        expectation_file)
        return expectation_file

    def GetTagGroups(self, contents: str) -> List[List[str]]:
        tag_groups = super().GetTagGroups(contents)
        tag_groups_lower = [[t.lower() for t in tg] for tg in tag_groups]
        return tag_groups_lower

    def GetExpectedResult(self, fraction: float,
                          flaky_threshold: float) -> Optional[str]:
        # TODO(crbug.com/1358735):  For web_tests, need to handle other
        # types of results like CRASH and TIMEOUT.
        if fraction < flaky_threshold:
            return None
        return 'Failure'

    def ProcessTypTagsBeforeWriting(self, typ_tags: ct.TagTupleType
                                    ) -> ct.TagTupleType:
        return tuple([tag.capitalize() for tag in typ_tags])
