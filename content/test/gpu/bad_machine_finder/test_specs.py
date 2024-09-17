# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for interacting with the test spec files in //testing/buildbot."""

import ast
import collections
import copy
import os
from typing import Any, Dict, Generator, List, Tuple

import gpu_path_util


class DimensionSet:
  """Represents a set of Swarming dimensions pulled from the test spec files.

  Handles the Swarming OR operator (|) and guarantees a consistent order when
  iterating over dimension key/value pairs.
  """

  def __init__(self, dimensions: Dict[str, str]):
    """
    Args:
      dimensions: A dict mapping dimension keys to values.
    """
    self._dimensions = collections.OrderedDict()
    for key in sorted(dimensions.keys()):
      value = dimensions[key]
      self._dimensions[key] = value.split('|')

  def Pairs(self) -> Generator[Tuple[str, List[str]], None, None]:
    """Iterates over the key/value pairs stored internally.

    Yields:
      (dimension_name, valid_values). |dimension_name| is a string containing
      the name of the dimension. |valid_values| is a list of one or more strings
      where each element is a valid value for |dimension_name|.
    """
    for dimension_name, valid_values in self._dimensions.items():
      yield dimension_name, valid_values

  def AsDict(self) -> Dict[str, List[str]]:
    """Returns a copy of the stored information as a dict."""
    return copy.deepcopy(self._dimensions)


def _LoadPylFile(filepath: str) -> Any:
  with open(filepath, encoding='utf-8') as infile:
    return ast.literal_eval(infile.read())


def GetMixinDimensions(mixin: str) -> 'DimensionSet':
  """Gets the dimensions provided by |mixin|.

  Args:
    mixin: The name of the mixin to look up.

  Returns:
    A dict mapping dimension names to values that |mixin| specifies.
  """
  mixin_content = _LoadPylFile(
      os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'testing', 'buildbot',
                   'mixins.pyl'))
  dimensions = mixin_content.get(mixin, {}).get('swarming',
                                                {}).get('dimensions')
  if not dimensions:
    raise RuntimeError(
        f'Specified mixin {mixin} does not contain Swarming dimensions')
  return DimensionSet(dimensions)
