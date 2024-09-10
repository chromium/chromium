# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for interacting with the test spec files in //testing/buildbot."""

import ast
import os
from typing import Any, List

import gpu_path_util


def _LoadPylFile(filepath: str) -> Any:
  with open(filepath, encoding='utf-8') as infile:
    return ast.literal_eval(infile.read())


def GetBuildersWithMixin(mixin: str) -> List[str]:
  """Gets all publicly defined builders that use the specified mixin.

  Args:
    mixin: The mixin name to search for.

  Returns:
    A list of builder names have use |mixin| as a mixin.
  """
  builders_of_interest = []
  waterfalls_content = _LoadPylFile(
      os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'testing', 'buildbot',
                   'waterfalls.pyl'))
  for waterfall in waterfalls_content:
    for builder_name, configuration in waterfall['machines'].items():
      builder_mixins = configuration.get('mixins', [])
      if mixin in builder_mixins:
        builders_of_interest.append(builder_name)
  return builders_of_interest
