# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Runs the bindings code generator for the given tasks.
"""

import argparse
import sys

import web_idl
import bind_gen


def parse_output_reldirs(reldirs):
    required = ['core', 'modules']
    valid = required + ['extensions_chromeos', 'extensions_webview']
    result = {}
    for key_value_pair in reldirs:
        key, value = key_value_pair.split('=', 1)
        key = key.strip()
        result[key] = value

    for c in required:
        assert c in result, 'missing required --output_reldir "{0}"'.format(c)

    for c in result.keys():
        assert c in valid, 'invalid --output_reldir "{0}"'.format(c)

    return result


def parse_options(valid_tasks):
    parser = argparse.ArgumentParser(
        description='Generator for Blink bindings.')
    parser.add_argument('--web_idl_database',
                        required=True,
                        type=str,
                        help='filepath of the input database')
    parser.add_argument('--root_src_dir',
                        required=True,
                        type=str,
                        help='root directory of chromium project, i.e. "//"')
    parser.add_argument('--root_gen_dir',
                        required=True,
                        type=str,
                        help='root directory of generated code files, i.e. '
                        '"//out/Default/gen"')
    parser.add_argument(
        '--output_reldir',
        metavar='KEY=VALUE',
        action='append',
        help='output directory of KEY component relative to root_gen_dir.')
    parser.add_argument(
        '--format_generated_files',
        action='store_true',
        default=False,
        help=('format the resulting generated files by applying clang-format, '
              'etc.'))
    parser.add_argument(
        '--enable_code_generation_tracing',
        action='store_true',
        default=False,
        help='output debug info in generated code to help track down which '
        'line of Python code has generated which line of generated code.')
    parser.add_argument(
        '--single_process',
        action='store_true',
        default=False,
        help=('run everything in a single process, which makes debugging '
              'easier'))
    parser.add_argument('tasks',
                        nargs='+',
                        choices=valid_tasks,
                        help='types to generate')

    options = parser.parse_args()

    return options


def main():
    dispatch_table = {
        'async_iterator': bind_gen.generate_async_iterators,
        'callback_function': bind_gen.generate_callback_functions,
        'callback_interface': bind_gen.generate_callback_interfaces,
        'dictionary': bind_gen.generate_dictionaries,
        'enumeration': bind_gen.generate_enumerations,
        'interface': bind_gen.generate_interfaces,
        'namespace': bind_gen.generate_namespaces,
        'observable_array': bind_gen.generate_observable_arrays,
        'sync_iterator': bind_gen.generate_sync_iterators,
        'typedef': bind_gen.generate_typedefs,
        'union': bind_gen.generate_unions,
    }

    options = parse_options(valid_tasks=dispatch_table.keys())

    output_reldirs = parse_output_reldirs(options.output_reldir)

    component_reldirs = {}
    for component, reldir in output_reldirs.items():
        component_reldirs[web_idl.Component(component)] = reldir

    bind_gen.init(
        web_idl_database_path=options.web_idl_database,
        root_src_dir=options.root_src_dir,
        root_gen_dir=options.root_gen_dir,
        component_reldirs=component_reldirs,
        enable_style_format=options.format_generated_files,
        enable_code_generation_tracing=options.enable_code_generation_tracing)

    task_queue = bind_gen.TaskQueue(single_process=options.single_process)

    for task in options.tasks:
        dispatch_table[task](task_queue)

    def print_to_console(message):
        out = sys.stdout
        if not out.isatty():
            return
        out.write(message)
        out.flush()

    def report_progress(total, done):
        percentage = (int(float(done) / float(total) *
                          100) if total != 0 else 100)
        message = 'Blink-V8 bindings generation: {}% done\r'.format(percentage)
        print_to_console(message)

    task_queue.run(report_progress)
    print_to_console('\n')


if __name__ == '__main__':
    main()
