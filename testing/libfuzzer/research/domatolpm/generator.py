#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import copy
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

sys.path.insert(1, os.path.join(SOURCE_DIR, 'third_party'))
sys.path.insert(1, os.path.join(SOURCE_DIR, 'third_party/domato/src'))
sys.path.append(os.path.join(SOURCE_DIR, 'build'))

import action_helpers
import jinja2
import grammar

# TODO(crbug.com/361369290): Remove this disable once DomatoLPM development is
# finished and upstream changes can be made to expose the relevant protected
# fields.
# pylint: disable=protected-access

def to_snake_case(name):
  name = re.sub(r'([A-Z]{2,})([A-Z][a-z])', r'\1_\2', name)
  return re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name, sys.maxsize).lower()


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
    'int16': 'int16',
    'uint16': 'uint16',
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


@dataclasses.dataclass
class ProtoType:
  """Represents a Proto type."""
  name: str

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

  def is_one_of(self) -> bool:
    return True


class CppExpression:

  def repr(self):
    raise Exception('Not implemented.')


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

  def is_oneof_handler(self) -> bool:
    return False

  def is_string_table_handler(self) -> bool:
    return False

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

  def is_oneof_handler(self) -> bool:
    return True


class DomatoBuilder:
  """DomatoBuilder is the class that takes a Domato grammar, and modelize it
  into a protobuf representation and its corresponding C++ parsing code.
  """

  @dataclasses.dataclass
  class Entry:
    msg: ProtoMessage
    func: CppFunctionHandler

  def __init__(self, g: grammar.Grammar):
    self.handlers: typing.Dict[str, DomatoBuilder.Entry] = {}
    self.backrefs: typing.Dict[str, typing.List[str]] = {}
    self.grammar = g
    if self.grammar._root and self.grammar._root != 'root':
      self.root = self.grammar._root
    else:
      self.root = 'lines'
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

  def should_generate_repeated_lines(self):
    return self.root == 'lines'

  def should_generate_one_line_handler(self):
    return self.root.startswith('lines')

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
    self.handlers[name] = DomatoBuilder.Entry(msg, handler)
    return True

  def get_roots(self) -> typing.Tuple[ProtoMessage, CppFunctionHandler]:
    root = self.root
    root_handler = f'{CPP_HANDLER_PREFIX}{root}'
    fuzz_case = ProtoMessage(
        name='fuzzcase',
        fields=[ProtoField(type=ProtoType(name=root), name='root', proto_id=1)])
    fuzz_fct = CppProtoMessageFunctionHandler(
        name='fuzzcase',
        exprs=[CppHandlerCallExpr(handler=root_handler, field_name='root')])
    return fuzz_case, fuzz_fct

  def get_protos(self) -> typing.Tuple[typing.List[ProtoMessage]]:
    if self.should_generate_one_line_handler():
      # We're handling a code grammar.
      roots = [v.msg for k, v in self.handlers.items() if k.startswith('line')]
      roots.append(self.get_roots()[0])
      non_roots = [
          v.msg for k, v in self.handlers.items() if not k.startswith('line')
      ]
      return roots, non_roots
    return [self.get_roots()[0]], self.all_proto_messages()

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
    self._oneofs_reorderer()
    self._oneof_message_renamer()
    self._message_renamer()

  def _add(self, message: ProtoMessage,
           handler: CppProtoMessageFunctionHandler):
    self.handlers[message.name] = DomatoBuilder.Entry(message, handler)
    for field in message.fields:
      if not field.type.name in self.backrefs:
        self.backrefs[field.type.name] = []
      self.backrefs[field.type.name].append(message.name)

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

  def _default_handler(
      self, part,
      field_name: str) -> typing.Tuple[ProtoType, CppHandlerCallExpr]:
    proto_type = DOMATO_TO_PROTO_BUILT_IN[part['tagname']]
    handler = DOMATO_TO_CPP_HANDLERS[part['tagname']]
    contents = CppHandlerCallExpr(handler=handler, field_name=field_name)
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
        creator = {'var_type': creator_name, 'var_prefix': 'var'}
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
      if not field.type.name in self.backrefs:
        self.backrefs[field.type.name] = []
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
      if msg.is_one_of() or not func.is_message_handler() or func.creates_new(
      ) or self._is_root_node(name):
        continue
      if name not in self.backrefs:
        continue
      for elt in self.backrefs[name]:
        if elt == name or elt not in self.handlers:
          continue
        if self.handlers[elt].msg.is_one_of():
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
      if entry.msg.is_one_of() or entry.func.is_string_table_handler():
        continue
      for proto_id, field in enumerate(entry.msg.fields, start=1):
        field.proto_id = proto_id
        if entry.func.creates_new() and field.name == 'old':
          continue
        field.name = to_proto_field_name(f'{entry.msg.name}_{proto_id}')
      index = 2 if entry.func.creates_new() else 1
      new_contents = []
      for expr in entry.func.exprs:
        if not isinstance(expr, CppHandlerCallExpr):
          new_contents.append(expr)
          continue
        new_contents.append(
            CppHandlerCallExpr(expr.handler,
                               to_proto_field_name(f'{entry.msg.name}_{index}'),
                               expr.extra_args))
        index += 1
      entry.func.exprs = new_contents

  def _oneof_message_renamer(self):
    """Renames OneOfProtoMessage fields that might have been merged. This
    ensures proto field naming remains consistent with the current rule being
    generated.
    """
    for entry in self.handlers.values():
      if not entry.msg.is_one_of():
        continue
      cases = {}
      for proto_id, field in enumerate(entry.msg.fields, start=1):
        field.proto_id = proto_id
        exprs = entry.func.cases.pop(field.name)
        field.name = to_proto_field_name(f'{entry.msg.name}_{proto_id}')
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

      if not msg.is_one_of():
        continue

      if not all(f.type.name in self.handlers and len(self.handlers[
          f.type.name].msg.fields) == 0 and not self.handlers[f.type.name].msg.
                 is_one_of() and len(self.handlers[f.type.name].func.exprs) == 1
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
      if msg.is_one_of():
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
      if not msg.is_one_of():
        continue

      for field in msg.fields:
        if not field.type.name in self.handlers:
          continue
        field_msg = self.handlers[field.type.name].msg
        field_func = self.handlers[field.type.name].func
        if field_msg.is_one_of() or len(
            field_msg.fields) != 1 or not field_func.is_message_handler(
            ) or field_func.creates_new():
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

      if not msg.is_one_of() or len(msg.fields) > 1:
        continue

      # The message is a unary oneof. Let's make sure it's only child doesn't
      # have backrefs.
      if self._count_backref(msg.fields[0].type.name) > 1:
        continue

      # The only backref should really only be us. If not we screwed up
      # somewhere else.
      assert name in self.backrefs[msg.fields[0].type.name]
      field_msg: ProtoMessage = self.handlers[msg.fields[0].type.name].msg
      if field_msg.is_one_of():
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
      if not func.is_message_handler() or len(func.exprs) <= 1:
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

  def _is_root_node(self, name: str):
    # If there is no existing root, we set it to `lines`, since this will
    # be picked as the default root.
    if 'line' not in self.root:
      return self.root == name
    return re.match('^line(s)?(_[0-9]*)?$', name) is not None

  def _remove_unlinked_nodes(self) -> bool:
    """Removes proto messages that are neither part of the root definition nor
    referenced by any other messages. This can happen during other optimization
    functions.

    Returns:
        whether a change was made.
    """
    to_remove = set()
    for name in self.handlers:
      if name not in self.backrefs or len(self.backrefs[name]) == 0:
        if not self._is_root_node(name):
          to_remove.add(name)
    local_root = 'line' if self.should_generate_one_line_handler(
    ) else self.root
    seen = set()

    def visit_msg(msg: ProtoMessage):
      if msg.name in seen:
        return
      seen.add(msg.name)
      for field in msg.fields:
        if field.type.name in self.handlers:
          visit_msg(self.handlers[field.type.name].msg)

    visit_msg(self.handlers[local_root].msg)
    not_seen = set(self.handlers.keys()) - seen
    to_remove.update(set(filter(lambda x: not self._is_root_node(x), not_seen)))
    for t in to_remove:
      self._remove(t)
    return len(to_remove) > 0


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
      'messages': [m for m in proto_messages if not m.is_one_of()],
      'oneofmessages': [m for m in proto_messages if m.is_one_of()],
      'generate_repeated_lines': should_generate_repeated_lines,
      'proto_ns': proto_ns,
      'imports': imports,
  },
                   out_f=out_f)


