# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implement the common methods used in mojom_cpp_generator.py and
mojom_cpp_parameter_tracing.py in order to get rid of circular dependency."""

import mojom.generate.module as mojom


def IsNativeOnlyKind(kind):
  return (mojom.IsStructKind(kind) or mojom.IsEnumKind(kind)) and \
      kind.native_only
