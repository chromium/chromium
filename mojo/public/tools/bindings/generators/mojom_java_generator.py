# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates java source files from a mojom.Module."""

import argparse
import ast
import contextlib
import os
import re
import shutil
import sys
import tempfile

from jinja2 import contextfilter

import mojom.fileutil as fileutil
import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir,
                             os.pardir, os.pardir, os.pardir, os.pardir,
                             'build', 'android', 'gyp'))
from util import build_utils


GENERATOR_PREFIX = 'java'

_spec_to_java_type = {
  mojom.BOOL.spec: 'boolean',
  mojom.DCPIPE.spec: 'org.chromium.mojo.system.DataPipe.ConsumerHandle',
  mojom.DOUBLE.spec: 'double',
  mojom.DPPIPE.spec: 'org.chromium.mojo.system.DataPipe.ProducerHandle',
  mojom.FLOAT.spec: 'float',
  mojom.HANDLE.spec: 'org.chromium.mojo.system.UntypedHandle',
  mojom.INT16.spec: 'short',
  mojom.INT32.spec: 'int',
  mojom.INT64.spec: 'long',
  mojom.INT8.spec: 'byte',
  mojom.MSGPIPE.spec: 'org.chromium.mojo.system.MessagePipeHandle',
  mojom.NULLABLE_DCPIPE.spec:
      'org.chromium.mojo.system.DataPipe.ConsumerHandle',
  mojom.NULLABLE_DPPIPE.spec:
      'org.chromium.mojo.system.DataPipe.ProducerHandle',
  mojom.NULLABLE_HANDLE.spec: 'org.chromium.mojo.system.UntypedHandle',
  mojom.NULLABLE_MSGPIPE.spec: 'org.chromium.mojo.system.MessagePipeHandle',
  mojom.NULLABLE_SHAREDBUFFER.spec:
      'org.chromium.mojo.system.SharedBufferHandle',
  mojom.NULLABLE_STRING.spec: 'String',
  mojom.SHAREDBUFFER.spec: 'org.chromium.mojo.system.SharedBufferHandle',
  mojom.STRING.spec: 'String',
  mojom.UINT16.spec: 'short',
  mojom.UINT32.spec: 'int',
  mojom.UINT64.spec: 'long',
  mojom.UINT8.spec: 'byte',
}

_spec_to_decode_method = {
  mojom.BOOL.spec:                  'readBoolean',
  mojom.DCPIPE.spec:                'readConsumerHandle',
  mojom.DOUBLE.spec:                'readDouble',
  mojom.DPPIPE.spec:                'readProducerHandle',
  mojom.FLOAT.spec:                 'readFloat',
  mojom.HANDLE.spec:                'readUntypedHandle',
  mojom.INT16.spec:                 'readShort',
  mojom.INT32.spec:                 'readInt',
  mojom.INT64.spec:                 'readLong',
  mojom.INT8.spec:                  'readByte',
  mojom.MSGPIPE.spec:               'readMessagePipeHandle',
  mojom.NULLABLE_DCPIPE.spec:       'readConsumerHandle',
  mojom.NULLABLE_DPPIPE.spec:       'readProducerHandle',
  mojom.NULLABLE_HANDLE.spec:       'readUntypedHandle',
  mojom.NULLABLE_MSGPIPE.spec:      'readMessagePipeHandle',
  mojom.NULLABLE_SHAREDBUFFER.spec: 'readSharedBufferHandle',
  mojom.NULLABLE_STRING.spec:       'readString',
  mojom.SHAREDBUFFER.spec:          'readSharedBufferHandle',
  mojom.STRING.spec:                'readString',
  mojom.UINT16.spec:                'readShort',
  mojom.UINT32.spec:                'readInt',
  mojom.UINT64.spec:                'readLong',
  mojom.UINT8.spec:                 'readByte',
}

_java_primitive_to_boxed_type = {
  'boolean': 'Boolean',
  'byte':    'Byte',
  'double':  'Double',
  'float':   'Float',
  'int':     'Integer',
  'long':    'Long',
  'short':   'Short',
}

_java_reserved_types = [
  # These two may clash with commonly used classes on Android.
  'Manifest',
  'R'
]

def UpperCamelCase(name):
  return ''.join([x.capitalize() for x in generator.SplitCamelCase(name)])

def CamelCase(name):
  uccc = UpperCamelCase(name)
  return uccc[0].lower() + uccc[1:]

def ConstantStyle(name):
  return generator.ToConstantCase(name)

