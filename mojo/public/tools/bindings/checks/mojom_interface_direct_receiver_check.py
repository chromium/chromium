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
  # Array/Map must be traversed into.
  def _CheckFieldOrParam(self, root_kind):
    stack = [root_kind]
    visited = set()

    while stack:
      kind = stack.pop()

      if kind in visited:
        continue

      if module.IsAnyInterfaceKind(kind):
        if module.IsPendingRemoteKind(kind):
          continue
        if module.IsPendingAssociatedRemoteKind(kind):
          continue
        if not kind.kind.direct_receiver:
          interface = kind.kind.mojom_name
          raise check.CheckException(
              self.module, f"interface {interface} must be a DirectReceiver")

      elif module.IsStructKind(kind):
        visited.add(kind)
        for field in kind.fields:
          stack.append(field.kind)

      elif module.IsUnionKind(kind):
        visited.add(kind)
        for field in kind.fields:
          stack.append(field.kind)

      elif module.IsArrayKind(kind):
        stack.append(kind.kind)

      elif module.IsMapKind(kind):
        stack.append(kind.key_kind)
        stack.append(kind.value_kind)

  def _CheckInterface(self, interface):
    if not interface.direct_receiver:
      return
    for method in interface.methods:
      for param in method.parameters:
        self._CheckFieldOrParam(param.kind)
      if method.response_parameters:
        for param in method.response_parameters:
          self._CheckFieldOrParam(param.kind)

  def CheckModule(self):
    """Validate that any runtime feature guarded interfaces that might be passed
    over mojo are nullable."""
    for interface in self.module.interfaces:
      self._CheckInterface(interface)
