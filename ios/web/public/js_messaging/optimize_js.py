#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys
import tempfile

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules

def create_rollup_config(config_out_file, rollup_root_dir):
  """Generates a rollup config file to configure and use the path resolver
  plugin which enforces import path format."""
  plugin_path = os.path.join(os.path.abspath(_HERE_PATH),
      'rollup_plugin_src_path_resolver.js')

  config_content = f'''
      import src_path_resolver from '{plugin_path}';
      export default ({{
        plugins: [ src_path_resolver('{rollup_root_dir}') ]
      }});
      '''
  with open(config_out_file, 'w') as f:
    f.write(config_content)

def optimize_js(primary_script, js_out_file, skip_minification):
  """Creates a single output JavaScript file from |primary_script| (and any
  imported scripts) and writes it out to |js_out_file|. The output script will
  be minimized unless skip_minification is false."""
  output_script_dir = os.path.dirname(js_out_file)

  os.makedirs(output_script_dir, exist_ok=True)
  with tempfile.TemporaryDirectory(dir = output_script_dir) as tmp_out_dir:

    rollup_config_file = \
        os.path.join(tmp_out_dir, 'rollup.config.js')
    create_rollup_config(rollup_config_file, os.path.abspath(_SRC_PATH))

    rollup_processed_file = \
        os.path.join(tmp_out_dir, os.path.basename(js_out_file))

    node.RunNode([node_modules.PathToRollup(),
        primary_script,
        '--format',
        'iife',
        '--no-strict',
        '--config',
        rollup_config_file,
        '--failAfterWarnings', # Ensures script imports are correctly resolved
        '--file',
        rollup_processed_file,
        '--silent',
    ])

    with open(rollup_processed_file, 'r', encoding='utf-8') as f:
      output = f.read()
      assert "<if expr" not in output, \
          'Unexpected <if expr> found in bundled output. Check that all ' + \
          'input files using such expressions are preprocessed.'

    if skip_minification:
      shutil.copy(rollup_processed_file, js_out_file)
    else:
      node.RunNode([
          node_modules.PathToTerser(),
          rollup_processed_file,
          '--mangle',
          '--output',
          js_out_file,
      ])

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--primary-script',
      required=True,
      help="Path to the entry point javascript file to compile")
  parser.add_argument(
      '--js-out-file',
      required=True,
      help="Path to the output file for the compiled JavaScript to be written")
  parser.add_argument(
      '--skip-minification',
      default=False,
      action='store_true')
  args = parser.parse_args(argv)

  primary_script = os.path.abspath(args.primary_script)
  js_out_file = os.path.abspath(args.js_out_file)

  optimize_js(primary_script, js_out_file, args.skip_minification)

if __name__ == '__main__':
  main(sys.argv[1:])