def GetNameForElement(element):
  if (mojom.IsEnumKind(element) or mojom.IsInterfaceKind(element) or
      mojom.IsStructKind(element) or mojom.IsUnionKind(element)):
    name = UpperCamelCase(element.name)
    if name in _java_reserved_types:
      return name + '_'
    return name
  if (mojom.IsInterfaceRequestKind(element) or
      mojom.IsAssociatedKind(element) or mojom.IsPendingRemoteKind(element) or
      mojom.IsPendingReceiverKind(element)):
    return GetNameForElement(element.kind)
  if isinstance(element, (mojom.Method,
                          mojom.Parameter,
                          mojom.Field)):
    return CamelCase(element.name)
  if isinstance(element,  mojom.EnumValue):
    return (GetNameForElement(element.enum) + '.' +
            ConstantStyle(element.name))
  if isinstance(element, (mojom.NamedValue,
                          mojom.Constant,
                          mojom.EnumField)):
    return ConstantStyle(element.name)
  raise Exception('Unexpected element: %s' % element)

def GetInterfaceResponseName(method):
  return UpperCamelCase(method.name + '_Response')

def ParseStringAttribute(attribute):
  assert isinstance(attribute, basestring)
  return attribute

def GetJavaTrueFalse(value):
  return 'true' if value else 'false'

def GetArrayNullabilityFlags(kind):
    """Returns nullability flags for an array type, see Decoder.java.

    As we have dedicated decoding functions for arrays, we have to pass
    nullability information about both the array itself, as well as the array
    element type there.
    """
    assert mojom.IsArrayKind(kind)
    ARRAY_NULLABLE   = \
        'org.chromium.mojo.bindings.BindingsHelper.ARRAY_NULLABLE'
    ELEMENT_NULLABLE = \
        'org.chromium.mojo.bindings.BindingsHelper.ELEMENT_NULLABLE'
    NOTHING_NULLABLE = \
        'org.chromium.mojo.bindings.BindingsHelper.NOTHING_NULLABLE'

    flags_to_set = []
    if mojom.IsNullableKind(kind):
        flags_to_set.append(ARRAY_NULLABLE)
    if mojom.IsNullableKind(kind.kind):
        flags_to_set.append(ELEMENT_NULLABLE)

    if not flags_to_set:
        flags_to_set = [NOTHING_NULLABLE]
    return ' | '.join(flags_to_set)


def AppendEncodeDecodeParams(initial_params, context, kind, bit):
  """ Appends standard parameters shared between encode and decode calls. """
  params = list(initial_params)
  if (kind == mojom.BOOL):
    params.append(str(bit))
  if mojom.IsReferenceKind(kind):
    if mojom.IsArrayKind(kind):
      params.append(GetArrayNullabilityFlags(kind))
    else:
      params.append(GetJavaTrueFalse(mojom.IsNullableKind(kind)))
  if mojom.IsArrayKind(kind):
    params.append(GetArrayExpectedLength(kind))
  if mojom.IsInterfaceKind(kind):
    params.append('%s.MANAGER' % GetJavaType(context, kind))
  if mojom.IsPendingRemoteKind(kind):
    params.append('%s.MANAGER' % GetJavaType(context, kind.kind))
  if mojom.IsArrayKind(kind) and mojom.IsInterfaceKind(kind.kind):
    params.append('%s.MANAGER' % GetJavaType(context, kind.kind))
  if mojom.IsArrayKind(kind) and mojom.IsPendingRemoteKind(kind.kind):
    params.append('%s.MANAGER' % GetJavaType(context, kind.kind.kind))
  return params


@contextfilter
def DecodeMethod(context, kind, offset, bit):
  def _DecodeMethodName(kind):
    if mojom.IsArrayKind(kind):
      return _DecodeMethodName(kind.kind) + 's'
    if mojom.IsEnumKind(kind):
      return _DecodeMethodName(mojom.INT32)
    if mojom.IsInterfaceRequestKind(kind) or mojom.IsPendingReceiverKind(kind):
      return 'readInterfaceRequest'
    if mojom.IsInterfaceKind(kind) or mojom.IsPendingRemoteKind(kind):
      return 'readServiceInterface'
    if (mojom.IsAssociatedInterfaceRequestKind(kind) or
        mojom.IsPendingAssociatedReceiverKind(kind)):
      return 'readAssociatedInterfaceRequestNotSupported'
    if (mojom.IsAssociatedInterfaceKind(kind) or
        mojom.IsPendingAssociatedRemoteKind(kind)):
      return 'readAssociatedServiceInterfaceNotSupported'
    return _spec_to_decode_method[kind.spec]
  methodName = _DecodeMethodName(kind)
  params = AppendEncodeDecodeParams([ str(offset) ], context, kind, bit)
  return '%s(%s)' % (methodName, ', '.join(params))

