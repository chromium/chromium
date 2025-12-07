# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validate mojo interfaces passed by [DirectReceiver] interfaces are also
[DirectReceiver] interfaces."""

import mojom.generate.check as check
import mojom.generate.module as module


class Check(check.Check):

  def __init__(self, *args, **kwargs):
    super(Check, self).__init__(*args, **kwargs)

  # `param` can be a lot of things so check if it is a remote/receiver.
  # Array/Map must be recursed into.
  def _CheckFieldOrParam(self, kind):
    if module.IsAnyInterfaceKind(kind):
      if module.IsPendingRemoteKind(kind):
        return
      if module.IsPendingAssociatedRemoteKind(kind):
        return
      if not kind.kind.direct_receiver:
        interface = kind.kind.mojom_name
        raise check.CheckException(
            self.module, f"interface {interface} must be a DirectReceiver")
    if module.IsStructKind(kind):
      self._CheckStruct(kind)
    if module.IsUnionKind(kind):
      self._CheckUnion(kind)
    if module.IsArrayKind(kind):
      self._CheckFieldOrParam(kind.kind)
    if module.IsMapKind(kind):
      self._CheckFieldOrParam(kind.key_kind)
      self._CheckFieldOrParam(kind.value_kind)

  def _CheckInterface(self, interface):
    if not interface.direct_receiver:
      return
    for method in interface.methods:
      for param in method.parameters:
        self._CheckFieldOrParam(param.kind)
      if method.response_parameters:
        for param in method.response_parameters:
          self._CheckFieldOrParam(param.kind)

  def _CheckStruct(self, struct):
    for field in struct.fields:
      self._CheckFieldOrParam(field.kind)

  def _CheckUnion(self, union):
    for field in union.fields:
      self._CheckFieldOrParam(field.kind)

  def CheckModule(self):
    """Validate that any runtime feature guarded interfaces that might be passed
    over mojo are nullable."""
    for interface in self.module.interfaces:
      self._CheckInterface(interface)
