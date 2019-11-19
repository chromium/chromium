# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code shared by the various language-specific code generators."""

from __future__ import print_function

from functools import partial
import os.path
import re

import mojom.generate.module as mojom
import mojom.fileutil as fileutil
import mojom.generate.pack as pack


def ExpectedArraySize(kind):
  if mojom.IsArrayKind(kind):
    return kind.length
  return None


def SplitCamelCase(identifier):
  """Splits a camel-cased |identifier| and returns a list of lower-cased
  strings.
  """
  # Add underscores after uppercase letters when appropriate. An uppercase
  # letter is considered the end of a word if it is followed by an upper and a
  # lower. E.g. URLLoaderFactory -> URL_LoaderFactory
  identifier = re.sub('([A-Z][0-9]*)(?=[A-Z][0-9]*[a-z])', r'\1_', identifier)
  # Add underscores after lowercase letters when appropriate. A lowercase letter
  # is considered the end of a word if it is followed by an upper.
  # E.g. URLLoaderFactory -> URLLoader_Factory
  identifier = re.sub('([a-z][0-9]*)(?=[A-Z])', r'\1_', identifier)
  return [x.lower() for x in identifier.split('_')]

def ToCamel(identifier, lower_initial=False, dilimiter='_'):
  """Splits |identifier| using |dilimiter|, makes the first character of each
  word uppercased (but makes the first character of the first word lowercased
  if |lower_initial| is set to True), and joins the words. Please note that for
  each word, all the characters except the first one are untouched.
  """
  result = ''.join(word[0].upper() + word[1:]
      for word in identifier.split(dilimiter) if word)
  if lower_initial and result:
    result = result[0].lower() + result[1:]
  return result

def ToConstantCase(identifier):
  """Splits camel-cased |identifier| into lower case words, removes the first
  word if it's "k" and joins them using "_" e.g. for "URLLoaderFactory", returns
  "URL_LOADER_FACTORY".
  """
  words = SplitCamelCase(identifier)
  if words[0] == 'k' and len(words) > 1:
    words = words[1:]

  # Variables cannot start with a digit
  if (words[0][0].isdigit()):
    words[0] = '_' + words[0]

  return '_'.join([word.upper() for word in words])

class Stylizer(object):
  """Stylizers specify naming rules to map mojom names to names in generated
  code. For example, if you would like method_name in mojom to be mapped to
  MethodName in the generated code, you need to define a subclass of Stylizer
  and override StylizeMethod to do the conversion."""

  def StylizeConstant(self, mojom_name):
    return mojom_name

  def StylizeField(self, mojom_name):
    return mojom_name

  def StylizeStruct(self, mojom_name):
    return mojom_name

  def StylizeUnion(self, mojom_name):
    return mojom_name

  def StylizeParameter(self, mojom_name):
    return mojom_name

  def StylizeMethod(self, mojom_name):
    return mojom_name

  def StylizeInterface(self, mojom_name):
    return mojom_name

  def StylizeEnumField(self, mojom_name):
    return mojom_name

  def StylizeEnum(self, mojom_name):
    return mojom_name

  def StylizeModule(self, mojom_namespace):
    return mojom_namespace


def WriteFile(contents, full_path):
  # If |contents| is same with the file content, we skip updating.
  if os.path.isfile(full_path):
    with open(full_path, 'rb') as destination_file:
      if destination_file.read() == contents:
        return

  # Make sure the containing directory exists.
  full_dir = os.path.dirname(full_path)
  fileutil.EnsureDirectoryExists(full_dir)

  # Dump the data to disk.
  with open(full_path, "wb") as f:
    f.write(contents)