@contextfilter
def EncodeMethod(context, kind, variable, offset, bit):
  params = AppendEncodeDecodeParams(
      [ variable, str(offset) ], context, kind, bit)
  return 'encode(%s)' % ', '.join(params)

def GetPackage(module):
  if module.attributes and 'JavaPackage' in module.attributes:
    return ParseStringAttribute(module.attributes['JavaPackage'])
  # Default package.
  if module.namespace:
    return 'org.chromium.' + module.namespace
  return 'org.chromium'

def GetNameForKind(context, kind):
  def _GetNameHierachy(kind):
    hierachy = []
    if kind.parent_kind:
      hierachy = _GetNameHierachy(kind.parent_kind)
    hierachy.append(GetNameForElement(kind))
    return hierachy

  module = context.resolve('module')
  elements = []
  if GetPackage(module) != GetPackage(kind.module):
    elements += [GetPackage(kind.module)]
  elements += _GetNameHierachy(kind)
  return '.'.join(elements)

@contextfilter
def GetJavaClassForEnum(context, kind):
  return GetNameForKind(context, kind)

def GetBoxedJavaType(context, kind, with_generics=True):
  unboxed_type = GetJavaType(context, kind, False, with_generics)
  if unboxed_type in _java_primitive_to_boxed_type:
    return _java_primitive_to_boxed_type[unboxed_type]
  return unboxed_type

@contextfilter
def GetJavaType(context, kind, boxed=False, with_generics=True):
  if boxed:
    return GetBoxedJavaType(context, kind)
  if (mojom.IsStructKind(kind) or
      mojom.IsInterfaceKind(kind) or
      mojom.IsUnionKind(kind)):
    return GetNameForKind(context, kind)
  if mojom.IsPendingRemoteKind(kind):
    return GetNameForKind(context, kind.kind)
  if mojom.IsInterfaceRequestKind(kind) or mojom.IsPendingReceiverKind(kind):
    return ('org.chromium.mojo.bindings.InterfaceRequest<%s>' %
            GetNameForKind(context, kind.kind))
  if (mojom.IsAssociatedInterfaceKind(kind) or
      mojom.IsPendingAssociatedRemoteKind(kind)):
    return 'org.chromium.mojo.bindings.AssociatedInterfaceNotSupported'
  if (mojom.IsAssociatedInterfaceRequestKind(kind) or
      mojom.IsPendingAssociatedReceiverKind(kind)):
    return 'org.chromium.mojo.bindings.AssociatedInterfaceRequestNotSupported'
  if mojom.IsMapKind(kind):
    if with_generics:
      return 'java.util.Map<%s, %s>' % (
          GetBoxedJavaType(context, kind.key_kind),
          GetBoxedJavaType(context, kind.value_kind))
    else:
      return 'java.util.Map'
  if mojom.IsArrayKind(kind):
    return '%s[]' % GetJavaType(context, kind.kind, boxed, with_generics)
  if mojom.IsEnumKind(kind):
    return 'int'
  return _spec_to_java_type[kind.spec]

@contextfilter
def DefaultValue(context, field):
  assert field.default
  if isinstance(field.kind, mojom.Struct):
    assert field.default == 'default'
    return 'new %s()' % GetJavaType(context, field.kind)
  return '(%s) %s' % (
      GetJavaType(context, field.kind),
      ExpressionToText(context, field.default, kind_spec=field.kind.spec))

@contextfilter
def ConstantValue(context, constant):
  return '(%s) %s' % (
      GetJavaType(context, constant.kind),
      ExpressionToText(context, constant.value, kind_spec=constant.kind.spec))

@contextfilter
def NewArray(context, kind, size):
  if mojom.IsArrayKind(kind.kind):
    return NewArray(context, kind.kind, size) + '[]'
  return 'new %s[%s]' % (
      GetJavaType(context, kind.kind, boxed=False, with_generics=False), size)

