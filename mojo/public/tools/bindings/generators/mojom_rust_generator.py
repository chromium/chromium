# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates Rust source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja


class Generator(generator.Generator):

  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)

  @staticmethod
  def GetTemplatePrefix():
    return "rust_templates"

  @staticmethod
  def GetFilters():
    return {}

  @UseJinja("module.tmpl")
  def _GenerateModule(self):
    return {"module": self.module}

  def GenerateFiles(self, unparsed_args):
    # Make sure all the AST nodes have pretty names
    self.module.Stylize(generator.Stylizer())

    self.WriteWithComment(self._GenerateModule(), f"{self.module.path}.rs")
