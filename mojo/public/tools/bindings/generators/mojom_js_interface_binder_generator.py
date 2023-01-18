# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ source files from a mojom.Module with a JSInterfaceBinder"""
import os.path
import sys


# Set up |sys.path| so that this module works without user-side setup of
# PYTHONPATH assuming Chromium's directory tree structure.
def _setup_sys_path():
  expected_path = 'mojo/public/tools/bindings/generators/'

  this_dir = os.path.dirname(__file__)
  root_dir = os.path.abspath(
      os.path.join(this_dir, *(['..'] * expected_path.count('/'))))

  module_dirs = (
      # //mojo/public/tools/mojom
      os.path.join(root_dir, "mojo", "public", "tools", "mojom"), )
  for module_dir in reversed(module_dirs):
    # Preserve sys.path[0] as is.
    # https://docs.python.org/3/library/sys.html?highlight=path[0]#sys.path
    sys.path.insert(1, module_dir)


_setup_sys_path()

from mojom.generate import generator
from mojom.generate.template_expander import UseJinja
from mojom.generate import template_expander
# Import it as "mojom" because "module" is a common variable name.
import mojom.generate.module as mojom


class Generator(generator.Generator):
  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)

  @staticmethod
  def GetTemplatePrefix():
    return "js_interface_binder_templates"

  def GetFilters(self):
    return {}

  def _GetParameters(self):
    return {}

  @UseJinja("js_interface_binder_impl.h.tmpl")
  def _GenerateJsInterfaceBinderImpl(self):
    return self._GetParameters()

  def GenerateFiles(self, args):
    self.module.Stylize(generator.Stylizer())
    self.WriteWithComment(self._GenerateJsInterfaceBinderImpl(),
                          "%s-js-interface-binder-impl.h" % self.module.path)
