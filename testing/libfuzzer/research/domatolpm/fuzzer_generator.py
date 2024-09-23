#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import copy
import os
import sys
import re


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

import action_helpers
import jinja2

_C_STR_TRANS = str.maketrans({
    '\n': '\\n',
    '\r': '\\r',
    '\t': '\\t',
    '\"': '\\\"',
    '\\': '\\\\'
})


def c_escape(v: str) -> str:
  return v.translate(_C_STR_TRANS)


def main():
  parser = argparse.ArgumentParser(
      description=
      'Generate the necessary files for DomatoLPM to function properly.')
  parser.add_argument('-p',
                      '--path',
                      required=True,
                      help='The path to the template file.')
  parser.add_argument('-f',
                      '--file-format',
                      required=True,
                      help='The path (file format) where the generated files'
                      ' should be written to.')
  parser.add_argument('-n',
                      '--name',
                      required=True,
                      help='The name of the fuzzer.')

  parser.add_argument('-g', '--grammar', action='append')

  parser.add_argument('-d',
                      '--generated-dir',
                      required=True,
                      help='The path to the target gen directory.')

  args = parser.parse_args()
  template_str = ''
  with open(args.path, 'r') as f:
    template_str = f.read()

  grammars = [{
      'proto_type': repr.split(':')[0],
      'proto_name': repr.split(':')[1]
  } for repr in args.grammar]
  grammar_types = [f'<{grammar["proto_type"]}>' for grammar in grammars]

  # This splits the template into fuzzing tags, so that we know where we need
  # to insert grammar results.
  splitted_template = re.split('|'.join([f'({g})' for g in grammar_types]),
                               template_str)
  splitted_template = [a for a in splitted_template if a is not None]

  grammar_elements = []
  counter = collections.defaultdict(int)
  for elt in splitted_template:
    if elt in grammar_types:
      g = next(g for g in grammars if f'<{g["proto_type"]}>' == elt)
      g = copy.deepcopy(g)
      g['is_str'] = False
      counter[elt] += 1
      c = counter[elt]
      g['proto_field_name'] = f'{g["proto_name"]}{c}'
      grammar_elements.append(g)
    else:
      grammar_elements.append({'is_str': True, 'content': c_escape(elt)})

  template_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              'templates')
  environment = jinja2.Environment(loader=jinja2.FileSystemLoader(template_dir))
  rendering_context = {
      'template_path': args.generated_dir,
      'template_name': args.name,
      'grammars': grammars,
      'grammar_elements': grammar_elements,
  }
  template = environment.get_template('domatolpm_fuzzer.proto.tmpl')
  with action_helpers.atomic_output(f'{args.file_format}.proto', mode='w') as f:
    f.write(template.render(rendering_context))
  template = environment.get_template('domatolpm_fuzzer.h.tmpl')
  with action_helpers.atomic_output(f'{args.file_format}.h', mode='w') as f:
    f.write(template.render(rendering_context))
  template = environment.get_template('domatolpm_fuzzer.cc.tmpl')
  with action_helpers.atomic_output(f'{args.file_format}.cc', mode='w') as f:
    f.write(template.render(rendering_context))


if __name__ == '__main__':
  main()
