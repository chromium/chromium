# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Validate RequireContext and AllowedContext annotations before generation."""

import mojom.generate.check as check
import mojom.generate.module as module


class Check(check.Check):
  def __init__(self, *args, **kwargs):
    self.kind_to_interfaces = dict()
    super(Check, self).__init__(*args, **kwargs)

  def _IsPassedInterface(self, candidate):
    if isinstance(
        candidate.kind,
        (module.PendingReceiver, module.PendingRemote,
         module.PendingAssociatedReceiver, module.PendingAssociatedRemote)):
      return True
    return False

  def _CheckInterface(self, method, param):
    # |param| is a pending_x<Interface> so need .kind.kind to get Interface.
    interface = param.kind.kind
    if interface.require_context:
      if method.allowed_context is None:
        raise check.CheckException(
            self.module, "method `{}` has parameter `{}` which passes interface"
            " `{}` that requires an AllowedContext annotation but none exists.".
            format(
                method.mojom_name,
                param.mojom_name,
                interface.mojom_name,
            ))
      # If a string was provided, or if an enum was not imported, this will
      # be a string and we cannot validate that it is in range.
      if not isinstance(method.allowed_context, module.EnumValue):
        raise check.CheckException(
            self.module,
            "method `{}` has AllowedContext={} which is not a valid enum value."
            .format(method.mojom_name, method.allowed_context))
      # EnumValue must be from the same enum to be compared.
      if interface.require_context.enum != method.allowed_context.enum:
        raise check.CheckException(
            self.module, "method `{}` has parameter `{}` which passes interface"
            " `{}` that requires AllowedContext={} but one of kind `{}` was "
            "provided.".format(
                method.mojom_name,
                param.mojom_name,
                interface.mojom_name,
                interface.require_context.enum,
                method.allowed_context.enum,
            ))
      # RestrictContext enums have most privileged field first (lowest value).
      interface_value = interface.require_context.field.numeric_value
      method_value = method.allowed_context.field.numeric_value
      if interface_value < method_value:
        raise check.CheckException(
            self.module, "RequireContext={} > AllowedContext={} for method "
            "`{}` which passes interface `{}`.".format(
                interface.require_context.GetSpec(),
                method.allowed_context.GetSpec(), method.mojom_name,
                interface.mojom_name))
      return True

  def _GatherReferencedInterfaces(self, field):
    key = field.kind.spec
    # structs/unions can nest themselves so we need to bookkeep.
    if not key in self.kind_to_interfaces:
      # Might reference ourselves so have to create the list first.
      self.kind_to_interfaces[key] = set()
      for param in field.kind.fields:
        if self._IsPassedInterface(param):
          self.kind_to_interfaces[key].add(param)
        elif isinstance(param.kind, (module.Struct, module.Union)):
          for iface in self._GatherReferencedInterfaces(param):
            self.kind_to_interfaces[key].add(iface)
    return self.kind_to_interfaces[key]

  def _CheckParams(self, method, params):
    # Note: we have to repeat _CheckParams for each method as each might have
    # different AllowedContext= attributes. We cannot memoize this function,
    # but can do so for gathering referenced interfaces as their RequireContext
    # attributes do not change.
    for param in params:
      if self._IsPassedInterface(param):
        self._CheckInterface(method, param)
      elif isinstance(param.kind, (module.Struct, module.Union)):
        for interface in self._GatherReferencedInterfaces(param):
          self._CheckInterface(method, interface)

  def _CheckMethod(self, method):
    if method.parameters:
      self._CheckParams(method, method.parameters)
    if method.response_parameters:
      self._CheckParams(method, method.response_parameters)

  def CheckModule(self):
    for interface in self.module.interfaces:
      for method in interface.methods:
        self._CheckMethod(method)
