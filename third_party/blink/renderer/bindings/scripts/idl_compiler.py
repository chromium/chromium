#!/usr/bin/python
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Compile an .idl file to Blink V8 bindings (.h and .cpp files).

Design doc: https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/bindings/IDLCompiler.md
"""

import abc
from optparse import OptionParser
import os
import sys

from code_generator_v8 import CodeGeneratorDictionaryImpl
from code_generator_v8 import CodeGeneratorV8
from code_generator_v8 import CodeGeneratorUnionType
from code_generator_v8 import CodeGeneratorCallbackFunction
from idl_reader import IdlReader
from utilities import create_component_info_provider
from utilities import read_idl_files_list_from_file
from utilities import write_file


def parse_options():
    parser = OptionParser()
    parser.add_option(
        '--cache-directory',
        help='cache directory, defaults to output directory')
    parser.add_option('--generate-impl', action='store_true', default=False)
    parser.add_option('--generate-impl-skip-callback-function',
                      action='store_true',
                      default=False)
    parser.add_option(
        '--read-idl-list-from-file', action='store_true', default=False)
    parser.add_option('--output-directory')
    parser.add_option('--impl-output-directory')
    parser.add_option('--info-dir')
    # FIXME: We should always explicitly specify --target-component and
    # remove the default behavior.
    parser.add_option(
        '--target-component',
        type='choice',
        choices=['core', 'modules'],
        help='target component to generate code, defaults to '
        'component of input idl file')
    # ensure output comes last, so command line easy to parse via regexes
    parser.disable_interspersed_args()

    options, args = parser.parse_args()
    if options.output_directory is None:
        parser.error('Must specify output directory using --output-directory.')
    if len(args) != 1:
        parser.error(
            'Must specify exactly 1 input file as argument, but %d given.' %
            len(args))
    idl_filename = os.path.realpath(args[0])
    return options, idl_filename


class IdlCompiler(object):
    """The IDL Compiler.

    """
    __metaclass__ = abc.ABCMeta

    def __init__(self,
                 output_directory,
                 cache_directory=None,
                 code_generator_class=None,
                 info_provider=None,
                 target_component=None):
        """
        Args:
          output_directory: directory to put output files.
          cache_directory: directory which contains PLY caches.
          code_generator_class: code generator class to be used.
          info_provider: component-specific information provider.
          target_component: component to be processed.
        """
        self.cache_directory = cache_directory
        self.info_provider = info_provider
        self.output_directory = output_directory
        self.target_component = target_component
        self.reader = IdlReader(info_provider.interfaces_info, cache_directory)
        self.code_generator = code_generator_class(
            self.info_provider, self.cache_directory, self.output_directory)

    def compile_and_write(self, idl_filename):
        definitions = self.reader.read_idl_definitions(idl_filename)
        target_definitions = definitions[self.target_component]
        interface_name = target_definitions.first_name
        output_code_list = self.code_generator.generate_code(
            target_definitions, interface_name)

        # Generator may choose to omit the file.
        if output_code_list is None:
            return

        for output_path, output_code in output_code_list:
            write_file(output_code, output_path)

    def compile_file(self, idl_filename):
        self.compile_and_write(idl_filename)


def generate_bindings(code_generator_class, info_provider, options,
                      input_filenames):
    idl_compiler = IdlCompiler(
        output_directory=options.output_directory,
        cache_directory=options.cache_directory,
        code_generator_class=code_generator_class,
        info_provider=info_provider,
        target_component=options.target_component)

    for idl_filename in input_filenames:
        idl_compiler.compile_file(idl_filename)


def generate_dictionary_impl(code_generator_class, info_provider, options,
                             input_filenames):
    idl_compiler = IdlCompiler(
        output_directory=options.impl_output_directory,
        cache_directory=options.cache_directory,
        code_generator_class=code_generator_class,
        info_provider=info_provider,
        target_component=options.target_component)

    for idl_filename in input_filenames:
        idl_compiler.compile_file(idl_filename)


def generate_union_type_containers(code_generator_class, info_provider,
                                   options):
    generator = code_generator_class(info_provider, options.cache_directory,
                                     options.output_directory,
                                     options.target_component)
    output_code_list = generator.generate_code()
    for output_path, output_code in output_code_list:
        write_file(output_code, output_path)


def generate_callback_function_impl(code_generator_class, info_provider,
                                    options):
    generator = code_generator_class(info_provider, options.cache_directory,
                                     options.output_directory,
                                     options.target_component)
    output_code_list = generator.generate_code()
    for output_path, output_code in output_code_list:
        write_file(output_code, output_path)


def main():
    options, input_filename = parse_options()
    info_provider = create_component_info_provider(options.info_dir,
                                                   options.target_component)
    if options.generate_impl or options.read_idl_list_from_file:
        # |input_filename| should be a file which contains a list of IDL
        # dictionary paths.
        input_filenames = read_idl_files_list_from_file(input_filename)
    else:
        input_filenames = [input_filename]

    if options.generate_impl:
        if not info_provider.interfaces_info:
            raise Exception('Interfaces info is required to generate '
                            'impl classes')
        generate_dictionary_impl(CodeGeneratorDictionaryImpl, info_provider,
                                 options, input_filenames)
        generate_union_type_containers(CodeGeneratorUnionType, info_provider,
                                       options)
        if not options.generate_impl_skip_callback_function:
            generate_callback_function_impl(CodeGeneratorCallbackFunction,
                                            info_provider, options)
    else:
        generate_bindings(CodeGeneratorV8, info_provider, options,
                          input_filenames)


if __name__ == '__main__':
    sys.exit(main())
