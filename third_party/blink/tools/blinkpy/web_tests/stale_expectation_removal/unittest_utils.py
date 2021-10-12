# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.web_tests.stale_expectation_removal import queries
from unexpected_passes_common import unittest_utils as uu


def CreateGenericWebTestQuerier(*args, **kwargs):
    return uu.CreateGenericQuerier(cls=queries.WebTestBigQueryQuerier,
                                   *args,
                                   **kwargs)
