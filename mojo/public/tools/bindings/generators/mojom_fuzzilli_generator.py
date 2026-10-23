# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja
from generators.mojom_js_generator import JavaScriptStylizer

# Supported features, design decisions, trade-offs, and known quirks of the
# generated Fuzzilli profiles are documented in
# //testing/libfuzzer/research/fuzzilli_mojom_fuzzing/README.md.

GENERATOR_PREFIX = "fuzzilli"
# Map primitive predicates to (fuzzilli type representation, type name). Type
# name refers to the name used for arrays and maps (e.g. jsInt64MojoArray).
PRIMITIVES_MAPPING = {
  mojom.BOOL: ("boolean", "Bool"),
  mojom.INT8: ("jsInt8", "Int8"),
  mojom.INT16: ("jsInt16", "Int16"),
  mojom.INT32: ("jsInt32", "Int32"),
  mojom.INT64: ("bigint", "Int64"),
  mojom.UINT8: ("jsUint8", "Uint8"),
  mojom.UINT16: ("jsUint16", "Uint16"),
  mojom.UINT32: ("jsUint32", "Uint32"),
  # TODO(crbug.com/553587894): Determine a way to support uint64, as
  # `ILType.bigint` is currently internally represented as an int64.
  mojom.UINT64: ("bigint", "Uint64"),
  mojom.FLOAT: ("float", "Float"),
  mojom.DOUBLE: ("float", "Float"),  # no dedicated `.double` type
  mojom.STRING: ("string", "String"),
  mojom.NULLABLE_BOOL: ("boolean", "Bool"),
  mojom.NULLABLE_INT8: ("jsInt8", "Int8"),
  mojom.NULLABLE_INT16: ("jsInt16", "Int16"),
  mojom.NULLABLE_INT32: ("jsInt32", "Int32"),
  mojom.NULLABLE_INT64: ("bigint", "Int64"),
  mojom.NULLABLE_UINT8: ("jsUint8", "Uint8"),
  mojom.NULLABLE_UINT16: ("jsUint16", "Uint16"),
  mojom.NULLABLE_UINT32: ("jsUint32", "Uint32"),
  mojom.NULLABLE_UINT64: ("bigint", "Uint64"),
  mojom.NULLABLE_FLOAT: ("float", "Float"),
  mojom.NULLABLE_DOUBLE: ("float", "Float"),
  mojom.NULLABLE_STRING: ("string", "String"),
}
# Map raw Mojo handle kinds to their Fuzzilli UniqueName
# TODO(crbug.com/553473421): Add support for MSGPIPE and PLATFORMHANDLE
HANDLES_MAPPING = {
  mojom.HANDLE: "MojoHandle",
  mojom.DPPIPE: "MojoDataPipeProducerHandle",
  mojom.DCPIPE: "MojoDataPipeConsumerHandle",
  mojom.SHAREDBUFFER: "MojoSharedBufferHandle",
  mojom.NULLABLE_HANDLE: "MojoHandle",
  mojom.NULLABLE_DPPIPE: "MojoDataPipeProducerHandle",
  mojom.NULLABLE_DCPIPE: "MojoDataPipeConsumerHandle",
  mojom.NULLABLE_SHAREDBUFFER: "MojoSharedBufferHandle",
}
# List of types skipped during profile generation. These types should be
# hand-defined in testing/libfuzzer/research/fuzzilli_mojom_fuzzing/common/. The
# files should then be copied over to the same directory as the rest of the
# Fuzzilli profiles.
IGNORED_TYPES = {
  "mojoBase.mojom.BigBuffer",
  "mojoBase.mojom.BigBufferSharedMemoryRegion",
  "mojoBase.mojom.String16",
  "mojoBase.mojom.BigString16",
  "mojoBase.mojom.BigString",
  "mojoBase.mojom.Uint128",
  "network.mojom.ConnectionInfo",
  "network.mojom.EffectiveConnectionType",
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

    # The following track which pending endpoint types are used from the JS
    # side.
    self.interface_pending_remotes = set()
    self.interface_associated_pending_remotes = set()
    self.interface_pending_receivers = set()
    self.interface_associated_pending_receivers = set()

    self.arrays = {}
    self.maps = {}
    self.enums = {}
    self.unions = {}
    self.structs = {}
    # Methods with a response return a Promise that resolves to a JavaScript
    # object holding the response parameters. Represent these objects as
    # `mojom.Struct` objects, since we can then handle them the same way we
    # handle Mojo structs in a few parts of the templates. The lite bindings
    # have no synchronous calls, so `[Sync]` methods also return a Promise.
    self.response_structs = {}

  def GetFilters(self):
    return {
      "callback_receiver_name": self._FormatCallbackReceiverName,
      "format_il_type": self._ILTypeName,
      "format_unique_name": self._FormatUniqueName,
      "fully_qualified_name": self._FullyQualifiedName,
      "is_array_kind": mojom.IsArrayKind,
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
      "mojo_scripts_page_path": self._GetMojoScriptsPagePath(),
      "primary": self.primary_interface,
      "interface_remotes": list(self.interface_remotes.values()),
      "interface_receivers": list(self.interface_receivers.values()),
      "interface_pending_remotes": list(self.interface_pending_remotes),
      "interface_associated_pending_remotes": list(
        self.interface_associated_pending_remotes
      ),
      "interface_pending_receivers": list(self.interface_pending_receivers),
      "interface_associated_pending_receivers": list(
        self.interface_associated_pending_receivers
      ),
      "arrays": list(self.arrays.values()),
      "maps": list(self.maps.values()),
      "enums": list(self.enums.values()),
      "unions": list(self.unions.values()),
      "structs": list(self.structs.values()),
      "response_structs": list(self.response_structs.values()),
    }

  def _IsIgnoredType(self, kind):
    if not self._IsUserType(kind):
      return False
    return self._FullyQualifiedName(kind) in IGNORED_TYPES

  def _CollectInterfaceAndTypes(self, kind, is_in_js):
    if self._IsIgnoredType(kind):
      return

    if mojom.IsStructKind(kind):
      self._CollectStruct(kind, is_in_js)
    elif mojom.IsEnumKind(kind):
      self._CollectEnum(kind)
    elif mojom.IsArrayKind(kind):
      self._CollectArray(kind, is_in_js)
    elif mojom.IsMapKind(kind):
      self._CollectMap(kind, is_in_js)
    elif mojom.IsInterfaceKind(kind) or mojom.IsAnyInterfaceKind(kind):
      self._CollectInterface(kind, is_in_js)
    elif mojom.IsUnionKind(kind):
      self._CollectUnion(kind, is_in_js)

  def _CollectStruct(self, struct, is_in_js):
    name = self._FormatUniqueName(struct)
    if name in self.structs:
      return
    self.structs[name] = struct
    for field in struct.fields:
      self._CollectInterfaceAndTypes(field.kind, is_in_js)

  def _CollectArray(self, array, is_in_js):
    name = self._FormatUniqueName(array.kind)
    if name in self.arrays:
      return
    self.arrays[name] = array
    self._CollectInterfaceAndTypes(array.kind, is_in_js)

  def _CollectMap(self, m, is_in_js):
    name = self._FormatUniqueName(m)
    if name in self.maps:
      return
    self.maps[name] = m
    self._CollectInterfaceAndTypes(m.key_kind, is_in_js)
    self._CollectInterfaceAndTypes(m.value_kind, is_in_js)

  def _CollectEnum(self, enum):
    name = self._FormatUniqueName(enum)
    self.enums[name] = enum

  def _CollectUnion(self, union, is_in_js):
    name = self._FormatUniqueName(union)
    if name in self.unions:
      return
    self.unions[name] = union
    for field in union.fields:
      self._CollectInterfaceAndTypes(field.kind, is_in_js)

  # Marks the interface as a remote or receiver depending on which "side"
  # (JS or browser) the interface is used from. The `is_in_js` parameter
  # tracks whether the current context is in the JS side and alternates
  # accordingly
  def _CollectInterface(self, kind, is_in_js):
    is_pending_remote = self._IsAnyPendingRemoteKind(kind)
    is_pending_receiver = self._IsAnyPendingReceiverKind(kind)

    interface = None
    if is_pending_remote or is_pending_receiver:
      interface = kind.kind
    else:
      interface = kind
    name = self._FormatUniqueName(interface)

    # Track the pending endpoints used from the JS side. A pending remote passed
    # from the browser side is used directly as a `Remote`, so it is not
    # tracked.
    if is_pending_receiver or (is_pending_remote and is_in_js):
      self._TrackPendingInterfaces(kind)

    # TODO(crbug.com/522372048): this check prevents interfaces from being
    # registered as both remotes and receivers. Rewrite the logic to
    # support the edge case where an interface is used as both.
    if name in self.interface_remotes or name in self.interface_receivers:
      return

    if is_pending_remote:
      if is_in_js:
        # Pending remote passed from JS side, JS keeps receiver
        self.interface_receivers[name] = interface
      else:
        # Pending remote passed from browser side, JS keeps remote
        self.interface_remotes[name] = interface
      is_in_js = not is_in_js  # interface is used from the other side
    elif is_pending_receiver:
      if is_in_js:
        # Pending receiver passed from JS side, JS keeps remote
        self.interface_remotes[name] = interface
      else:
        # Pending receiver passed from browser side, JS keeps receiver
        self.interface_receivers[name] = interface
      # Do NOT flip is_in_js; the interface is used from the same side that
      # passed the receiver to the other side.
    else:
      self.interface_remotes[name] = interface

    for method in interface.methods:
      for param in method.parameters:
        self._CollectInterfaceAndTypes(param.kind, is_in_js)

      if not method.response_param_struct:
        continue
      name = self._FormatUniqueName(method.response_param_struct)
      self.response_structs[name] = method.response_param_struct
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
  def _FormatUniqueName(self, kind):
    if kind in PRIMITIVES_MAPPING:
      _, element_prefix = PRIMITIVES_MAPPING[kind]
      return element_prefix

    if kind in HANDLES_MAPPING:
      return HANDLES_MAPPING[kind]

    # Certain kinds, such as `Array`, do not have a `module` attribute
    prefix = (
      ""
      if not kind.module
      else "".join(
        generator.ToCamel(part) for part in kind.module.namespace.split(".")
      )
    )

    if mojom.IsArrayKind(kind):
      # Despite the lack of a prefix, the element's unique name ensures that
      # the name of the array is unique within each profile. If two modules
      # have the same array (e.g., `FooMojoArray`), the name's will not collide
      # as the `ILType` definitions are `fileprivate`. The "Mojo" suffix ensures
      # that the types do not collide with Fuzzilli internal types
      # (e.g. jsUint8Array)
      return f"{self._FormatUniqueName(kind.kind)}MojoArray"

    if mojom.IsMapKind(kind):
      return (
        f"{self._FormatUniqueName(kind.key_kind)}_"
        f"{self._FormatUniqueName(kind.value_kind)}MojoMap"
      )

    if self._IsAnyPendingRemoteKind(kind) or self._IsAnyPendingReceiverKind(
      kind
    ):
      return f"{prefix}{self._FlattenKind(kind.kind)}"

    if mojom.IsUnionKind(kind):
      # Fuzzilli union constructors combine the union name with the field name,
      # creating the potential for collisions with other constructors generated
      # by Fuzzilli.
      return f"{prefix}{self._FlattenKind(kind)}Union"

    if mojom.IsStructKind(kind) or mojom.IsEnumKind(kind):
      return f"{prefix}{self._FlattenKind(kind)}"

    if mojom.IsInterfaceKind(kind):
      return f"{prefix}{kind.name}"

    assert False, f"Unsupported type: {kind}."

  # Maps a Mojom kind to its corresponding Fuzzilli Intermediate Language (IL)
  # type name representation (e.g., "boolean", "jsFooStruct").
  def _ILTypeName(self, kind):
    if kind in PRIMITIVES_MAPPING:
      il_type, _ = PRIMITIVES_MAPPING[kind]
      return il_type

    if (
      mojom.IsStructKind(kind)
      or mojom.IsEnumKind(kind)
      or mojom.IsUnionKind(kind)
      or kind in HANDLES_MAPPING
    ):
      return f"js{self._FormatUniqueName(kind)}"

    if mojom.IsArrayKind(kind):
      return f"js{self._FormatUniqueName(kind.kind)}MojoArray"

    if mojom.IsMapKind(kind):
      return (
        f"js{self._FormatUniqueName(kind.key_kind)}_"
        f"{self._FormatUniqueName(kind.value_kind)}MojoMap"
      )

    if mojom.IsInterfaceKind(kind):
      return f"js{self._FormatUniqueName(kind)}Remote"

    # When JS implements the interface (i.e., it is a receiver), JS only passes
    # remotes to the browser, so they are typed after their Mojom kind
    # (`PendingRemote` / `PendingAssociatedRemote`). Remotes that JS calls into
    # keep the `Remote` type, which carries the interface's methods.
    if self._IsAnyPendingRemoteKind(kind):
      name = self._FormatUniqueName(kind.kind)
      if name not in self.interface_receivers:
        return f"js{name}Remote"
      if mojom.IsPendingAssociatedRemoteKind(kind):
        return f"js{name}PendingAssociatedRemote"
      return f"js{name}PendingRemote"

    if mojom.IsPendingReceiverKind(kind):
      return f"js{self._FormatUniqueName(kind.kind)}PendingReceiver"

    if mojom.IsPendingAssociatedReceiverKind(kind):
      return f"js{self._FormatUniqueName(kind.kind)}PendingAssociatedReceiver"

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

  def _GetMojoScriptsPagePath(self):
    """Returns the path of the Mojo scripts page of the module, i.e. the page
    loading the MojoJS bindings of the module, relative to the root gen
    directory."""
    return "%s-scripts-page.html" % self.module.path

  def _CollectModuleDependencies(
    self,
    m: mojom.Module,
    js_modules: dict[str, None],
  ):
    """Appends the lite bindings of |m| and of its transitive imports to
    |js_modules|, in dependency order."""
    js_path = f'gen/{m.path}-lite.js'
    if js_path in js_modules:
      return

    for imported_module in m.imports:
      self._CollectModuleDependencies(imported_module, js_modules)

    js_modules[js_path] = None

  @UseJinja("mojo_scripts_page.tmpl")
  def _GenerateMojoScriptsPage(self):
    js_modules: dict[str, None] = {}
    self._CollectModuleDependencies(
      self.module,
      js_modules,
    )
    return {
      "module": self.module,
      "js_modules": js_modules,
    }

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--fuzzilli_primary_interface_name')
    args = parser.parse_args(unparsed_args)

    self.Write(self._GenerateMojoScriptsPage(), self._GetMojoScriptsPagePath())

    primary_interface_name = args.fuzzilli_primary_interface_name
    # GN doesn't provide primary interface when generating Mojo scripts page.
    if not primary_interface_name:
      return
    self.primary_interface = next(
      (
        i
        for i in self.module.interfaces
        if i.mojom_name == primary_interface_name
      ),
      None,
    )
    if not self.primary_interface:
      raise Exception(
        f'Unable to find primary interface "{primary_interface_name}".'
      )

    file_name = "%s.MojoProfile.swift" % self.module.path
    self.WriteWithComment(self._GenerateFuzzilliModule(), file_name)

  # Stores the interface of a pending kind used from the JS side in the
  # dictionary matching that pending kind.
  #
  # Note: takes the Mojom pending kind as it is used. So, this method should
  # receive the kind `pending_remote<Foo>`, not the kind `Foo`.
  def _TrackPendingInterfaces(self, pending_kind):
    interface = pending_kind.kind
    if mojom.IsPendingRemoteKind(pending_kind):
      self.interface_pending_remotes.add(interface)
    elif mojom.IsPendingAssociatedRemoteKind(pending_kind):
      self.interface_associated_pending_remotes.add(interface)
    elif mojom.IsPendingReceiverKind(pending_kind):
      self.interface_pending_receivers.add(interface)
    elif mojom.IsPendingAssociatedReceiverKind(pending_kind):
      self.interface_associated_pending_receivers.add(interface)
    else:
      assert False, (
        f"Expected pending interface kind, received: {pending_kind}."
      )

  def _IsAnyPendingRemoteKind(self, kind):
    return mojom.IsPendingRemoteKind(
      kind
    ) or mojom.IsPendingAssociatedRemoteKind(kind)

  def _IsAnyPendingReceiverKind(self, kind):
    return mojom.IsPendingReceiverKind(
      kind
    ) or mojom.IsPendingAssociatedReceiverKind(kind)

  def _IsUserType(self, kind):
    return (
      mojom.IsStructKind(kind)
      or mojom.IsEnumKind(kind)
      or mojom.IsUnionKind(kind)
    )

  def _FormatCallbackReceiverName(self, method):
    return (
      f"{self._FormatUniqueName(method.interface)}"
      f"{generator.ToCamel(method.name)}CallbackReceiver"
    )

  def _NamespaceAsArray(self, namespace):
    return namespace.split(".")
