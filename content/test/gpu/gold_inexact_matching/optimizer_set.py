# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import typing

from gold_inexact_matching import base_parameter_optimizer as bpo
from gold_inexact_matching import common_typing as ct


class OptimizerSet():
  """Class to run a ParameterOptimizer for multiple tests."""

  def __init__(self, args: ct.ParsedCmdArgs,
               optimizer_class: typing.Type[bpo.BaseParameterOptimizer]):
    """
    Args:
      args: The parse arguments from an argparse.ArgumentParser.
      optimizer_class: The optimizer class to use for the optimization.
    """
    self._args = args
    self._optimizer_class = optimizer_class

  def RunOptimization(self) -> None:
    test_names = set(self._args.test_names)
    for name in test_names:
      print('Running optimization for test %s' % name)
      optimizer = self._optimizer_class(self._args, name)
      optimizer.RunOptimization()
