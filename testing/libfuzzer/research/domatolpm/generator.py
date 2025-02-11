#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import copy
import functools
import os
import pathlib
import sys
import typing
import re
import dataclasses


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

# //build imports.
sys.path.append(os.path.join(SOURCE_DIR, 'build'))
import action_helpers

# //third_party imports.
sys.path.insert(1, os.path.join(SOURCE_DIR, 'third_party'))
import jinja2

# //third_party/domato/src imports.
sys.path.insert(1, os.path.join(SOURCE_DIR, 'third_party/domato/src'))
import grammar

# TODO(crbug.com/361369290): Remove this disable once DomatoLPM development is
# finished and upstream changes can be made to expose the relevant protected
# fields.
# pylint: disable=protected-access

def to_snake_case(name):
  name = re.sub(r'([A-Z]{2,})([A-Z][a-z])', r'\1_\2', name)
  return re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name, count=sys.maxsize).lower()


def GetProtoId(name):
  # We reserve ids [0,15]
  # Protobuf implementation reserves [19000,19999]
  # Max proto id is 2^29-1
  # 32-bit fnv-1a
  fnv = 2166136261
  for c in name:
    fnv = fnv ^ ord(c)
    fnv = (fnv * 16777619) & 0xffffffff
  # xor-fold to 29-bits
  fnv = (fnv >> 29) ^ (fnv & 0x1fffffff)
  # now use a modulo to reduce to [0,2^29-1 - 1016]
  fnv = fnv % 536869895
  # now we move out the disallowed ranges
  fnv = fnv + 15
  if fnv >= 19000:
    fnv += 1000
  return fnv


DOMATO_INT_TYPE_TO_CPP_INT_TYPE = {
    'int': 'int',
    'int32': 'int32_t',
    'uint32': 'uint32_t',
    'int8': 'int8_t',
    'uint8': 'uint8_t',
    'int16': 'int16_t',
    'uint16': 'uint16_t',
    'int64': 'uint64_t',
    'uint64': 'uint64_t',
}

DOMATO_TO_PROTO_BUILT_IN = {
    'int': 'int32',
    'int32': 'int32',
    'uint32': 'uint32',
    'int8': 'int32',
    'uint8': 'uint32',
    'int16': 'int32',
    'uint16': 'uint32',
    'int64': 'int64',
    'uint64': 'uint64',
    'float': 'float',
    'double': 'double',
    'char': 'int32',
    'string': 'string',
    'htmlsafestring': 'string',
    'hex': 'int32',
    'lines': 'repeated lines',
}

DOMATO_TO_CPP_HANDLERS = {
    'int': 'handle_int_conversion<int32_t, int>',
    'int32': 'handle_int_conversion<int32_t, int32_t>',
    'uint32': 'handle_int_conversion<uint32_t, uint32_t>',
    'int8': 'handle_int_conversion<int32_t, int8_t>',
    'uint8': 'handle_int_conversion<uint32_t, uint8_t>',
    'int16': 'handle_int_conversion<int16_t, int16_t>',
    'uint16': 'handle_int_conversion<uint16_t, uint16_t>',
    'int64': 'handle_int_conversion<int64_t, int64_t>',
    'uint64': 'handle_int_conversion<uint64_t, uint64_t>',
    'float': 'handle_float',
    'double': 'handle_double',
    'char': 'handle_char',
    'string': 'handle_string',
    'htmlsafestring': 'handle_string',
    'hex': 'handle_hex',
}

_C_STR_TRANS = str.maketrans({
    '\n': '\\n',
    '\r': '\\r',
    '\t': '\\t',
    '\"': '\\\"',
    '\\': '\\\\'
})

BASE_PROTO_NS = 'domatolpm.generated'


def to_cpp_ns(proto_ns: str) -> str:
  return proto_ns.replace('.', '::')


CPP_HANDLER_PREFIX = 'handle_'


def to_proto_field_name(name: str) -> str:
  """Converts a creator or rule name to a proto field name. This tries to
  respect the protobuf naming convention that field names should be snake case.

  Args:
      name: the name of the creator or the rule.

  Returns:
      the proto field name to use.
  """
  res = to_snake_case(name.replace('-', '_'))
  if res in ['short', 'class', 'bool', 'boolean', 'long', 'void']:
    res += '_proto'
  return res


def to_proto_type(creator_name: str) -> str:
  """Converts a creator name to a proto type. This is deliberately very simple
  so that we avoid naming conflicts.

  Args:
      creator_name: the name of the creator.

  Returns:
      the name of the proto type.
  """
  res = creator_name.replace('-', '_')
  if res in ['short', 'class', 'bool', 'boolean', 'long', 'void']:
    res += '_proto'
  return res


def c_escape(v: str) -> str:
  return v.translate(_C_STR_TRANS)


def tarjan(g):
  """This is a simple implementation of Tarjan's algorithm for finding the
  strongly connected components of the graph @g in a topological order."""
  stack = []
  index = {}
  lowlink = {}
  ret = []

  def visit(v):
    index[v] = len(index)
    lowlink[v] = index[v]
    stack.append(v)
    for w in g.get(v, ()):
      if w not in index:
        visit(w)
        lowlink[v] = min(lowlink[w], lowlink[v])
      elif w in stack:
        lowlink[v] = min(lowlink[v], index[w])
    if lowlink[v] == index[v]:
      scc = []
      w = None
      while v != w:
        w = stack.pop()
        scc.append(w)
      ret.append(scc)

  for v in g:
    if v not in index:
      visit(v)
  return ret


