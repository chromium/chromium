#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""IDL to Fuzzilli profile generator.

A Fuzzilli profile [1] describes how to fuzz a particular target. For example,
it describes which builtins are available to JS running in that target, and all
interesting functions and types that Fuzzilli would benefit from knowing.
This helps the fuzzing engine generate interesting JS programs without having
to discover how to e.g. create a DOM node through coverage-guided trial and
error.

This tool outputs a Swift file defining a profile for JS running in Chrome. The
core of this is defining `ILType` and `ObjectGroup` variables describing all
known types. See Fuzzilli's `TypeSystem.swift` [2] for more details.

This output file is automatically derived from the contents of all WebIDL files
known to Chrome. Those files describe all JS interfaces exposed to websites.
The meat of this tool is converting from IDL types to Fuzzilli IL types.

[1] https://github.com/googleprojectzero/fuzzilli/blob/main/Sources/FuzzilliCli\
/Profiles/Profile.swift
[2] https://github.com/googleprojectzero/fuzzilli/blob/main/Sources/Fuzzilli/\
FuzzIL/TypeSystem.swift.
"""

from __future__ import annotations

import argparse
import functools
import os
import sys
from typing import List, Optional, Dict, Tuple, Union, Sequence

import dataclasses  # Built-in, but pylint treats it as a third party module.


def _GetDirAbove(dirname: str):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    if not tail:
      return None
    if tail == dirname:
      return path


SOURCE_DIR = _GetDirAbove('testing')

sys.path.insert(1, os.path.join(SOURCE_DIR, 'third_party'))
sys.path.append(os.path.join(SOURCE_DIR, 'build'))
sys.path.append(
    os.path.join(SOURCE_DIR, 'third_party/blink/renderer/bindings/scripts/'))

import jinja2
import web_idl


class SwiftExpression:
  """Generic type for representing a Swift type."""

  def fuzzilli_repr(self) -> str:
    """Returns the Fuzzilli representation of this expression.

    Returns:
        the string representation of this expression.
    """
    raise Exception('Not implemented.')


class SwiftNil(SwiftExpression):
  """Swift nil value."""

  def fuzzilli_repr(self) -> str:
    return 'nil'


@dataclasses.dataclass
class StringLiteral(SwiftExpression):
  """Represents a Swift string literal."""
  value: str

  def fuzzilli_repr(self) -> str:
    return f'"{self.value}"'


@dataclasses.dataclass
class LiteralList(SwiftExpression):
  """Represents a literal Swift list. """
  values: List[SwiftExpression]

  def fuzzilli_repr(self) -> str:
    values = ', '.join([v.fuzzilli_repr() for v in self.values])
    return f'[{values}]'


@dataclasses.dataclass
class Add(SwiftExpression):
  lhs: SwiftExpression
  rhs: SwiftExpression

  def fuzzilli_repr(self):
    return f'{self.lhs.fuzzilli_repr()} + {self.rhs.fuzzilli_repr()}'


@dataclasses.dataclass
class Or(SwiftExpression):
  lhs: SwiftExpression
  rhs: SwiftExpression

  def fuzzilli_repr(self):
    return f'{self.lhs.fuzzilli_repr()} | {self.rhs.fuzzilli_repr()}'


@dataclasses.dataclass
class ILType(SwiftExpression):
  """Represents the Fuzzilli 'ILType' Swift type."""
  property: str
  args: Optional[List[SwiftExpression]] = None
  kwargs: Optional[Dict[str, SwiftExpression]] = None

  def fuzzilli_repr(self) -> str:
    if self.args is None and self.kwargs is None:
      return f'ILType.{self.property}'
    arg_s = ''
    if self.args:
      arg_s += ', '.join([a.fuzzilli_repr() for a in self.args])
    if self.kwargs:
      arg_s += ', '.join(
          [f'{k}: {v.fuzzilli_repr()}' for k, v in self.kwargs.items()])
    return f'ILType.{self.property}({arg_s})'

  @staticmethod
  def nothing() -> ILType:
    return ILType(property='nothing')

  @staticmethod
  def anything() -> ILType:
    return ILType(property='anything')

  @staticmethod
  def undefined() -> ILType:
    return ILType(property='undefined')

  @staticmethod
  def integer() -> ILType:
    return ILType(property='integer')

  @staticmethod
  def float() -> ILType:
    return ILType(property='float')

  @staticmethod
  def bigint() -> ILType:
    return ILType(property='bigint')

  @staticmethod
  def boolean() -> ILType:
    return ILType(property='boolean')

  @staticmethod
  def iterable() -> ILType:
    return ILType(property='iterable')

  @staticmethod
  def string() -> ILType:
    return ILType(property='string')

  @staticmethod
  def jsString() -> ILType:
    return ILType(property='jsString')

  @staticmethod
  def jsPromise() -> ILType:
    return ILType(property='jsPromise')

  @staticmethod
  def jsArrayBuffer() -> ILType:
    return ILType(property='jsArrayBuffer')

  @staticmethod
  def jsSharedArrayBuffer() -> ILType:
    return ILType(property='jsSharedArrayBuffer')

  @staticmethod
  def jsDataView() -> ILType:
    return ILType(property='jsDataView')

  @staticmethod
  def jsMap() -> ILType:
    return ILType(property='jsMap')

  @staticmethod
  def refType(name: str) -> ILType:
    return ILType(property=name)

  @staticmethod
  def jsTypedArray(variant: str) -> ILType:
    return ILType(property='jsTypedArray',
                  args=[StringLiteral(value=variant)],
                  kwargs=None)

  @staticmethod
  def function(signature: SignatureType) -> ILType:
    return ILType(property='function', args=[signature], kwargs=None)

  @staticmethod
  def object(group: Optional[str] = None,
             props: Optional[List[str]] = None,
             methods: Optional[List[str]] = None) -> ILType:
    if not group and not props and not methods:
      return ILType(property='object', args=[])

    props_literals = [StringLiteral(value=prop) for prop in props]
    methods_literals = [StringLiteral(value=method) for method in methods]
    props_list = LiteralList(values=props_literals)
    methods_list = LiteralList(values=methods_literals)
    group_val = StringLiteral(value=group) if group else SwiftNil()
    kwargs = {
        'ofGroup': group_val,
        'withProperties': props_list,
        'withMethods': methods_list,
    }
    return ILType(property='object', kwargs=kwargs)

  @staticmethod
  def constructor(signature: SignatureType) -> ILType:
    return ILType(property='constructor', args=[signature])

  def __add__(self, other: ILType):
    return Add(self, other)

  def __or__(self, other: ILType):
    return Or(self, other)


SIMPLE_TYPE_TO_ILTYPE = {
    'void': ILType.undefined(),
    'object': ILType.object(),
    'undefined': ILType.undefined(),
    'any': ILType.anything(),
    'byte': ILType.integer(),
    'octet': ILType.integer(),
    'short': ILType.integer(),
    'unsigned short': ILType.integer(),
    'long': ILType.integer(),
    'unsigned long': ILType.integer(),
    'long long': ILType.integer(),
    'unsigned long long': ILType.integer(),
    'integer': ILType.integer(),
    'float': ILType.float(),
    'double': ILType.float(),
    'unrestricted float': ILType.float(),
    'unrestricted double': ILType.float(),
    'bigint': ILType.bigint(),
    'boolean': ILType.boolean(),
    'DOMString': ILType.string(),
    'ByteString': ILType.string(),
    'USVString': ILType.string(),
    'ArrayBuffer': ILType.jsArrayBuffer(),
    'ArrayBufferView': ILType.jsDataView(),
    'SharedArray': ILType.jsSharedArrayBuffer(),
    'Int8Array': ILType.jsTypedArray('Int8Array'),
    'Int16Array': ILType.jsTypedArray('Int16Array'),
    'Int32Array': ILType.jsTypedArray('Int32Array'),
    'Uint8Array': ILType.jsTypedArray('Uint8Array'),
    'Uint16Array': ILType.jsTypedArray('Uint16Array'),
    'Uint32Array': ILType.jsTypedArray('Uint32Array'),
    'Uint8ClampedArray': ILType.jsTypedArray('Uint8ClampedArray'),
    'BigInt64Array': ILType.jsTypedArray('BigInt64Array'),
    'BigUint64Array': ILType.jsTypedArray('BigUint64Array'),
    'Float16Array': ILType.jsTypedArray('Float16Array'),
    'Float32Array': ILType.jsTypedArray('Float32Array'),
    'Float64Array': ILType.jsTypedArray('Float64Array'),
    'DataView': ILType.jsDataView(),
}


@dataclasses.dataclass
class ParameterType(SwiftExpression):
  """Represents the Fuzzilli 'Parameter' Swift type."""
  property: str
  arg: ILType

  def fuzzilli_repr(self) -> str:
    return f'Parameter.{self.property}({self.arg.fuzzilli_repr()})'

  @staticmethod
  def opt(inner_type: ILType):
    return ParameterType(property='opt', arg=inner_type)

  @staticmethod
  def plain(inner_type: ILType):
    return ParameterType(property='plain', arg=inner_type)

  @staticmethod
  def rest(inner_type: ILType):
    return ParameterType(property='rest', arg=inner_type)


@dataclasses.dataclass
class SignatureType(SwiftExpression):
  """Represents the Fuzzilli 'Signature' Swift type."""
  args: List[ILType]
  ret: ILType

  def fuzzilli_repr(self):
    args = self.args if self.args else []
    args_repr = LiteralList(values=args).fuzzilli_repr()
    ret_repr = self.ret.fuzzilli_repr()
    return f'Signature(expects: {args_repr}, returns: {ret_repr})'


@dataclasses.dataclass()
class ObjectGroup(SwiftExpression):
  """Represents the Fuzzilli ObjectGroup swift object."""
  name: str
  instanceType: ILType
  properties: Dict[str, ILType]
  methods: Dict[str, ILType]


def idl_type_to_iltype(idl_type: web_idl.idl_type.IdlType) -> ILType:
  """Converts a WebIDL type to a Fuzzilli ILType.

  Args:
      idl_type: the idl type to parse.

  Raises:
      whether a type was not handled, used for debugging purposes.

  Returns:
      the equivalent Fuzzilli ILType that was parsed.
  """
  if isinstance(idl_type, web_idl.idl_type.SimpleType):
    return SIMPLE_TYPE_TO_ILTYPE[idl_type.keyword_typename]
  if isinstance(idl_type, web_idl.idl_type.ReferenceType):
    return ILType.refType(f'js{idl_type.identifier}')
  if isinstance(idl_type, web_idl.idl_type.UnionType):
    members = [idl_type_to_iltype(t) for t in idl_type.member_types]
    return functools.reduce(ILType.__or__, members)
  if isinstance(idl_type, web_idl.idl_type.NullableType):
    return idl_type_to_iltype(idl_type.inner_type)
  if web_idl.idl_type.IsArrayLike(idl_type):
    return ILType.iterable()
  if isinstance(idl_type, web_idl.idl_type.PromiseType):
    return ILType.jsPromise()
  if isinstance(idl_type, web_idl.idl_type.RecordType):
    return ILType.jsMap()

  raise TypeError(f'Unhandled IdlType {repr(idl_type)}')


def parse_args(
    args: Sequence[web_idl.argument.Argument]) -> List[ParameterType]:
  """Parse the list of arguments and returns a list of parameter types.

  Args:
      op: the operation

  Returns:
      the list of parameter types
  """
  # In IDL constructor definitions, it is possible to have optional arguments
  # before plain arguments, which doesn't really make sense in JS.
  rev_args = []
  has_seen_plain = False
  for arg in reversed(args):
    if arg.is_optional:
      if has_seen_plain:
        rev_args.append(ParameterType.plain(idl_type_to_iltype(arg.idl_type)))
      else:
        rev_args.append(ParameterType.opt(idl_type_to_iltype(arg.idl_type)))
    elif arg.is_variadic:
      rev_args.append(
          ParameterType.rest(idl_type_to_iltype(arg.idl_type.element_type)))
    else:
      has_seen_plain = True
      rev_args.append(ParameterType.plain(idl_type_to_iltype(arg.idl_type)))
  rev_args.reverse()
  return rev_args


def parse_operation(
    op: Union[web_idl.operation.Operation,
              web_idl.callback_function.CallbackFunction]
) -> SignatureType:
  """Parses an IDL 'operation', which is the method equivalent of a Javascript
  object.

  Args:
      op: the operation to parse

  Returns:
      the signature of the operation
  """
  ret = idl_type_to_iltype(op.return_type)
  return SignatureType(args=parse_args(op.arguments), ret=ret)


def parse_interface(
    interface: Union[web_idl.interface.Interface,
                     web_idl.callback_interface.CallbackInterface]
) -> Tuple[ILType, ObjectGroup]:
  """Parses an IDL 'interface', which is a Javascript object.

  Args:
      interface: the interface to parse

  Returns:
      a tuple containing the ILType variable declaration and the associated
      object group that defines the object properties and methods.
  """
  attributes = {
      a.identifier: idl_type_to_iltype(a.idl_type)
      for a in interface.attributes if not a.is_static
  }
  methods = {
      o.identifier: parse_operation(o)
      for o in interface.operations if not o.is_static
  }

  obj = ILType.object(group=interface.identifier,
                      props=list(attributes.keys()),
                      methods=list(methods.keys()))
  group = ObjectGroup(name=interface.identifier,
                      instanceType=ILType.refType(f'js{interface.identifier}'),
                      properties=attributes,
                      methods=methods)
  return obj, group


def parse_constructors(
    interface: Union[web_idl.interface.Interface,
                     web_idl.callback_interface.CallbackInterface]
) -> Tuple[Optional[ILType], Optional[ObjectGroup]]:
  """Parses an IDL 'interface' static properties, methods and constructors.
  This must be differentiated with `parse_interface`, because those are
  actually two different objects in Javascript.

  Args:
      interface: the interface to parse.

  Returns:
      A var declaration and an object group, if any.
  """
  attributes = {
      a.identifier: idl_type_to_iltype(a.idl_type)
      for a in interface.attributes if a.is_static
  }
  methods = {
      o.identifier: parse_operation(o)
      for o in interface.operations if o.is_static
  }

  typedecl = None
  has_object = False
  if attributes or methods:
    has_object = True
    typedecl = ILType.object(group=f'{interface.identifier}Constructor',
                             props=attributes.keys(),
                             methods=methods.keys())

  if interface.constructors:
    # As of now, Fuzzilli cannot handle multiple constructors, because it
    # doesn't make sense in Javascript.
    c = interface.constructors[0]
    args = parse_args(c.arguments)
    ctor = ILType.constructor(
        SignatureType(args=args,
                      ret=ILType.refType(f'js{interface.identifier}')))
    typedecl = ctor + typedecl if typedecl else ctor

  if not typedecl:
    return None, None

  group = None
  if has_object:
    var_name = f'{interface.identifier}Constructor'
    group = ObjectGroup(name=var_name,
                        instanceType=ILType.refType(f'js{var_name}'),
                        properties=attributes,
                        methods=methods)
  return typedecl, group


def parse_dictionary(
    dictionary: web_idl.dictionary.Dictionary
) -> Tuple[Optional[ILType], Optional[ObjectGroup]]:
  """Parses an IDL dictionary.

  Args:
      dictionary: the IDL dictionary

  Returns:
      A tuple consisting of its ILType definition and its ObjectGroup
      definition.
  """
  props = {
      m.identifier: idl_type_to_iltype(m.idl_type)
      for m in dictionary.members
  }
  obj = ILType.object(group=f'{dictionary.identifier}',
                      props=list(props.keys()),
                      methods=[])
  group = ObjectGroup(name=f'{dictionary.identifier}',
                      instanceType=ILType.refType(f'js{dictionary.identifier}'),
                      properties=props,
                      methods={})
  return obj, group


def main():
  parser = argparse.ArgumentParser(
      description=
      'Generates a Chrome Profile for Fuzzilli that describes WebIDLs.')
  parser.add_argument('-p',
                      '--path',
                      required=True,
                      help='Path to the web_idl_database.')
  parser.add_argument('-o',
                      '--outfile',
                      required=True,
                      help='Path to the output profile.')

  args = parser.parse_args()
  database = web_idl.Database.read_from_file(args.path)

  template_dir = os.path.dirname(os.path.abspath(__file__))
  environment = jinja2.Environment(loader=jinja2.FileSystemLoader(template_dir))
  environment.filters['parse_interface'] = parse_interface
  environment.filters['parse_constructors'] = parse_constructors
  environment.filters['parse_operation'] = parse_operation
  environment.filters['parse_dictionary'] = parse_dictionary
  environment.filters['idl_type_to_iltype'] = idl_type_to_iltype
  template = environment.get_template('ChromiumProfile.swift.tmpl')
  context = {
      'database': database,
  }
  with open(args.outfile, 'w') as f:
    f.write(template.render(context))


if __name__ == '__main__':
  main()
