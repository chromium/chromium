# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import typing

from blinkpy.web_tests.stale_expectation_removal import queries
from unexpected_passes_common import unittest_utils as uu


def CreateGenericWebTestQuerier(*args,
                                **kwargs) -> queries.WebTestBigQueryQuerier:
    return typing.cast(
        queries.WebTestBigQueryQuerier,
        uu.CreateGenericQuerier(cls=queries.WebTestBigQueryQuerier,
                                *args,
                                **kwargs))