@dataclasses.dataclass
class ProtoType:
  """Represents a Proto type."""
  name: str

  @property
  def is_one_of(self) -> bool:
    return False


@dataclasses.dataclass
class ProtoField:
  """Represents a proto message field."""
  type: ProtoType
  name: str
  proto_id: int


@dataclasses.dataclass
class ProtoMessage(ProtoType):
  """Represents a Proto message."""
  fields: typing.List[ProtoField]


@dataclasses.dataclass
class OneOfProtoMessage(ProtoMessage):
  """Represents a Proto message with a oneof field."""
  oneofname: str

  @property
  def is_one_of(self) -> bool:
    return True


class CppExpression:

  # pylint: disable=no-self-use
  def repr(self):
    raise Exception('Not implemented.')
  # pylint: enable=no-self-use


@dataclasses.dataclass
class CppTxtExpression(CppExpression):
  """Represents a Raw text expression."""
  content: str

  def repr(self):
    return self.content


@dataclasses.dataclass
class CppCallExpr(CppExpression):
  """Represents a CallExpr."""
  fct_name: str
  args: typing.List[CppExpression]
  ns: str = ''

  def repr(self):
    arg_s = ', '.join([a.repr() for a in self.args])
    return f'{self.ns}{self.fct_name}({arg_s})'


class CppHandlerCallExpr(CppCallExpr):

  def __init__(self,
               handler: str,
               field_name: str,
               extra_args: typing.Optional[typing.List[CppExpression]] = None):
    args = [CppTxtExpression('ctx'), CppTxtExpression(f'arg.{field_name}()')]
    if extra_args:
      args += extra_args
    super().__init__(fct_name=handler, args=args)
    self.handler = handler
    self.field_name = field_name
    self.extra_args = extra_args


@dataclasses.dataclass
class CppStringExpr(CppExpression):
  """Represents a C++ literal string.
  """
  content: str

  def repr(self):
    return f'\"{c_escape(self.content)}\"'


@dataclasses.dataclass
class CppFunctionHandler:
  """Represents a C++ function.
  """
  name: str
  exprs: typing.List[CppExpression]

  @property
  def is_oneof_handler(self) -> bool:
    return False

  @property
  def is_string_table_handler(self) -> bool:
    return False

  @property
  def is_message_handler(self) -> bool:
    return False


class CppStringTableHandler(CppFunctionHandler):
  """Represents a C++ function that implements a string table and returns one
  of the represented strings.
  """

  def __init__(self, name: str, var_name: str,
               strings: typing.List[CppStringExpr]):
    super().__init__(name=f'{CPP_HANDLER_PREFIX}{name}', exprs=[])
    self.proto_type = f'{name}& arg'
    self.strings = strings
    self.var_name = var_name

  @property
  def is_string_table_handler(self) -> bool:
    return True


class CppProtoMessageFunctionHandler(CppFunctionHandler):
  """Represents a C++ function that handles a ProtoMessage.
  """

  def __init__(self,
               name: str,
               exprs: typing.List[CppExpression],
               creator: typing.Optional[typing.Dict[str, str]] = None):
    super().__init__(name=f'{CPP_HANDLER_PREFIX}{name}', exprs=exprs)
    self.proto_type = f'{name}& arg'
    self.creator = creator

  def creates_new(self):
    return self.creator is not None

  @property
  def is_message_handler(self) -> bool:
    return True


class CppOneOfMessageFunctionHandler(CppFunctionHandler):
  """Represents a C++ function that handles a OneOfProtoMessage.
  """

  def __init__(self, name: str, switch_name: str,
               cases: typing.Dict[str, typing.List[CppExpression]]):
    super().__init__(name=f'{CPP_HANDLER_PREFIX}{name}', exprs=[])
    self.proto_type = f'{name}& arg'
    self.switch_name = switch_name
    self.cases = cases

  def all_except_last(self):
    a = list(self.cases.keys())[:-1]
    return {e: self.cases[e] for e in a}

  def last(self):
    a = list(self.cases.keys())[-1]
    return self.cases[a]

  @property
  def is_oneof_handler(self) -> bool:
    return True


@dataclasses.dataclass
class File:
  name: str
  deps = []
  protos = []
  cpps = []