@contextfilter
def ExpressionToText(context, token, kind_spec=''):
  def _TranslateNamedValue(named_value):
    entity_name = GetNameForElement(named_value)
    if named_value.parent_kind:
      return GetJavaType(context, named_value.parent_kind) + '.' + entity_name
    # Handle the case where named_value is a module level constant:
    if not isinstance(named_value, mojom.EnumValue):
      entity_name = (GetConstantsMainEntityName(named_value.module) + '.' +
                      entity_name)
    if GetPackage(named_value.module) == GetPackage(context.resolve('module')):
      return entity_name
    return GetPackage(named_value.module) + '.' + entity_name

  if isinstance(token, mojom.NamedValue):
    return _TranslateNamedValue(token)
  if kind_spec.startswith('i') or kind_spec.startswith('u'):
    number = ast.literal_eval(token.lstrip('+ '))
    if not isinstance(number, (int, long)):
      raise ValueError('got unexpected type %r for int literal %r' % (
          type(number), token))
    # If the literal is too large to fit a signed long, convert it to the
    # equivalent signed long.
    if number >= 2 ** 63:
      number -= 2 ** 64
    if number < 2 ** 31 and number >= -2 ** 31:
      return '%d' % number
    return '%dL' % number
  if isinstance(token, mojom.BuiltinValue):
    if token.value == 'double.INFINITY':
      return 'java.lang.Double.POSITIVE_INFINITY'
    if token.value == 'double.NEGATIVE_INFINITY':
      return 'java.lang.Double.NEGATIVE_INFINITY'
    if token.value == 'double.NAN':
      return 'java.lang.Double.NaN'
    if token.value == 'float.INFINITY':
      return 'java.lang.Float.POSITIVE_INFINITY'
    if token.value == 'float.NEGATIVE_INFINITY':
      return 'java.lang.Float.NEGATIVE_INFINITY'
    if token.value == 'float.NAN':
      return 'java.lang.Float.NaN'
  return token

def GetArrayKind(kind, size = None):
  if size is None:
    return mojom.Array(kind)
  else:
    array = mojom.Array(kind, 0)
    array.java_map_size = size
    return array

def GetArrayExpectedLength(kind):
  if mojom.IsArrayKind(kind) and kind.length is not None:
    return getattr(kind, 'java_map_size', str(kind.length))
  else:
    return 'org.chromium.mojo.bindings.BindingsHelper.UNSPECIFIED_ARRAY_LENGTH'

def IsPointerArrayKind(kind):
  if not mojom.IsArrayKind(kind):
    return False
  sub_kind = kind.kind
  return mojom.IsObjectKind(sub_kind) and not mojom.IsUnionKind(sub_kind)

def IsUnionArrayKind(kind):
  if not mojom.IsArrayKind(kind):
    return False
  sub_kind = kind.kind
  return mojom.IsUnionKind(sub_kind)

def GetConstantsMainEntityName(module):
  if module.attributes and 'JavaConstantsClassName' in module.attributes:
    return ParseStringAttribute(module.attributes['JavaConstantsClassName'])
  # This constructs the name of the embedding classes for module level constants
  # by extracting the mojom's filename and prepending it to Constants.
  return (UpperCamelCase(module.path.split('/')[-1].rsplit('.', 1)[0]) +
          'Constants')

def GetMethodOrdinalName(method):
  return ConstantStyle(method.name) + '_ORDINAL'

def HasMethodWithResponse(interface):
  for method in interface.methods:
    if method.response_parameters is not None:
      return True
  return False

def HasMethodWithoutResponse(interface):
  for method in interface.methods:
    if method.response_parameters is None:
      return True
  return False

@contextlib.contextmanager
def TempDir():
  dirname = tempfile.mkdtemp()
  try:
    yield dirname
  finally:
    shutil.rmtree(dirname)

def EnumCoversContinuousRange(kind):
  if not kind.fields:
    return False
  number_of_unique_keys = len(set(map(
      lambda field: field.numeric_value, kind.fields)))
  if kind.max_value - kind.min_value + 1 != number_of_unique_keys:
    return False
  return True

