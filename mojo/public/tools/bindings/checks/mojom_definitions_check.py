# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Ensure no duplicate type definitions before generation."""

import mojom.generate.check as check
import mojom.generate.module as module


class Check(check.Check):
  def __init__(self, *args, **kwargs):
    super(Check, self).__init__(*args, **kwargs)

  def CheckModule(self):
    kinds = dict()
    for module in self.module.imports:
      for kind in module.enums + module.structs + module.unions:
        kind_name = f'{kind.module.mojom_namespace}.{kind.mojom_name}'
        if kind_name in kinds:
          previous_module = kinds[kind_name]
          if previous_module.path != module.path:
            raise check.CheckException(
                self.module, f"multiple-definition for type {kind_name}" +
                f"(defined in both {previous_module} and {module})")
        kinds[kind_name] = kind.module

    for kind in self.module.enums + self.module.structs + self.module.unions:
      kind_name = f'{kind.module.mojom_namespace}.{kind.mojom_name}'
      if kind_name in kinds:
        previous_module = kinds[kind_name]
        raise check.CheckException(
            self.module, f"multiple-definition for type {kind_name}" +
            f"(previous definition in {previous_module})")
    return True
