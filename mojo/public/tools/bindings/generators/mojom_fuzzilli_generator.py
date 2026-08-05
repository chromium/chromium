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
# List of types skipped during profile generation.
# These types should be hand-defined in MojoCommonProfile.swift; its definitions
# are always used in every generated profile.
IGNORED_TYPES = {
    "mojoBase.mojom.BigBuffer",
    "mojoBase.mojom.BigBufferSharedMemoryRegion",
    "mojoBase.mojom.String16",
    "mojoBase.mojom.BigString16",
    "mojoBase.mojom.BigString",
    "mojoBase.mojom.Uint128",
    "skia.mojom.BitmapN32",
    "skia.mojom.BitmapN32ImageInfo",
    "skia.mojom.AlphaType",
    "url.mojom.Url",
    "url.mojom.SchemeHostPort",
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
    self.arrays = {}
    self.enums = {}

  def GetFilters(self):
    return {
        "callback_receiver_name": self._FormatCallbackReceiverName,
        "format_il_type": self._ILTypeName,
        "format_unique_name": self._FormatUniqueName,
        "fully_qualified_name": self._FullyQualifiedName,
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
        "arrays": list(self.arrays.values()),
        "enums": list(self.enums.values()),
    }

  def _IsIgnoredType(self, kind):
    if not self._IsUserType(kind):
      return False
    return self._FullyQualifiedName(kind) in IGNORED_TYPES

  def _CollectInterfaceAndTypes(self, kind, is_in_js):
    if self._IsIgnoredType(kind):
      return

    if mojom.IsEnumKind(kind):
      self._CollectEnum(kind)
    elif mojom.IsArrayKind(kind):
      self._CollectArray(kind, is_in_js)
    elif mojom.IsAnyInterfaceKind(kind):
      self._CollectInterface(kind, is_in_js)

  def _CollectArray(self, array, is_in_js):
    name = self._FormatUniqueName(array.kind)
    if name in self.arrays:
      return
    self.arrays[name] = array
    self._CollectInterfaceAndTypes(array.kind, is_in_js)

  def _CollectEnum(self, enum):
    name = self._FormatUniqueName(enum)
    self.enums[name] = enum

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

    name = self._FormatUniqueName(interface)
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

  # Returns the "flattened" name of a Mojom kind, recursing into parent_kind
  # for nested definitions (e.g., "ParentStruct_NestedEnum").
  def _FlattenKind(self, named_kind):
    if named_kind.parent_kind:
      return f"{self._FlattenKind(named_kind.parent_kind)}_{named_kind.name}"
    return named_kind.name

  # Formats a unique type identifier string across namespaces for a Mojom type
  # by combining the CamelCase namespace prefix with the type's local name.
  # The `primitive_with_suffix` argument determines whether the name returned
  # for primitives represents the primitive itself or a proxy `IL.object`
  # type. These proxy types are identified by their `Element` suffix.
  def _FormatUniqueName(self, kind, primitive_with_suffix=False):
    if kind in PRIMITIVES_MAPPING:
      name = generator.ToCamel(PRIMITIVES_MAPPING[kind])
      return name + "Element" if primitive_with_suffix else name

    # Certain kinds, such as `Array`, do not have a `module` attribute
    prefix = "" if not kind.module else "".join(
        generator.ToCamel(part) for part in kind.module.namespace.split("."))

    if mojom.IsArrayKind(kind):
      # Despite the lack of a prefix, the element's unique name ensures that
      # the name of the array is unique within each profile. If two modules
      # have the same array (e.g., `FooArray`), the name's will not collide
      # as the `ILType` definitions are `fileprivate`.
      return f"{self._FormatUniqueName(kind.kind)}Array"

    if self._IsAnyPendingRemoteKind(kind) or self._IsAnyPendingReceiverKind(
        kind):
      return f"{prefix}{self._FlattenKind(kind.kind)}"

    if mojom.IsStructKind(kind) or mojom.IsEnumKind(kind):
      return f"{prefix}{self._FlattenKind(kind)}"

    if mojom.IsInterfaceKind(kind):
      return f"{prefix}{kind.name}"

    assert False, f"Unsupported type: {kind}."

  # Maps a Mojom kind to its corresponding Fuzzilli Intermediate Language (IL)
  # type name representation (e.g., "boolean", "jsFooStruct").
  # The `primitive_with_suffix` argument determines whether the name returned
  # for primitives represents the primitive itself or a proxy `IL.object`
  # type. These proxy types are identified by their `Element` suffix.
  # TODO(crbug.com/522372048): Handle nullable types explicitly. Currently, we
  # silently generate non-nullables for nullable types.
  def _ILTypeName(self, kind, primitive_with_suffix=False):
    if kind in PRIMITIVES_MAPPING:
      name = PRIMITIVES_MAPPING[kind]
      if primitive_with_suffix:
        return f"js{generator.ToCamel(name)}Element"
      return name

    if mojom.IsStructKind(kind) or mojom.IsEnumKind(kind):
      return f"js{self._FormatUniqueName(kind)}"

    if mojom.IsArrayKind(kind):
      return f"js{self._FormatUniqueName(kind.kind)}Array"

    if mojom.IsInterfaceKind(kind):
      return f"js{self._FormatUniqueName(kind)}Remote"

    if self._IsAnyPendingRemoteKind(kind):
      return f"js{self._FormatUniqueName(kind.kind)}Remote"

    if self._IsAnyPendingReceiverKind(kind):
      return f"js{self._FormatUniqueName(kind.kind)}PendingReceiver"

    assert False, f"Unsupported type: {kind}."

  # Returns the complete dot-separated path of a Mojom type, combining its
  # module namespace with its local name (e.g., "mojoBase.mojom.String16").
  # Used for group names.
  def _FullyQualifiedName(self, kind):
    suffix = self._FlattenKind(kind)
    if not kind.module:
      return suffix
    return f"{kind.module.namespace}.{suffix}"

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

  def _IsUserType(self, kind):
    return mojom.IsStructKind(kind) or mojom.IsEnumKind(
        kind) or mojom.IsUnionKind(kind)

  def _FormatCallbackReceiverName(self, method):
    return (f"{self._FormatUniqueName(method.interface)}"
            f"{generator.ToCamel(method.name)}CallbackReceiver")

  def _NamespaceAsArray(self, namespace):
    return namespace.split(".")
