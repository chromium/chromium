#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile

_SCRIPT_DIR = os.path.dirname(__file__)
_PARSER_SCRIPT = os.path.join(_SCRIPT_DIR,
                              '../public/tools/mojom/mojom_parser.py')
_GENERATOR_SCRIPT = os.path.join(
    _SCRIPT_DIR, '../public/tools/bindings/mojom_bindings_generator.py')

def removesuffix(string, suffix):
    if not suffix or not string.endswith(suffix):
        return string
    return string[:-len(suffix)]


def generate_bindings(input_dir, output_dir):
    if not os.path.isdir(output_dir):
        raise NotADirectoryError(
            f'Output directory "{output_dir}" must exist')

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_modules_dir = os.path.join(tmp_dir, 'modules')
        tmp_bytecode_dir = os.path.join(tmp_dir, 'bytecode')
        tmp_bindings_dir = os.path.join(tmp_dir, 'bindings')
        os.mkdir(tmp_modules_dir)
        os.mkdir(tmp_bytecode_dir)
        os.mkdir(tmp_bindings_dir)

        mojom_files = glob.glob(os.path.join(input_dir, '*.test-mojom'))
        subprocess.run([
            'python3', _PARSER_SCRIPT, '--input-root', input_dir,
            '--add-module-metadata', 'webui_module_path="golden://test"',
            '--output-root', tmp_modules_dir, '--mojoms', *mojom_files
        ],
                       check=True)
        subprocess.run([
            'python3', _GENERATOR_SCRIPT, '-o', tmp_bytecode_dir, 'precompile'
        ],
                       check=True)

        for lang in ['typescript', 'c++', 'java']:
          language_flags = []

          lang_tmp_output = f'{tmp_bindings_dir}/{lang}'
          lang_output = f'{output_dir}/{lang}'

          if lang == 'java':
            language_flags += ['--java_output_directory=' + lang_tmp_output]


          # Paths to module files relative to the bindings output directory.
          mojom_modules = (os.path.join('../../modules',
                            removesuffix(module_filename, '-module'))
                            for module_filename in os.listdir(tmp_modules_dir))
          subprocess.run([
              'python3', _GENERATOR_SCRIPT, '-o', lang_tmp_output, 'generate',
              '--bytecode_path', tmp_bytecode_dir, '--generators', lang,
              # typemap is hardcoded for now.
              '--typemap', f'{input_dir}/typemap.json',
              *language_flags, *mojom_modules],
                         check=True)
          # Append '.golden' file extension to avoid presubmit checks.
          for root, dirs, files in os.walk(lang_tmp_output):
              for file in files:
                path = root + '/'.join(dirs) + '/' + file
                os.rename(path, path + '.golden')
          shutil.copytree(lang_tmp_output, lang_output, dirs_exist_ok=True)


def main():
    parser = argparse.ArgumentParser(
        description='Generate mojo binding golden files.')
    parser.add_argument('--input-dir',
                        default=os.path.join(_SCRIPT_DIR, 'corpus'),
                        dest='input_dir',
                        help='directory containing input .mojom files')
    parser.add_argument(
        '--output-dir',
        default=os.path.join(_SCRIPT_DIR, 'generated/'),
        dest='output_dir',
        help='empty directory in which to write generated bindings')
    args = parser.parse_args(sys.argv[1:])

    generate_bindings(args.input_dir, args.output_dir)


if __name__ == "__main__":
    main()
