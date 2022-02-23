# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.



class OptimizerSet():
  """Class to run a ParameterOptimizer for multiple tests."""

  def __init__(self, args, optimizer_class):
    """
    Args:
      args: The parse arguments from an argparse.ArgumentParser.
      optimizer_class: The optimizer class to use for the optimization.
    """
    self._args = args
    self._optimizer_class = optimizer_class

  def RunOptimization(self):
    test_names = set(self._args.test_names)
    for name in test_names:
      print('Running optimization for test %s' % name)
      optimizer = self._optimizer_class(self._args, name)
      optimizer.RunOptimization()
