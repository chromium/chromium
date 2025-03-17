# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Singleton registries for sharing repeated data.

Since large amount of string-based data is repeated, storing references to
shared strings results in large memory savings compared to storing the actual
strings in each object.
"""

from typing import Any, FrozenSet


class _Registry:

  def __init__(self):
    self._values_by_id = []
    self._id_by_values = {}

  def Register(self, value: Any) -> int:
    existing_id = self._id_by_values.get(value)
    if existing_id is not None:
      return existing_id

    new_id = len(self._values_by_id)
    self._values_by_id.append(value)
    self._id_by_values[value] = new_id
    return new_id

  def GetValueForId(self, identifier: int) -> Any:
    return self._values_by_id[identifier]


_test_name_registry = _Registry()
_typ_tag_registry = _Registry()
_actual_result_registry = _Registry()
_expected_result_registry = _Registry()
_bug_registry = _Registry()
_step_registry = _Registry()


def RegisterTestName(test_name: str) -> int:
  return _test_name_registry.Register(test_name)


def RetrieveTestName(identifier: int) -> str:
  return _test_name_registry.GetValueForId(identifier)


def RegisterTagSet(tag_set: FrozenSet[str]) -> int:
  return _typ_tag_registry.Register(tag_set)


def RetrieveTagSet(identifier: int) -> FrozenSet[str]:
  return _typ_tag_registry.GetValueForId(identifier)


def RegisterActualResult(result: str) -> int:
  return _actual_result_registry.Register(result)


def RetrieveActualResult(identifier: int) -> str:
  return _actual_result_registry.GetValueForId(identifier)


def RegisterExpectedResults(results: FrozenSet[str]) -> int:
  return _expected_result_registry.Register(results)


def RetrieveExpectedResults(identifier: int) -> FrozenSet[str]:
  return _expected_result_registry.GetValueForId(identifier)


def RegisterBug(bug: str) -> int:
  return _bug_registry.Register(bug)


def RetrieveBug(identifier: int) -> str:
  return _bug_registry.GetValueForId(identifier)


def RegisterStep(step: str) -> int:
  return _step_registry.Register(step)


def RetrieveStep(identifier: int) -> str:
  return _step_registry.GetValueForId(identifier)