def render_proto(environment: jinja2.Environment, generated_dir: str,
                 out_f: str, name: str, builder: DomatoBuilder):
  template = environment.get_template('domatolpm.proto.tmpl')
  roots, non_roots = builder.get_protos()
  ns = f'{BASE_PROTO_NS}.{name}'
  sub_proto_filename = pathlib.PurePosixPath(f'{out_f}_sub.proto').name
  import_path = pathlib.PurePosixPath(generated_dir).joinpath(
      sub_proto_filename)
  _render_proto_internal(template, f'{out_f}.proto', roots,
                         builder.should_generate_repeated_lines(), ns,
                         [str(import_path)])
  _render_proto_internal(template, f'{out_f}_sub.proto', non_roots, False, ns,
                         [])


def render_cpp(environment: jinja2.Environment, out_f: str, name: str,
               builder: DomatoBuilder):
  functions = builder.all_cpp_functions()
  funcs = [f for f in functions if f.is_message_handler()]
  oneofs = [f for f in functions if f.is_oneof_handler()]
  stfunctions = [f for f in functions if f.is_string_table_handler()]
  _, root_func = builder.get_roots()

  rendering_context = {
      'basename': os.path.basename(out_f),
      'functions': funcs,
      'oneoffunctions': oneofs,
      'stfunctions': stfunctions,
      'root': root_func,
      'generate_repeated_lines': builder.should_generate_repeated_lines(),
      'generate_one_line_handler': builder.should_generate_one_line_handler(),
      'line_prefix': builder.get_line_prefix(),
      'line_suffix': builder.get_line_suffix(),
      'proto_ns': to_cpp_ns(f'{BASE_PROTO_NS}.{name}'),
      'cpp_ns': f'domatolpm::{name}',
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

  args = parser.parse_args()
  g = grammar.Grammar()
  g.parse_from_file(filename=args.path)

  template_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              'templates')
  environment = jinja2.Environment(loader=jinja2.FileSystemLoader(template_dir))
  builder = DomatoBuilder(g)
  builder.parse_grammar()
  builder.simplify()
  render_cpp(environment, args.file_format, args.name, builder)
  render_proto(environment, args.generated_dir, args.file_format, args.name,
               builder)


if __name__ == '__main__':
  main()