class DomatoBuilder:
  """DomatoBuilder is the class that takes a Domato grammar, and modelize it
  into a protobuf representation and its corresponding C++ parsing code.
  """

  @dataclasses.dataclass
  class Entry:
    msg: ProtoMessage
    func: CppFunctionHandler

  def __init__(self, g: grammar.Grammar, stabilize_grammar=False):
    self.handlers: typing.Dict[str, DomatoBuilder.Entry] = {}
    self.backrefs: typing.Dict[str,
                               typing.List[str]] = collections.defaultdict(list)
    self.grammar = g
    self.stabilize_grammar = stabilize_grammar
    if self.grammar._root and self.grammar._root != 'root':
      self.root = self.grammar._root
    else:
      self.root = 'line'
    if self.grammar._root and self.grammar._root == 'root':
      rules = self.grammar._creators[self.grammar._root]
      # multiple roots doesn't make sense, so we only consider the last defined
      # one.
      rule = rules[-1]
      for part in rule['parts']:
        if part['type'] == 'tag' and part[
            'tagname'] == 'lines' and 'count' in part:
          self.root = f'lines_{part["count"]}'
          break
    self._built_in_types_parser = {
        'int': self._int_handler,
        'int32': self._int_handler,
        'uint32': self._int_handler,
        'int8': self._int_handler,
        'uint8': self._int_handler,
        'int16': self._int_handler,
        'uint16': self._int_handler,
        'int64': self._int_handler,
        'uint64': self._int_handler,
        'float': self._default_handler,
        'double': self._default_handler,
        'char': self._default_handler,
        'string': self._default_handler,
        'htmlsafestring': self._default_handler,
        'hex': self._default_handler,
        'lines': self._lines_handler,
    }
    self.unique_id = 0

  def create_internal_message(self) -> str:
    """Returns a unique name for a newly created message.
    """
    self.unique_id += 1
    return f'DomatoLPMInternalMsg{self.unique_id}'

  def parse_grammar(self):
    for creator, rules in self.grammar._creators.items():
      field_name = to_proto_field_name(creator)
      type_name = to_proto_type(creator)
      messages = self._parse_rule(creator, rules)
      proto_fields: typing.List[ProtoField] = []
      for proto_id, msg in enumerate(messages, start=1):
        proto_fields.append(
            ProtoField(type=ProtoType(name=msg.name),
                       name=f'{field_name}_{proto_id}',
                       proto_id=proto_id))
      msg = OneOfProtoMessage(name=type_name,
                              oneofname='oneoffield',
                              fields=proto_fields)
      cases = {
          f.name: [
              CppHandlerCallExpr(handler=f'{CPP_HANDLER_PREFIX}{f.type.name}',
                                 field_name=f.name)
          ]
          for f in proto_fields
      }
      func = CppOneOfMessageFunctionHandler(name=type_name,
                                            switch_name='oneoffield',
                                            cases=cases)
      self._add(msg, func)

  def all_proto_messages(self):
    return [v.msg for v in self.handlers.values()]

  def all_cpp_functions(self):
    return [v.func for v in self.handlers.values()]

  def get_line_prefix(self) -> str:
    if not self.grammar._line_guard:
      return ''
    return self.grammar._line_guard.split('<line>')[0]

  def get_line_suffix(self) -> str:
    if not self.grammar._line_guard:
      return ''
    return self.grammar._line_guard.split('<line>')[1]

  def maybe_add_lines_handler(self, number: int) -> bool:
    name = f'lines_{number}'
    if name in self.handlers:
      return False
    fields = []
    exprs = []
    for i in range(1, number + 1):
      fields.append(ProtoField(ProtoType('line'), f'line_{i}', i))
      exprs.append(CppHandlerCallExpr('handle_one_line', f'line_{i}'))
    msg = ProtoMessage(name, fields=fields)
    handler = CppProtoMessageFunctionHandler(name, exprs=exprs)
    self._add(msg, handler)
    return True

  def get_root(self) -> typing.Tuple[ProtoMessage, CppFunctionHandler]:
    # If the root is 'line', we actually want to generate an arbitrary number
    # of lines. In this case, we'll invoke the special proto message 'lines'.
    # In any other case, we just use the existing root, which has been defined
    # during grammar construction.
    root = self.root
    if self.root == 'line':
      root = 'lines'
    root_handler = f'{CPP_HANDLER_PREFIX}{root}'
    fuzz_case = ProtoMessage(
        name='fuzzcase',
        fields=[ProtoField(type=ProtoType(name=root), name='root', proto_id=1)])
    fuzz_fct = CppProtoMessageFunctionHandler(
        name='fuzzcase',
        exprs=[CppHandlerCallExpr(handler=root_handler, field_name='root')])
    return fuzz_case, fuzz_fct

  def split_files(self, file_prefix: str, file_num=15):
    res = self._fusion_similar_messages()
    protos = self._split_protos(file_num)
    files = [File(f'tmp{i}') for i in range(0, file_num)]
    for i, (file, proto) in enumerate(zip(files, protos)):
      file.deps = []
      all_deps = set()
      for field in (f for e in proto for f in self.handlers[e].msg.fields):
        if field.type.name in self.handlers:
          all_deps.add(field.type.name)
      for j in range(i + 1, len(protos)):
        if any(dep in protos[j] for dep in all_deps):
          file.deps.append(files[j])

      file.protos = [self.handlers[elt].msg for elt in proto]
      file.cpps = []
      for elt in proto:
        if elt in res:
          file.cpps += res[elt]
        else:
          file.cpps.append(self.handlers[elt].func)

    # sort the files with most def first and least last.
    def comp(f1, f2):
      return len(f1.protos) - len(f2.protos)

    files = list(sorted(files, key=functools.cmp_to_key(comp), reverse=True))
    for i, file in enumerate(files):
      file.name = f'{file_prefix}{i}'
    return files

  def simplify(self):
    """Simplifies the proto and functions."""
    should_continue = True
    while should_continue:
      should_continue = False
      should_continue |= self._merge_unary_oneofs()
      should_continue |= self._merge_strings()
      should_continue |= self._merge_multistrings_oneofs()
      should_continue |= self._remove_unlinked_nodes()
      should_continue |= self._merge_proto_messages()
      should_continue |= self._merge_oneofs()
    if self.stabilize_grammar:
      self._hash_line_proto_ids()
    self._oneofs_reorderer()
    self._oneof_message_renamer()
    self._message_renamer()

  def _hash_line_proto_ids(self):
    if 'line' not in self.handlers:
      return
    rules = self.grammar._creators['line']
    for (rule, field) in zip(rules, self.handlers['line'].msg.fields):
      concat = ''.join(p['text'] if p['type'] == 'text' else p['tagname']
                       for p in rule['parts'])
      field.proto_id = GetProtoId(concat)

  def _add(self, message: ProtoMessage,
           handler: CppProtoMessageFunctionHandler):
    self.handlers[message.name] = DomatoBuilder.Entry(message, handler)
    for field in message.fields:
      self.backrefs[field.type.name].append(message.name)

  # Handlers should be together even if some of them don't actually use self.
  # pylint: disable=no-self-use
  def _int_handler(
      self, part,
      field_name: str) -> typing.Tuple[ProtoType, CppHandlerCallExpr]:
    proto_type = DOMATO_TO_PROTO_BUILT_IN[part['tagname']]
    handler = DOMATO_TO_CPP_HANDLERS[part['tagname']]
    extra_args = []
    if 'min' in part:
      extra_args.append(CppTxtExpression(part['min']))
    if 'max' in part:
      if not extra_args:
        cpp_type = DOMATO_INT_TYPE_TO_CPP_INT_TYPE[part['tagname']]
        extra_args.append(
            CppTxtExpression(f'std::numeric_limits<{cpp_type}>::min()'))
      extra_args.append(CppTxtExpression(part['max']))
    contents = CppHandlerCallExpr(handler=handler,
                                  field_name=field_name,
                                  extra_args=extra_args)
    return proto_type, contents

  def _default_handler(
      self, part,
      field_name: str) -> typing.Tuple[ProtoType, CppHandlerCallExpr]:
    proto_type = DOMATO_TO_PROTO_BUILT_IN[part['tagname']]
    handler = DOMATO_TO_CPP_HANDLERS[part['tagname']]
    contents = CppHandlerCallExpr(handler=handler, field_name=field_name)
    return proto_type, contents

  # pylint: enable=no-self-use

  def _lines_handler(
      self, part,
      field_name: str) -> typing.Tuple[ProtoType, CppHandlerCallExpr]:
    handler_name = 'lines'
    if 'count' in part:
      count = part['count']
      handler_name = f'{handler_name}_{count}'
      self.maybe_add_lines_handler(int(part['count']))
    proto_type = handler_name
    contents = CppHandlerCallExpr(handler=f'handle_{handler_name}',
                                  field_name=field_name)
    return proto_type, contents

  def _parse_rule(self, creator_name, rules):
    messages = []
    for rule_id, rule in enumerate(rules, start=1):
      rule_msg_field_name = f'{to_proto_field_name(creator_name)}_{rule_id}'
      proto_fields = []
      cpp_contents = []
      ret_vars = 0
      for part_id, part in enumerate(rule['parts'], start=1):
        field_name = f'{rule_msg_field_name}_{part_id}'
        proto_type = None
        if rule['type'] == 'code' and 'new' in part:
          proto_fields.insert(
              0,
              ProtoField(type=ProtoType('optional int32'),
                         name='old',
                         proto_id=part_id))
          ret_vars += 1
          continue
        if part['type'] == 'text':
          contents = CppStringExpr(part['text'])
        elif part['tagname'] == 'import':
          # The current domato project is currently not handling that either in
          # its built-in rules, and I do not plan on using the feature with
          # newly written rules, as I think this directive has a lot of
          # constraints with not much added value.
          continue
        elif part['tagname'] == 'call':
          raise Exception(
              'DomatoLPM does not implement <call> and <import> tags.')
        elif part['tagname'] in self.grammar._constant_types.keys():
          contents = CppStringExpr(
              self.grammar._constant_types[part['tagname']])
        elif part['tagname'] in self._built_in_types_parser:
          handler = self._built_in_types_parser[part['tagname']]
          proto_type, contents = handler(part, field_name)
        elif part['type'] == 'tag':
          proto_type = to_proto_type(part['tagname'])
          contents = CppHandlerCallExpr(
              handler=f'{CPP_HANDLER_PREFIX}{proto_type}',
              field_name=field_name)
        if proto_type:
          proto_fields.append(
              ProtoField(type=ProtoType(name=proto_type),
                         name=field_name,
                         proto_id=part_id))
        cpp_contents.append(contents)

      if ret_vars > 1:
        raise Exception('Not implemented.')

      creator = None
      if rule['type'] == 'code' and ret_vars > 0:
        creates = rule['creates']
        # For some reason, Domato sets a dictionary when the creator is a line
        # and a list when its a helper. Thus the unpacking code below. The
        # assertion ensures we are not dealing with another unknown format.
        if isinstance(creates, list):
          assert len(creates) == 1
          creates = creates[0]
        creator = {'var_type': creates['tagname'], 'var_prefix': 'var'}
      proto_type = to_proto_type(creator_name)
      rule_msg = ProtoMessage(name=f'{proto_type}_{rule_id}',
                              fields=proto_fields)
      rule_func = CppProtoMessageFunctionHandler(name=f'{proto_type}_{rule_id}',
                                                 exprs=cpp_contents,
                                                 creator=creator)

      self._add(rule_msg, rule_func)
      messages.append(rule_msg)
    return messages

  def _remove(self, name: str):
    assert name in self.handlers
    for field in self.handlers[name].msg.fields:
      if field.type.name in self.backrefs:
        self.backrefs[field.type.name].remove(name)
    if name in self.backrefs:
      self.backrefs.pop(name)
    self.handlers.pop(name)

  def _update(self, name: str):
    assert name in self.handlers
    for field in self.handlers[name].msg.fields:
      self.backrefs[field.type.name].append(name)

  def _count_backref(self, proto_name: str) -> int:
    """Counts the number of backreference a given proto message has.

    Args:
        proto_name: the proto message name.

    Returns:
        the number of backreferences.
    """
    return len(self.backrefs[proto_name])

  def _merge_proto_messages(self) -> bool:
    """Merges messages referencing other messages into the same message. This
    allows to tremendously reduce the number of protobuf messages that will be
    generated.
    """
    to_merge = collections.defaultdict(set)
    for name in self.handlers:
      msg = self.handlers[name].msg
      func = self.handlers[name].func
      if msg.is_one_of or not func.is_message_handler or func.creates_new(
      ) or name == self.root:
        continue
      if name not in self.backrefs:
        continue
      for elt in self.backrefs[name]:
        if elt == name or elt not in self.handlers:
          continue
        if self.handlers[elt].msg.is_one_of:
          continue
        to_merge[elt].add(name)

    for parent, childs in to_merge.items():
      msg = self.handlers[parent].msg
      fct = self.handlers[parent].func
      for child in childs:
        new_contents = []
        for expr in fct.exprs:
          if isinstance(expr, CppStringExpr):
            new_contents.append(expr)
            continue
          assert isinstance(expr, CppHandlerCallExpr)
          field: ProtoField = next(
              (f for f in msg.fields if f.type.name == child), None)
          if not field or not expr.field_name == field.name:
            new_contents.append(expr)
            continue
          self.backrefs[field.type.name].remove(msg.name)
          idx = msg.fields.index(field)
          field_msg = self.handlers[child].msg
          field_fct = self.handlers[child].func

          # The following deepcopy is required because we might change the
          # child's messages fields at some point, and we don't want those
          # changes to affect this current's message fields.
          fields_copy = copy.deepcopy(field_msg.fields)
          msg.fields = msg.fields[:idx] + fields_copy + msg.fields[idx + 1:]
          new_contents += copy.deepcopy(field_fct.exprs)
          for f in field_msg.fields:
            self.backrefs[f.type.name].append(msg.name)
        fct.exprs = new_contents
    return len(to_merge) > 0

  def _message_renamer(self):
    """Renames ProtoMessage fields that might have been merged. This ensures
    proto field naming remains consistent with the current rule being
    generated.
    """
    for entry in self.handlers.values():
      if entry.msg.is_one_of or entry.func.is_string_table_handler:
        continue
      for proto_id, field in enumerate(entry.msg.fields, start=1):
        field.proto_id = proto_id
        if entry.func.creates_new() and field.name == 'old':
          continue
        field.name = to_proto_field_name(f'field_{proto_id}')
      index = 2 if entry.func.creates_new() else 1
      new_contents = []
      for expr in entry.func.exprs:
        if not isinstance(expr, CppHandlerCallExpr):
          new_contents.append(expr)
          continue
        new_contents.append(
            CppHandlerCallExpr(expr.handler,
                               to_proto_field_name(f'field_{index}'),
                               expr.extra_args))
        index += 1
      entry.func.exprs = new_contents

  def _oneof_message_renamer(self):
    """Renames OneOfProtoMessage fields that might have been merged. This
    ensures proto field naming remains consistent with the current rule being
    generated.
    """
    for entry in self.handlers.values():
      if not entry.msg.is_one_of:
        continue
      cases = {}
      for proto_id, field in enumerate(entry.msg.fields, start=1):
        if entry.msg.name != 'line':
          field.proto_id = proto_id
        exprs = entry.func.cases.pop(field.name)
        field.name = to_proto_field_name(f'field_{proto_id}')
        new_contents = []
        for expr in exprs:
          if not isinstance(expr, CppHandlerCallExpr):
            new_contents.append(expr)
            continue
          new_contents.append(
              CppHandlerCallExpr(expr.handler, field.name, expr.extra_args))
        cases[field.name] = new_contents
      entry.func.cases = cases

  def _merge_multistrings_oneofs(self) -> bool:
    """Merges multiple strings into a string table function."""
    has_made_changes = False
    for name in list(self.handlers.keys()):
      msg = self.handlers[name].msg

      if not msg.is_one_of:
        continue

      if not all(f.type.name in self.handlers and len(self.handlers[
          f.type.name].msg.fields) == 0 and not self.handlers[f.type.name].msg.
                 is_one_of and len(self.handlers[f.type.name].func.exprs) == 1
                 for f in msg.fields):
        continue

      fields = [ProtoField(type=ProtoType('uint32'), name='val', proto_id=1)]
      new_msg = ProtoMessage(name=msg.name, fields=fields)
      strings = []
      for field in msg.fields:
        self.backrefs[field.type.name].remove(name)
        for expr in self.handlers[field.type.name].func.exprs:
          assert isinstance(expr, CppStringExpr)
          strings += [expr]
      new_func = CppStringTableHandler(name=msg.name,
                                       var_name='val',
                                       strings=strings)
      self.handlers[name] = DomatoBuilder.Entry(new_msg, new_func)
      self._update(name)
      has_made_changes = True
    return has_made_changes

  def _oneofs_reorderer(self):
    """Reorders the OneOfProtoMessage so that the last element can be extracted
    out of the protobuf oneof's field in order to always have a correct
    path to be generated. This requires having at least one terminal path in
    the grammar.
    """
    _terminal_messages = set()
    _being_visited = set()

    def recursive_terminal_marker(name: str):
      if name in _terminal_messages or name not in self.handlers:
        return True
      if name in _being_visited:
        return False
      _being_visited.add(name)
      msg = self.handlers[name].msg
      func = self.handlers[name].func
      if len(msg.fields) == 0:
        _terminal_messages.add(name)
        _being_visited.remove(name)
        return True
      if msg.is_one_of:
        f = next(
            (f for f in msg.fields if recursive_terminal_marker(f.type.name)),
            None)
        if not f:
          #FIXME: for testing purpose only, we're not hard-failing on this.
          _being_visited.remove(name)
          return False
        msg.fields.remove(f)
        msg.fields.append(f)
        m = next(k for k in func.cases.keys() if k == f.name)
        func.cases[m] = func.cases.pop(m)
        _terminal_messages.add(name)
        _being_visited.remove(name)
        return True
      res = all(recursive_terminal_marker(f.type.name) for f in msg.fields)
      #FIXME: for testing purpose only, we're not hard-failing on this.
      _being_visited.remove(name)
      return res

    for name in self.handlers:
      recursive_terminal_marker(name)

  def _merge_oneofs(self) -> bool:
    has_made_changes = False
    for name in list(self.handlers.keys()):
      msg = self.handlers[name].msg
      func = self.handlers[name].func
      if not msg.is_one_of:
        continue

      for field in msg.fields:
        if not field.type.name in self.handlers:
          continue
        field_msg = self.handlers[field.type.name].msg
        field_func = self.handlers[field.type.name].func
        if (field_msg.is_one_of or len(field_msg.fields) != 1
            or not field_func.is_message_handler or field_func.creates_new()):
          continue
        func.cases.pop(field.name)
        field.name = field_msg.fields[0].name
        field.type = field_msg.fields[0].type
        while field.name in func.cases:
          field.name += '_1'
        func.cases[field.name] = copy.deepcopy(field_func.exprs)
        self.backrefs[field_msg.name].remove(name)
        self.backrefs[field.type.name].append(name)
        has_made_changes = True
    return has_made_changes

  def _merge_unary_oneofs(self) -> bool:
    """Transfors OneOfProtoMessage messages containing only one field into a
    ProtoMessage containing the fields of the contained message. E.g.:
        message B {
          int field1 = 1;
          Whatever field2 = 2;
        }
        message A {
          oneof field {
            B b = 1;
          }
        }
        Into:
        message A {
          int field1 = 1;
          Whatever field2 = 2;
        }
    """
    has_made_changes = False
    for name in list(self.handlers.keys()):
      msg = self.handlers[name].msg
      func = self.handlers[name].func

      if not msg.is_one_of or len(msg.fields) > 1:
        continue

      # The message is a unary oneof. Let's make sure it's only child doesn't
      # have backrefs.
      if self._count_backref(msg.fields[0].type.name) > 1:
        continue

      # The only backref should really only be us. If not we screwed up
      # somewhere else.
      assert name in self.backrefs[msg.fields[0].type.name]
      field_msg: ProtoMessage = self.handlers[msg.fields[0].type.name].msg
      if field_msg.is_one_of:
        continue

      field_func = self.handlers[msg.fields[0].type.name].func
      self._remove(msg.fields[0].type.name)
      msg = ProtoMessage(name=msg.name, fields=field_msg.fields)
      func = CppProtoMessageFunctionHandler(name=msg.name,
                                            exprs=field_func.exprs,
                                            creator=field_func.creator)
      self.handlers[name] = DomatoBuilder.Entry(msg, func)
      self._update(name)
      has_made_changes = True
    return has_made_changes

  def _merge_strings(self) -> bool:
    """Merges following CppString, e.g.
    [ CppString("<first>"), CppString("<second>")]
    Into:
    [ CppString("<first><second>")]
    """
    has_made_changes = False
    for name in self.handlers:
      func: CppFunctionHandler = self.handlers[name].func
      if not func.is_message_handler or len(func.exprs) <= 1:
        continue

      exprs = []
      prev = func.exprs[0]
      for i in range(1, len(func.exprs)):
        cur = func.exprs[i]
        if isinstance(prev, CppStringExpr) and isinstance(cur, CppStringExpr):
          cur = CppStringExpr(prev.content + cur.content)
          has_made_changes = True
        else:
          exprs.append(prev)
        prev = cur
      exprs.append(prev)
      func.exprs = exprs
    return has_made_changes

  def _remove_unlinked_nodes(self) -> bool:
    """Removes proto messages that are neither part of the root definition nor
    referenced by any other messages. This can happen during other optimization
    functions.

    Returns:
        whether a change was made.
    """
    to_remove = set()
    for name in (n for n in self.handlers if n != self.root):
      if name not in self.backrefs or len(self.backrefs[name]) == 0:
        to_remove.add(name)
    seen = set()

    def visit_msg(msg: ProtoMessage):
      if msg.name in seen:
        return
      seen.add(msg.name)
      for field in msg.fields:
        if field.type.name in self.handlers:
          visit_msg(self.handlers[field.type.name].msg)

    visit_msg(self.handlers[self.root].msg)
    not_seen = set(self.handlers.keys()) - seen
    to_remove.update(set(filter(lambda x: x != self.root, not_seen)))
    for t in to_remove:
      self._remove(t)
    return len(to_remove) > 0

  def _split_oneof_internal(self, entry):
    assert entry.msg.is_one_of
    low_name = self.create_internal_message()
    high_name = self.create_internal_message()
    fields_low = copy.copy(entry.msg.fields[:len(entry.msg.fields) // 2])
    fields_high = copy.copy(entry.msg.fields[len(entry.msg.fields) // 2:])
    low = OneOfProtoMessage(low_name, fields=fields_low, oneofname='oneoffield')
    high = OneOfProtoMessage(high_name,
                             fields=fields_high,
                             oneofname='oneoffield')
    low_cases = {}
    for field in low.fields:
      low_cases[field.name] = entry.func.cases[field.name]
    high_cases = {}
    for field in high.fields:
      high_cases[field.name] = entry.func.cases[field.name]
    func_low = CppOneOfMessageFunctionHandler(low_name,
                                              switch_name='oneoffield',
                                              cases=low_cases)
    func_high = CppOneOfMessageFunctionHandler(high_name,
                                               switch_name='oneoffield',
                                               cases=high_cases)
    entry.msg.fields = [
        ProtoField(type=ProtoType(low_name), name='line_1', proto_id=1),
        ProtoField(type=ProtoType(high_name), name='line_2', proto_id=2),
    ]
    entry.func = CppOneOfMessageFunctionHandler(
        f'{entry.msg.name}',
        switch_name='oneoffield',
        cases={
            'line_1': [
                CppHandlerCallExpr(handler=f'{CPP_HANDLER_PREFIX}{low_name}',
                                   field_name='line_1')
            ],
            'line_2': [
                CppHandlerCallExpr(handler=f'{CPP_HANDLER_PREFIX}{high_name}',
                                   field_name='line_2')
            ]
        })
    self.handlers[low_name] = DomatoBuilder.Entry(low, func_low)
    self.handlers[high_name] = DomatoBuilder.Entry(high, func_high)
    self.backrefs[low_name] = [entry.msg.name]
    self.backrefs[high_name] = [entry.msg.name]
    for field in low.fields:
      self.backrefs[field.type.name].remove(entry.msg.name)
      self.backrefs[field.type.name].append(low_name)
    for field in high.fields:
      self.backrefs[field.type.name].remove(entry.msg.name)
      self.backrefs[field.type.name].append(high_name)

  def _split_oneofs(self):
    """Splits oneofs that are too big and that would grow protobuf files.
    """
    for entry in self.handlers.values():
      if entry.msg.is_one_of and len(entry.msg.fields) > 200:
        self._split_oneof_internal(entry)
        return True
    return False

  def _split_protos(self, num_files: int):
    """Splits the current proto definitions graph into multiple files
    referencing each others. This helps reducing the overall pb.cc compile
    time.

    Args:
        num_files: the number of files to be generated.

    Returns:
        a list of Files.
    """
    graph_rep = {}
    for v in self.handlers.values():
      graph_rep[v.msg.name] = set()
      for f in (f for f in v.msg.fields if f.type.name in self.handlers):
        graph_rep[v.msg.name].add(f.type.name)
    components = tarjan(graph_rep)

    assert self.root in components[-1]

    def weight(elts):
      return sum([len(self.handlers[entry].msg.fields) + 1 for entry in elts])

    def _get_comp_list(max_weight):
      comp_list = []
      current_list = []
      for comp in reversed(components):
        if weight(comp) + weight(current_list) > max_weight:
          comp_list.append(copy.copy(current_list))
          current_list.clear()
        current_list += comp
      if len(current_list) > 0:
        comp_list.append(current_list)
      return comp_list

    total_weight = sum(
        [len(entry.msg.fields) + 1 for entry in self.handlers.values()])
    # we purposefuly take a greater number here so that we can lower that until
    # we have the correct number of file being generated.
    cur_weight = total_weight / (num_files + 10)
    comp_list = _get_comp_list(cur_weight)
    while len(comp_list) > num_files:
      cur_weight *= 1.2
      comp_list = _get_comp_list(cur_weight)

    return comp_list

  def _fusion_similar_messages_impl(self, new_msg_name, messages):
    is_one_of = self.handlers[messages[0]].msg.is_one_of
    if is_one_of:
      new_msg = OneOfProtoMessage(new_msg_name,
                                  fields=self.handlers[messages[0]].msg.fields,
                                  oneofname='oneoffield')
      new_func = CppOneOfMessageFunctionHandler(
          new_msg_name, 'oneoffield',
          {field.name: [CppStringExpr('')]
           for field in new_msg.fields})
    else:
      new_msg = ProtoMessage(new_msg_name,
                             fields=self.handlers[messages[0]].msg.fields)
      new_func = CppProtoMessageFunctionHandler(new_msg_name, exprs=[])
    self._add(new_msg, new_func)
    res = []
    for e in messages:
      self.handlers[e].func.proto_type = new_func.proto_type
      for b in self.backrefs[e]:
        for f in (f for f in self.handlers[b].msg.fields if f.type.name == e):
          f.type.name = new_msg.name
          self.backrefs[new_msg.name].append(b)
      res.append(self.handlers[e].func)
      self._remove(e)
    return res

  def _fusion_similar_messages(self):
    oneof_entries = collections.defaultdict(list)
    msg_entries = collections.defaultdict(list)
    for e in (e for e in self.handlers.values() if e.msg.is_one_of):
      h = hash(''.join(f.type.name for f in e.msg.fields))
      oneof_entries[h].append(e.msg.name)
    for e in (e for e in self.handlers.values() if not e.msg.is_one_of):
      h = hash(''.join(f.type.name for f in e.msg.fields))
      msg_entries[h].append(e.msg.name)
    res = {}
    for value in list(oneof_entries.values()) + list(msg_entries.values()):
      if len(value) <= 1:
        continue
      msg_name = self.create_internal_message()
      res[msg_name] = self._fusion_similar_messages_impl(msg_name, value)
    self._remove_unlinked_nodes()
    return res


def _render_internal(template: jinja2.Template,
                     context: typing.Dict[str, typing.Any], out_f: str):
  with action_helpers.atomic_output(out_f, mode='w') as f:
    f.write(template.render(context))


def _render_proto_internal(
    template: jinja2.Template, out_f: str,
    proto_messages: typing.List[typing.Union[ProtoMessage, OneOfProtoMessage]],
    should_generate_repeated_lines: bool, proto_ns: str,
    imports: typing.List[str]):
  _render_internal(template, {
      'messages': [m for m in proto_messages if not m.is_one_of],
      'oneofmessages': [m for m in proto_messages if m.is_one_of],
      'generate_repeated_lines': should_generate_repeated_lines,
      'proto_ns': proto_ns,
      'imports': imports,
  },
                   out_f=out_f)


def to_relative_path(generated_dir: str, filepath: str):
  return str(
      pathlib.PurePosixPath(generated_dir).joinpath(
          pathlib.PurePosixPath(filepath).name))


def render_proto(environment: jinja2.Environment, generated_dir: str,
                 out_f: str, name: str, builder: DomatoBuilder,
                 files: typing.List[File]):
  template = environment.get_template('domatolpm.proto.tmpl')
  ns = f'{BASE_PROTO_NS}.{name}'
  for file in files:
    _render_proto_internal(
        template, f'{file.name}.proto', file.protos, False, ns, [
            to_relative_path(generated_dir, f'{dep.name}.proto')
            for dep in file.deps
        ])
  root, _ = builder.get_root()
  _render_proto_internal(
      template, f'{out_f}.proto', [root], builder.root == 'line', ns, [
          to_relative_path(generated_dir, f'{file.name}.proto')
          for file in files if builder.root in (m.name for m in file.protos)
      ])


def render_cpp(environment: jinja2.Environment, gen_dir: str, out_f: str,
               name: str, builder: DomatoBuilder, files: typing.List[File]):
  for file in files:
    funcs = [f for f in file.cpps if f.is_message_handler]
    oneofs = [f for f in file.cpps if f.is_oneof_handler]
    stfunctions = [f for f in file.cpps if f.is_string_table_handler]
    has_line = 'line' in (f.type.name for msg in file.protos
                          for f in msg.fields)
    rendering_context = {
        'includes':
        [to_relative_path(gen_dir, f'{dep.name}.h') for dep in file.deps],
        'functions':
        funcs,
        'oneoffunctions':
        oneofs,
        'stfunctions':
        stfunctions,
        'root':
        None,
        'generate_root':
        False,
        'generate_repeated_lines':
        False,
        'generate_one_line_handler':
        has_line,
        'line_prefix':
        builder.get_line_prefix(),
        'line_suffix':
        builder.get_line_suffix(),
        'proto_ns':
        to_cpp_ns(f'{BASE_PROTO_NS}.{name}'),
        'cpp_ns':
        f'domatolpm::{name}',
    }
    rendering_context['includes'].append(
        to_relative_path(gen_dir, f'{file.name}.h'))
    rendering_context['includes'].append(
        to_relative_path(gen_dir, f'{file.name}.pb.h'))
    template = environment.get_template('domatolpm.cc.tmpl')
    _render_internal(template, rendering_context, f'{file.name}.cc')
    rendering_context['includes'] = [
        to_relative_path(gen_dir, f'{file.name}.pb.h')
    ]
    template = environment.get_template('domatolpm.h.tmpl')
    _render_internal(template, rendering_context, f'{file.name}.h')
  _, root_func = builder.get_root()

  rendering_context = {
      'includes':
      [to_relative_path(gen_dir, f'{file.name}.h')
       for file in files] + [f'{os.path.basename(out_f)}.pb.h'],
      'functions': [],
      'oneoffunctions': [],
      'stfunctions': [],
      'root':
      root_func,
      'generate_root':
      True,
      'generate_repeated_lines':
      builder.root == 'line',
      'generate_one_line_handler':
      builder.root == 'line',
      'line_prefix':
      builder.get_line_prefix(),
      'line_suffix':
      builder.get_line_suffix(),
      'proto_ns':
      to_cpp_ns(f'{BASE_PROTO_NS}.{name}'),
      'cpp_ns':
      f'domatolpm::{name}',
  }
  template = environment.get_template('domatolpm.cc.tmpl')
  _render_internal(template, rendering_context, f'{out_f}.cc')
  template = environment.get_template('domatolpm.h.tmpl')
  _render_internal(template, rendering_context, f'{out_f}.h')


def main():
  parser = argparse.ArgumentParser(
      description=
      'Generate the necessary files for DomatoLPM to function properly.')
  parser.add_argument('-p',
                      '--path',
                      required=True,
                      help='The path to a Domato grammar file.')
  parser.add_argument('-n',
                      '--name',
                      required=True,
                      help='The name of this grammar.')
  parser.add_argument(
      '-f',
      '--file-format',
      required=True,
      help='The path prefix to which the files should be generated.')
  parser.add_argument('-d',
                      '--generated-dir',
                      required=True,
                      help='The path to the target gen directory.')
  parser.add_argument('-s',
                      '--stabilize-grammar',
                      required=False,
                      default=False,
                      action='store_true',
                      help='Whether we should stabilize the proto generation.'
                      'Grammars should not have duplicate lines')

  args = parser.parse_args()
  g = grammar.Grammar()
  g.parse_from_file(filename=args.path)

  template_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              'templates')
  environment = jinja2.Environment(loader=jinja2.FileSystemLoader(template_dir))
  builder = DomatoBuilder(g, args.stabilize_grammar)
  builder.parse_grammar()
  builder.simplify()
  files = builder.split_files(f'{args.file_format}_sub', file_num=12)
  render_cpp(environment, args.generated_dir, args.file_format, args.name,
             builder, files)
  render_proto(environment, args.generated_dir, args.file_format, args.name,
               builder, files)


if __name__ == '__main__':
  main()
