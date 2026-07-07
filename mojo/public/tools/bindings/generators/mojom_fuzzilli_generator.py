# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja
from generators.mojom_js_generator import JavaScriptStylizer

GENERATOR_PREFIX = "fuzzilli"
# Map primitive predicates to the fuzzilli type representation
PRIMITIVES_MAPPING = {
    mojom.BOOL: "boolean",
    mojom.INT8: "integer",
    mojom.INT16: "integer",
    mojom.INT32: "integer",
    mojom.INT64: "integer",
    mojom.UINT8: "integer",
    mojom.UINT16: "integer",
    mojom.UINT32: "integer",
    mojom.UINT64: "integer",
    mojom.FLOAT: "float",
    mojom.DOUBLE: "float",  # no dedicated `.double` type
    mojom.STRING: "string",
}


class Generator(generator.Generator):

  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)

    # The primary interface is the first interface that the renderer requests
    # from the browser. All other interfaces, which will be bound using the
    # primary interface, are considered secondary interfaces
    self.primary_interface = None
    self.interface_remotes = {}
    self.interface_receivers = {}

  def GetFilters(self):
    return {
        "callback_receiver_name": self._FormatCallbackReceiverName,
        "format_il_type": self._ILTypeName,
        "name_with_namespace": self._NameWithNamespace,
        "namespace_as_array": self._NamespaceAsArray,
        "to_camel": generator.ToCamel,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "fuzzilli_templates"

  def _GetParameters(self):
    # Stylize first to get JS names (camelCase for fields/methods)
    self.module.Stylize(JavaScriptStylizer())

    self._CollectInterfaceAndTypes(self.primary_interface, is_in_js=True)

    return {
        "module": self.module,
        "primary": self.primary_interface,
        "interface_remotes": list(self.interface_remotes.values()),
        "interface_receivers": list(self.interface_receivers.values()),
    }

  def _CollectInterfaceAndTypes(self, kind, is_in_js):
    if mojom.IsAnyInterfaceKind(kind):
      self._CollectInterface(kind, is_in_js)

  # Marks the interface as a remote or receiver depending on which "side"
  # (JS or browser) the interface is used from. The `is_in_js` parameter
  # tracks whether the current context is in the JS side and alternates
  # accordingly
  def _CollectInterface(self, kind, is_in_js):
    is_pending_remote = self._IsAnyPendingRemoteKind(kind)
    is_pending_receiver = self._IsAnyPendingReceiverKind(kind)

    interface = None
    if is_pending_remote or is_pending_receiver:
      # TODO(crbug.com/522372048): add handling for non-associated interfaces
      assert self._IsPendingAssociatedKind, (
          "Only pending associated interfaces are supported.")
      interface = kind.kind
    else:
      interface = kind

    name = self._FuzzilliName(interface)
    if name in self.interface_remotes or name in self.interface_receivers:
      return

    if is_pending_remote or is_pending_receiver:
      if is_pending_remote == is_in_js:
        # Either (1) is pending remote used in JS side OR
        #        (2) is pending receiver used in browser side
        # In either case, JS side keeps a receiver
        self.interface_receivers[name] = interface
      else:
        # Either (1) is pending remote used in browser side OR
        #        (2) is pending receiver used in JS side
        # In either case, JS side keeps a remote
        self.interface_remotes[name] = interface
      is_in_js = not is_in_js  # interface is used from other side
    else:
      self.interface_remotes[name] = interface

    for method in interface.methods:
      for param in method.parameters:
        self._CollectInterfaceAndTypes(param.kind, is_in_js)

      if not method.response_parameters:
        continue
      for param in method.response_parameters:
        self._CollectInterfaceAndTypes(param.kind, not is_in_js)

  def _FuzzilliName(self, kind):
    name = []
    if kind.parent_kind:
      name.append(kind.parent_kind.name)
    name.append(kind.name)
    return "".join(name)

  # TODO(crbug.com/522372048): Handle nullable types explicitly. Currently, we
  # silently generate non-nullables for nullable types.
  def _ILTypeName(self, kind):
    if kind in PRIMITIVES_MAPPING:
      return PRIMITIVES_MAPPING[kind]

    if mojom.IsStructKind(kind) or mojom.IsEnumKind(kind):
      return f"js{self._FuzzilliName(kind)}"

    if mojom.IsInterfaceKind(kind):
      return f"js{self._FuzzilliName(kind)}Remote"

    if self._IsAnyPendingRemoteKind(kind):
      return f"js{self._FuzzilliName(kind.kind)}Remote"

    if self._IsAnyPendingReceiverKind(kind):
      return f"js{self._FuzzilliName(kind.kind)}PendingReceiver"

    if mojom.IsArrayKind(kind):
      return f"js{self._FuzzilliName(kind.kind)}Array"

    assert False, f"Unsupported type: {kind}."

  @UseJinja("fuzzilli_profile.tmpl")
  def _GenerateFuzzilliModule(self):
    return self._GetParameters()

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--fuzzilli_primary_interface_name')
    args = parser.parse_args(unparsed_args)

    primary_interface_name = args.fuzzilli_primary_interface_name
    self.primary_interface = next((i for i in self.module.interfaces
                                   if i.mojom_name == primary_interface_name),
                                  None)
    if not self.primary_interface:
      raise Exception(
          f'Unable to find primary interface "{primary_interface_name}".')

    file_name = "%s.MojoProfile.swift" % self.module.path
    self.WriteWithComment(self._GenerateFuzzilliModule(), file_name)

  def _IsPendingAssociatedKind(self, kind):
    return mojom.IsPendingAssociatedRemoteKind(
        kind) or mojom.IsPendingAssociatedReceiverKind(kind)

  def _IsAnyPendingRemoteKind(self, kind):
    return mojom.IsPendingRemoteKind(
        kind) or mojom.IsPendingAssociatedRemoteKind(kind)

  def _IsAnyPendingReceiverKind(self, kind):
    return mojom.IsPendingReceiverKind(
        kind) or mojom.IsPendingAssociatedReceiverKind(kind)

  def _FormatCallbackReceiverName(self, method):
    return (f"{method.interface.name}"
            f"{generator.ToCamel(method.name)}CallbackReceiver")

  def _NameWithNamespace(self, kind):
    parent_suffix = kind.parent_kind.name if kind.parent_kind else ""
    return f"{kind.module.namespace}.{parent_suffix}{kind.name}"

  def _NamespaceAsArray(self, namespace):
    return namespace.split(".")
