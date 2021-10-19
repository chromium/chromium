# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom data types for the web test stale expectation remover."""

import fnmatch

from unexpected_passes_common import data_types

VIRTUAL_PREFIX = 'virtual/'


class WebTestExpectation(data_types.BaseExpectation):
    """Web test-specific container for a test expectation.

    Identical to the base implementation except it can properly handle the case
    of virtual tests falling back to non-virtual expectations.
    """

    def _CompareWildcard(self, result_test_name):
        success = super(WebTestExpectation,
                        self)._CompareWildcard(result_test_name)
        if not success and result_test_name.startswith(VIRTUAL_PREFIX):
            result_test_name = _StripOffVirtualPrefix(result_test_name)
            success = fnmatch.fnmatch(result_test_name, self.test)
        return success

    def _CompareNonWildcard(self, result_test_name):
        success = super(WebTestExpectation,
                        self)._CompareNonWildcard(result_test_name)
        if not success and result_test_name.startswith(VIRTUAL_PREFIX):
            result_test_name = _StripOffVirtualPrefix(result_test_name)
            success = result_test_name == self.test
        return success


def _StripOffVirtualPrefix(test_name):
    # Strip off the leading `virtual/virtual_identifier/`.
    return test_name.split('/', 2)[-1]