def AddComputedData(module):
  """Adds computed data to the given module. The data is computed once and
  used repeatedly in the generation process."""

  def _AddStructComputedData(exported, struct):
    struct.packed = pack.PackedStruct(struct)
    struct.bytes = pack.GetByteLayout(struct.packed)
    struct.versions = pack.GetVersionInfo(struct.packed)
    struct.exported = exported

  def _AddUnionComputedData(union):
    ordinal = 0
    for field in union.fields:
      if field.ordinal is not None:
        ordinal = field.ordinal
      field.ordinal = ordinal
      ordinal += 1

  def _AddInterfaceComputedData(interface):
    next_ordinal = 0
    interface.version = 0
    for method in interface.methods:
      if method.ordinal is None:
        method.ordinal = next_ordinal
      # this field is never scrambled
      method.sequential_ordinal = next_ordinal
      next_ordinal = method.ordinal + 1

      if method.min_version is not None:
        interface.version = max(interface.version, method.min_version)

      method.param_struct = _GetStructFromMethod(method)
      interface.version = max(interface.version,
                              method.param_struct.versions[-1].version)

      if method.response_parameters is not None:
        method.response_param_struct = _GetResponseStructFromMethod(method)
        interface.version = max(
            interface.version,
            method.response_param_struct.versions[-1].version)
      else:
        method.response_param_struct = None

  def _GetStructFromMethod(method):
    """Converts a method's parameters into the fields of a struct."""
    params_class = "%s_%s_Params" % (method.interface.mojom_name,
                                     method.mojom_name)
    struct = mojom.Struct(params_class, module=method.interface.module)
    for param in method.parameters:
      struct.AddField(param.mojom_name, param.kind, param.ordinal,
                      attributes=param.attributes)
    _AddStructComputedData(False, struct)
    return struct

  def _GetResponseStructFromMethod(method):
    """Converts a method's response_parameters into the fields of a struct."""
    params_class = "%s_%s_ResponseParams" % (method.interface.mojom_name,
                                             method.mojom_name)
    struct = mojom.Struct(params_class, module=method.interface.module)
    for param in method.response_parameters:
      struct.AddField(param.mojom_name, param.kind, param.ordinal,
                      attributes=param.attributes)
    _AddStructComputedData(False, struct)
    return struct

  for struct in module.structs:
    _AddStructComputedData(True, struct)
  for union in module.unions:
    _AddUnionComputedData(union)
  for interface in module.interfaces:
    _AddInterfaceComputedData(interface)


class Generator(object):
  # Pass |output_dir| to emit files to disk. Omit |output_dir| to echo all
  # files to stdout.
  def __init__(self, module, output_dir=None, typemap=None, variant=None,
               bytecode_path=None, for_blink=False,
               js_bindings_mode="new",
               js_generate_struct_deserializers=False,
               export_attribute=None,
               export_header=None, generate_non_variant_code=False,
               support_lazy_serialization=False, disallow_native_types=False,
               disallow_interfaces=False, generate_message_ids=False,
               generate_fuzzing=False, enable_kythe_annotations=False):
    self.module = module
    self.output_dir = output_dir
    self.typemap = typemap or {}
    self.variant = variant
    self.bytecode_path = bytecode_path
    self.for_blink = for_blink
    self.js_bindings_mode = js_bindings_mode
    self.js_generate_struct_deserializers = js_generate_struct_deserializers
    self.export_attribute = export_attribute
    self.export_header = export_header
    self.generate_non_variant_code = generate_non_variant_code
    self.support_lazy_serialization = support_lazy_serialization
    self.disallow_native_types = disallow_native_types
    self.disallow_interfaces = disallow_interfaces
    self.generate_message_ids = generate_message_ids
    self.generate_fuzzing = generate_fuzzing
    self.enable_kythe_annotations = enable_kythe_annotations

  def Write(self, contents, filename):
    if self.output_dir is None:
      print(contents)
      return
    full_path = os.path.join(self.output_dir, filename)
    WriteFile(contents, full_path)

  def WriteWithComment(self, contents, filename):
    generator_name = "mojom_bindings_generator.py"
    comment = r"// %s is auto generated by %s, do not edit" % (filename,
                                                               generator_name)

    contents = comment + '\n' + '\n' + contents;
    self.Write(contents, filename)

  def GenerateFiles(self, args):
    raise NotImplementedError("Subclasses must override/implement this method")

  def GetJinjaParameters(self):
    """Returns default constructor parameters for the jinja environment."""
    return {}

  def GetGlobals(self):
    """Returns global mappings for the template generation."""
    return {}
