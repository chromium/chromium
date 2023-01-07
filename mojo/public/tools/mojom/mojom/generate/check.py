# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code shared by the various pre-generation mojom checkers."""


class CheckException(Exception):
  def __init__(self, module, message):
    self.module = module
    self.message = message
    super().__init__(self.message)

  def __str__(self):
    return "Failed mojo pre-generation check for {}:\n{}".format(
        self.module.path, self.message)


class Check:
  def __init__(self, module):
    self.module = module

  def CheckModule(self):
    """ Subclass should return True if its Checks pass, and throw an
    exception otherwise. CheckModule will be called immediately before
    mojom.generate.Generator.GenerateFiles()"""
    raise NotImplementedError("Subclasses must override/implement this method")
