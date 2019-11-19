#! /usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A generator of mojom interfaces and typemaps from Chrome IPC messages.

For example,
generate_mojom.py content/common/file_utilities_messages.h
    --output_mojom=content/common/file_utilities.mojom
    --output_typemap=content/common/file_utilities.typemap
"""

import argparse
import logging
import os
import re
import subprocess
import sys

_MESSAGE_PATTERN = re.compile(
    r'(?:\n|^)IPC_(SYNC_)?MESSAGE_(ROUTED|CONTROL)(\d_)?(\d)')
_VECTOR_PATTERN = re.compile(r'std::(vector|set)<(.*)>')
_MAP_PATTERN = re.compile(r'std::map<(.*), *(.*)>')
_NAMESPACE_PATTERN = re.compile(r'([a-z_]*?)::([A-Z].*)')

_unused_arg_count = 0


def _git_grep(pattern, paths_pattern):
  try:
    args = ['git', 'grep', '-l', '-e', pattern, '--'] + paths_pattern
    result = subprocess.check_output(args).strip().splitlines()
    logging.debug('%s => %s', ' '.join(args), result)
    return result
  except subprocess.CalledProcessError:
    logging.debug('%s => []', ' '.join(args))
    return []


def _git_multigrep(patterns, paths):
  """Find a list of files that match all of the provided patterns."""
  if isinstance(paths, str):
    paths = [paths]
  if isinstance(patterns, str):
    patterns = [patterns]
  for pattern in patterns:
    # Search only the files that matched previous patterns.
    paths = _git_grep(pattern, paths)
    if not paths:
      return []
  return paths


class Typemap(object):

  def __init__(self, typemap_files):
    self._typemap_files = typemap_files
    self._custom_mappings = {}
    self._new_custom_mappings = {}
    self._imports = set()
    self._public_includes = set()
    self._traits_includes = set()
    self._enums = set()

  def load_typemaps(self):
    for typemap in self._typemap_files:
      self.load_typemap(typemap)

  def load_typemap(self, path):
    typemap = {}
    with open(path) as f:
      content = f.read().replace('=\n', '=')
    exec content in typemap
    for mapping in typemap['type_mappings']:
      mojom, native = mapping.split('=')
      self._custom_mappings[native] = {'name': mojom,
                                       'mojom': typemap['mojom'].strip('/')}

  def generate_typemap(self, output_mojom, input_filename, namespace):
    new_mappings = sorted(self._format_new_mappings(namespace))
    if not new_mappings:
      return
    yield """# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
    yield 'mojom = "//%s"' % output_mojom
    yield 'public_headers = [%s\n]' % ''.join(
        '\n  "//%s",' % include for include in sorted(self._public_includes))
    yield 'traits_headers = [%s\n]' % ''.join(
        '\n  "//%s",' % include
        for include in sorted(self._traits_includes.union([os.path.normpath(
            input_filename)])))
    yield 'deps = [ "//ipc" ]'
    yield 'type_mappings = [\n  %s\n]' % '\n  '.join(new_mappings)

  def _format_new_mappings(self, namespace):
    for native, mojom in self._new_custom_mappings.items():
      yield '"%s.%s=::%s",' % (namespace, mojom, native)

  def format_new_types(self):
    for native_type, typename in self._new_custom_mappings.items():
      if native_type in self._enums:
        yield '[Native]\nenum %s;\n' % typename
      else:
        yield '[Native]\nstruct %s;\n' % typename

  _BUILTINS = {
      'bool': 'bool',
      'int': 'int32',
      'unsigned': 'uint32',
      'char': 'uint8',
      'unsigned char': 'uint8',
      'short': 'int16',
      'unsigned short': 'uint16',
      'int8_t': 'int8',
      'int16_t': 'int16',
      'int32_t': 'int32',
      'int64_t': 'int64',
      'uint8_t': 'uint8',
      'uint16_t': 'uint16',
      'uint32_t': 'uint32',
      'uint64_t': 'uint64',
      'float': 'float',
      'double': 'double',
      'std::string': 'string',
      'base::string16': 'string',
      'base::FilePath::StringType': 'string',
      'base::SharedMemoryHandle': 'handle<shared_memory>',
      'IPC::PlatformFileForTransit': 'handle',
      'base::FileDescriptor': 'handle',
  }

  def lookup_type(self, typename):
    try:
      return self._BUILTINS[typename]
    except KeyError:
      pass

    vector_match = _VECTOR_PATTERN.search(typename)
    if vector_match:
      return 'array<%s>' % self.lookup_type(vector_match.groups()[1].strip())
    map_match = _MAP_PATTERN.search(typename)
    if map_match:
      return 'map<%s, %s>' % tuple(self.lookup_type(t.strip())
                                   for t in map_match.groups())
    try:
      result = self._custom_mappings[typename]['name']
      mojom = self._custom_mappings[typename].get('mojom', None)
      if mojom:
        self._imports.add(mojom)
      return result
    except KeyError:
      pass

    match = _NAMESPACE_PATTERN.match(typename)
    if match:
      namespace, name = match.groups()
    else:
      namespace = ''
      name = typename
    namespace = namespace.replace('::', '.')
    cpp_name = name
    name = name.replace('::', '')

    if name.endswith('Params'):
      try:
        _, name = name.rsplit('Msg_')
      except ValueError:
        try:
          _, name = name.split('_', 1)
        except ValueError:
          pass

    if namespace.endswith('.mojom'):
      generated_mojom_name = '%s.%s' % (namespace, name)
    elif not namespace:
      generated_mojom_name = 'mojom.%s' % name
    else:
      generated_mojom_name = '%s.mojom.%s' % (namespace, name)

    self._new_custom_mappings[typename] = name
    self._add_includes(namespace, cpp_name, typename)
    generated_mojom_name = name
    self._custom_mappings[typename] = {'name': generated_mojom_name}
    return generated_mojom_name

  def _add_includes(self, namespace, name, fullname):
    name_components = name.split('::')
    is_enum = False
    for i in xrange(len(name_components)):
      subname = '::'.join(name_components[i:])
      extra_names = name_components[:i] + [subname]
      patterns = [r'\(struct\|class\|enum\)[A-Z_ ]* %s {' % s
                  for s in extra_names]
      if namespace:
        patterns.extend(r'namespace %s' % namespace_component
                        for namespace_component in namespace.split('.'))
      includes = _git_multigrep(patterns, '*.h')
      if includes:
        if _git_grep(r'enum[A-Z_ ]* %s {' % subname, includes):
          self._enums.add(fullname)
          is_enum = True
        logging.info('%s => public_headers = %s', fullname, includes)
        self._public_includes.update(includes)
        break

    if is_enum:
      patterns = ['IPC_ENUM_TRAITS[A-Z_]*(%s' % fullname]
    else:
      patterns = [r'\(IPC_STRUCT_TRAITS_BEGIN(\|ParamTraits<\)%s' % fullname]
    includes = _git_multigrep(
        patterns,
        ['*messages.h', '*struct_traits.h', 'ipc/ipc_message_utils.h'])
    if includes:
      logging.info('%s => traits_headers = %s', fullname, includes)
      self._traits_includes.update(includes)

  def format_imports(self):
    for import_name in sorted(self._imports):
      yield 'import "%s";' % import_name
    if self._imports:
      yield ''