class Generator(generator.Generator):
  def _GetJinjaExports(self):
    return {
      'package': GetPackage(self.module),
    }

  @staticmethod
  def GetTemplatePrefix():
    return "java_templates"

  def GetFilters(self):
    java_filters = {
      'array_expected_length': GetArrayExpectedLength,
      'array': GetArrayKind,
      'constant_value': ConstantValue,
      'covers_continuous_range': EnumCoversContinuousRange,
      'decode_method': DecodeMethod,
      'default_value': DefaultValue,
      'encode_method': EncodeMethod,
      'expression_to_text': ExpressionToText,
      'has_method_without_response': HasMethodWithoutResponse,
      'has_method_with_response': HasMethodWithResponse,
      'interface_response_name': GetInterfaceResponseName,
      'is_array_kind': mojom.IsArrayKind,
      'is_any_handle_kind': mojom.IsAnyHandleKind,
      "is_enum_kind": mojom.IsEnumKind,
      'is_interface_request_kind': mojom.IsInterfaceRequestKind,
      'is_map_kind': mojom.IsMapKind,
      'is_nullable_kind': mojom.IsNullableKind,
      'is_pointer_array_kind': IsPointerArrayKind,
      'is_reference_kind': mojom.IsReferenceKind,
      'is_struct_kind': mojom.IsStructKind,
      'is_union_array_kind': IsUnionArrayKind,
      'is_union_kind': mojom.IsUnionKind,
      'java_class_for_enum': GetJavaClassForEnum,
      'java_true_false': GetJavaTrueFalse,
      'java_type': GetJavaType,
      'method_ordinal_name': GetMethodOrdinalName,
      'name': GetNameForElement,
      'new_array': NewArray,
      'ucc': lambda x: UpperCamelCase(x.name),
    }
    return java_filters

  def _GetJinjaExportsForInterface(self, interface):
    exports = self._GetJinjaExports()
    exports.update({'interface': interface})
    return exports

  @UseJinja('enum.java.tmpl')
  def _GenerateEnumSource(self, enum):
    exports = self._GetJinjaExports()
    exports.update({'enum': enum})
    return exports

  @UseJinja('struct.java.tmpl')
  def _GenerateStructSource(self, struct):
    exports = self._GetJinjaExports()
    exports.update({'struct': struct})
    return exports

  @UseJinja('union.java.tmpl')
  def _GenerateUnionSource(self, union):
    exports = self._GetJinjaExports()
    exports.update({'union': union})
    return exports

  @UseJinja('interface.java.tmpl')
  def _GenerateInterfaceSource(self, interface):
    return self._GetJinjaExportsForInterface(interface)

  @UseJinja('interface_internal.java.tmpl')
  def _GenerateInterfaceInternalSource(self, interface):
    return self._GetJinjaExportsForInterface(interface)

  @UseJinja('constants.java.tmpl')
  def _GenerateConstantsSource(self, module):
    exports = self._GetJinjaExports()
    exports.update({'main_entity': GetConstantsMainEntityName(module),
                    'constants': module.constants})
    return exports

  def _DoGenerateFiles(self):
    fileutil.EnsureDirectoryExists(self.output_dir)

    for struct in self.module.structs:
      self.WriteWithComment(self._GenerateStructSource(struct),
                            '%s.java' % GetNameForElement(struct))

    for union in self.module.unions:
      self.WriteWithComment(self._GenerateUnionSource(union),
                            '%s.java' % GetNameForElement(union))

    for enum in self.module.enums:
      self.WriteWithComment(self._GenerateEnumSource(enum),
                            '%s.java' % GetNameForElement(enum))

    for interface in self.module.interfaces:
      self.WriteWithComment(self._GenerateInterfaceSource(interface),
                            '%s.java' % GetNameForElement(interface))
      self.WriteWithComment(self._GenerateInterfaceInternalSource(interface),
                            '%s_Internal.java' % GetNameForElement(interface))

    if self.module.constants:
      self.WriteWithComment(self._GenerateConstantsSource(self.module),
                            '%s.java' % GetConstantsMainEntityName(self.module))

  def GenerateFiles(self, unparsed_args):
    # TODO(rockot): Support variant output for Java.
    if self.variant:
      raise Exception("Variants not supported in Java bindings.")

    self.module.Stylize(generator.Stylizer())

    parser = argparse.ArgumentParser()
    parser.add_argument('--java_output_directory', dest='java_output_directory')
    args = parser.parse_args(unparsed_args)
    package_path = GetPackage(self.module).replace('.', '/')

    # Generate the java files in a temporary directory and place a single
    # srcjar in the output directory.
    basename = "%s.srcjar" % self.module.path
    zip_filename = os.path.join(self.output_dir, basename)
    with TempDir() as temp_java_root:
      self.output_dir = os.path.join(temp_java_root, package_path)
      self._DoGenerateFiles();
      build_utils.ZipDir(zip_filename, temp_java_root)

    if args.java_output_directory:
      # If requested, generate the java files directly into indicated directory.
      self.output_dir = os.path.join(args.java_output_directory, package_path)
      self._DoGenerateFiles();

  def GetJinjaParameters(self):
    return {
      'lstrip_blocks': True,
      'trim_blocks': True,
    }

  def GetGlobals(self):
    return {
      'namespace': self.module.namespace,
      'module': self.module,
    }
