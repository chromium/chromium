# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import typing
from typing import Iterable

from blinkpy.web_tests.stale_expectation_removal import queries
from unexpected_passes_common import queries as common_queries
from unexpected_passes_common import unittest_utils as uu


# id_ is used instead of id since id is a python built-in.
def FakeQueryResult(builder_name: str, id_: str, test_id: str, status: str,
                    typ_tags: Iterable[str], step_name: str, duration: str,
                    timeout: str) -> common_queries.QueryResult:
    return common_queries.QueryResult(
        data={
            'builder_name': builder_name,
            'id': id_,
            'test_id': test_id,
            'status': status,
            'typ_tags': list(typ_tags),
            'step_name': step_name,
            'duration': duration,
            'timeout': timeout,
        })


def CreateGenericWebTestQuerier(*args,
                                **kwargs) -> queries.WebTestBigQueryQuerier:
    return typing.cast(
        queries.WebTestBigQueryQuerier,
        uu.CreateGenericQuerier(cls=queries.WebTestBigQueryQuerier,
                                *args,
                                **kwargs))