class Argument(object):

  def __init__(self, typename, name):
    self.typename = typename.strip()
    self.name = name.strip().replace('\n', '').replace(' ', '_').lower()
    if not self.name:
      global _unused_arg_count
      self.name = 'unnamed_arg%d' % _unused_arg_count
      _unused_arg_count += 1

  def format(self, typemaps):
    return '%s %s' % (typemaps.lookup_type(self.typename), self.name)


class Message(object):

  def __init__(self, match, content):
    self.sync = bool(match[0])
    self.routed = match[1] == 'ROUTED'
    self.args = []
    self.response_args = []
    if self.sync:
      num_expected_args = int(match[2][:-1])
      num_expected_response_args = int(match[3])
    else:
      num_expected_args = int(match[3])
      num_expected_response_args = 0
    body = content.split(',')
    name = body[0].strip()
    try:
      self.group, self.name = name.split('Msg_')
    except ValueError:
      try:
        self.group, self.name = name.split('_')
      except ValueError:
        self.group = 'UnnamedInterface'
        self.name = name
    self.group = '%s%s' % (self.group, match[1].title())
    args = list(self.parse_args(','.join(body[1:])))
    if len(args) != num_expected_args + num_expected_response_args:
      raise Exception('Incorrect number of args parsed for %s' % (name))
    self.args = args[:num_expected_args]
    self.response_args = args[num_expected_args:]

  def parse_args(self, args_str):
    args_str = args_str.strip()
    if not args_str:
      return
    looking_for_type = False
    type_start = 0
    comment_start = None
    comment_end = None
    type_end = None
    angle_bracket_nesting = 0
    i = 0
    while i < len(args_str):
      if args_str[i] == ',' and not angle_bracket_nesting:
        looking_for_type = True
        if type_end is None:
          type_end = i
      elif args_str[i:i + 2] == '/*':
        if type_end is None:
          type_end = i
        comment_start = i + 2
        comment_end = args_str.index('*/', i + 2)
        i = comment_end + 1
      elif args_str[i:i + 2] == '//':
        if type_end is None:
          type_end = i
        comment_start = i + 2
        comment_end = args_str.index('\n', i + 2)
        i = comment_end
      elif args_str[i] == '<':
        angle_bracket_nesting += 1
      elif args_str[i] == '>':
        angle_bracket_nesting -= 1
      elif looking_for_type and args_str[i].isalpha():
        if comment_start is not None and comment_end is not None:
          yield Argument(args_str[type_start:type_end],
                         args_str[comment_start:comment_end])
        else:
          yield Argument(args_str[type_start:type_end], '')
        type_start = i
        type_end = None
        comment_start = None
        comment_end = None
        looking_for_type = False
      i += 1
    if comment_start is not None and comment_end is not None:
      yield Argument(args_str[type_start:type_end],
                     args_str[comment_start:comment_end])
    else:
      yield Argument(args_str[type_start:type_end], '')

  def format(self, typemaps):
    result = '%s(%s)' % (self.name, ','.join('\n      %s' % arg.format(typemaps)
                                             for arg in self.args))
    if self.sync:
      result += ' => (%s)' % (',\n'.join('\n      %s' % arg.format(typemaps)
                                         for arg in self.response_args))
      result = '[Sync]\n  %s' % result
    return '%s;' % result


