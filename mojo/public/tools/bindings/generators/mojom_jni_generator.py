# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ JNI bridge source files from a mojom.Module."""

import generators.jni_util as jni_util
from generators.mojom_cpp_generator import Generator as CppGenerator
import mojom.generate.generator as generator
from mojom.generate.template_expander import UseJinja

GENERATOR_PREFIX = 'jni'


class Generator(CppGenerator):
  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)

  def _GetJinjaExports(self):
    exports = super()._GetJinjaExports()
    exports['java_to_native_interfaces'] = [
      i
      for i in self.module.interfaces
      if jni_util.ShouldGenerateDirectReturnStub(i)
    ]
    return exports

  @UseJinja('module-jni.h.tmpl')
  def _GenerateModuleJniHeader(self):
    return self._GetJinjaExports()

  @UseJinja('module-jni.cc.tmpl')
  def _GenerateModuleJniSource(self):
    return self._GetJinjaExports()

  def GenerateFiles(self, unparsed_args):
    self.module.Stylize(generator.Stylizer())
    if '--jni_generate_all' not in unparsed_args and not any(
      jni_util.ShouldGenerateDirectReturnStub(i) for i in self.module.interfaces
    ):
      return
    suffix = '-%s' % self.variant if self.variant else ''
    self.WriteWithComment(
      self._GenerateModuleJniHeader(),
      '%s%s-jni.h' % (self.module.path, suffix),
    )
    self.WriteWithComment(
      self._GenerateModuleJniSource(),
      '%s%s-jni.cc' % (self.module.path, suffix),
    )
