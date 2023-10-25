# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validate mojo runtime feature guarded interfaces are nullable."""

import mojom.generate.check as check
import mojom.generate.module as module


class Check(check.Check):
  def __init__(self, *args, **kwargs):
    super(Check, self).__init__(*args, **kwargs)

  # `param` is an Interface of some sort.
  def _CheckNonNullableFeatureGuardedInterface(self, kind):
    # Only need to validate interface if it has a RuntimeFeature
    if not kind.kind.runtime_feature:
      return
    # Nullable (optional) is ok as the interface expects they might not be sent.
    if kind.is_nullable:
      return
    interface = kind.kind.mojom_name
    raise check.CheckException(
        self.module,
        f"interface {interface} has a RuntimeFeature but is not nullable")

  # `param` can be a lot of things so check if it is a remote/receiver.
  # Array/Map must be recursed into.
  def _CheckFieldOrParam(self, kind):
    if module.IsAnyInterfaceKind(kind):
      self._CheckNonNullableFeatureGuardedInterface(kind)
    if module.IsArrayKind(kind):
      self._CheckFieldOrParam(kind.kind)
    if module.IsMapKind(kind):
      self._CheckFieldOrParam(kind.key_kind)
      self._CheckFieldOrParam(kind.value_kind)

  def _CheckInterfaceFeatures(self, interface):
    for method in interface.methods:
      for param in method.parameters:
        self._CheckFieldOrParam(param.kind)
      if method.response_parameters:
        for param in method.response_parameters:
          self._CheckFieldOrParam(param.kind)

  def _CheckStructFeatures(self, struct):
    for field in struct.fields:
      self._CheckFieldOrParam(field.kind)

  def _CheckUnionFeatures(self, union):
    for field in union.fields:
      self._CheckFieldOrParam(field.kind)

  def CheckModule(self):
    """Validate that any runtime feature guarded interfaces that might be passed
    over mojo are nullable."""
    for interface in self.module.interfaces:
      self._CheckInterfaceFeatures(interface)
    for struct in self.module.structs:
      self._CheckStructFeatures(struct)
    for union in self.module.unions:
      self._CheckUnionFeatures(union)