class Generator(object):

  def __init__(self, input_name, output_namespace):
    self._input_name = input_name
    with open(input_name) as f:
      self._content = f.read()
    self._namespace = output_namespace
    self._typemaps = Typemap(self._find_typemaps())
    self._interface_definitions = []

  def _get_messages(self):
    for m in _MESSAGE_PATTERN.finditer(self._content):
      i = m.end() + 1
      while i < len(self._content):
        if self._content[i:i + 2] == '/*':
          i = self._content.index('*/', i + 2) + 1
        elif self._content[i] == ')':
          yield Message(m.groups(), self._content[m.end() + 1:i])
          break
        i += 1

  def _extract_messages(self):
    grouped_messages = {}
    for m in self._get_messages():
      grouped_messages.setdefault(m.group, []).append(m)
    self._typemaps.load_typemaps()
    for interface, messages in grouped_messages.items():
      self._interface_definitions.append(self._format_interface(interface,
                                                                messages))

  def count(self):
    grouped_messages = {}
    for m in self._get_messages():
      grouped_messages.setdefault(m.group, []).append(m)
    return sum(len(messages) for messages in grouped_messages.values())

  def generate_mojom(self):
    self._extract_messages()
    if not self._interface_definitions:
      return
    yield """// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
"""
    yield 'module %s;\n' % self._namespace
    for import_statement in self._typemaps.format_imports():
      yield import_statement
    for typemap in self._typemaps.format_new_types():
      yield typemap
    for interface in self._interface_definitions:
      yield interface
      yield ''

  def generate_typemap(self, output_mojom, input_filename):
    return '\n'.join(self._typemaps.generate_typemap(
        output_mojom, input_filename, self._namespace)).strip()

  @staticmethod
  def _find_typemaps():
    return subprocess.check_output(
        ['git', 'ls-files', '*.typemap']).strip().split('\n')

  def _format_interface(self, name, messages):
    return 'interface %s {\n  %s\n};' % (name,
                                         '\n  '.join(m.format(self._typemaps)
                                                     for m in messages))


def parse_args():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('input', help='input messages.h file')
  parser.add_argument(
      '--output_namespace',
      default='mojom',
      help='the mojom module name to use in the generated mojom file '
      '(default: %(default)s)')
  parser.add_argument('--output_mojom', help='output mojom path')
  parser.add_argument('--output_typemap', help='output typemap path')
  parser.add_argument(
      '--count',
      action='store_true',
      default=False,
      help='count the number of messages in the input instead of generating '
      'a mojom file')
  parser.add_argument('-v',
                      '--verbose',
                      action='store_true',
                      help='enable logging')
  parser.add_argument('-vv', action='store_true', help='enable debug logging')
  return parser.parse_args()


def main():
  args = parse_args()
  if args.vv:
    logging.basicConfig(level=logging.DEBUG)
  elif args.verbose:
    logging.basicConfig(level=logging.INFO)
  generator = Generator(args.input, args.output_namespace)
  if args.count:
    count = generator.count()
    if count:
      print('%d %s' % (generator.count(), args.input))
    return
  mojom = '\n'.join(generator.generate_mojom()).strip()
  if not mojom:
    return
  typemap = generator.generate_typemap(args.output_mojom, args.input)

  if args.output_mojom:
    with open(args.output_mojom, 'w') as f:
      f.write(mojom)
  else:
    print(mojom)
  if typemap:
    if args.output_typemap:
      with open(args.output_typemap, 'w') as f:
        f.write(typemap)
    else:
      print(typemap)


if __name__ == '__main__':
  sys.exit(main())
