# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for custom data types."""

import datetime
from typing import Any, List, Optional

from flake_suppressor_common import common_typing as ct
from unexpected_passes_common import data_types as unexpected_dt


class Expectation(unexpected_dt.Expectation):
  """Container for a test expectation.

  Re-uses the unexpected pass finder's equivalent class, but updates the
  check to work with the flake suppressor's Result class instead of the
  unexpected pass finder's.

  Does not contain information on suite/expectation file, so these are expected
  to be separated based on that.
  """

  def AppliesToResult(self, result: 'Result') -> bool:
    assert isinstance(result, Result)
    return self._comp(result.test) and self.tags <= set(result.tags)


class Result():
  """Container for a test result.

  Contains all the relevant information we get back from BigQuery for a result
  for the purposes of the flake finder.
  """

  # TODO(crbug.com/40237087): Refactor this Class to take ResultTupleType as an
  # input.
  def __init__(self,
               suite: str,
               test: str,
               tags: ct.TagTupleType,
               build_id: str,
               status: Optional[str] = None,
               date: Optional[datetime.date] = None,
               is_slow: Optional[bool] = None,
               typ_expectations: Optional[List[str]] = None):
    assert isinstance(tags, tuple), 'Tags must be in tuple form to be hashable'
    # Results should not have any globs.
    assert '*' not in test
    self.suite = suite
    self.test = test
    self.tags = tags
    self.build_id = build_id
    self.status = status or ''
    self.date = date or datetime.date.min
    self.is_slow = is_slow or False
    self.typ_expectations = typ_expectations or []
    self.typ_expectations.sort()

  def __eq__(self, other: Any) -> bool:
    return (isinstance(other, Result) and self.suite == other.suite
            and self.test == other.test and self.tags == other.tags
            and self.build_id == other.build_id and self.status == other.status
            and self.date == other.date and self.is_slow == other.is_slow
            and self.typ_expectations == other.typ_expectations)

  def __hash__(self) -> int:
    return hash((self.suite, self.test, self.tags, self.build_id, self.status,
                 self.date, self.is_slow, tuple(self.typ_expectations)))
