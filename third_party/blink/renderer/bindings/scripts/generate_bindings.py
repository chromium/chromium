# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Runs the bindings code generator for the given tasks.
"""

import optparse
import sys

import web_idl
import bind_gen


def parse_options():
    parser = optparse.OptionParser(usage="%prog [options] TASK...")
    parser.add_option(
        "--web_idl_database",
        type="string",
        help="filepath of the input database")
    parser.add_option(
        "--root_src_dir",
        type="string",
        help='root directory of chromium project, i.e. "//"')
    parser.add_option(
        "--root_gen_dir",
        type="string",
        help='root directory of generated code files, i.e. '
        '"//out/Default/gen"')
    parser.add_option(
        "--output_core_reldir",
        type="string",
        help='output directory for "core" component relative to '
        'root_gen_dir')
    parser.add_option(
        "--output_modules_reldir",
        type="string",
        help='output directory for "modules" component relative '
        'to root_gen_dir')
    parser.add_option(
        '--single_process',
        action="store_true",
        default=False,
        help=('run everything in a single process, which makes debugging '
              'easier'))
    options, args = parser.parse_args()

    required_option_names = ("web_idl_database", "root_src_dir",
                             "root_gen_dir", "output_core_reldir",
                             "output_modules_reldir")
    for opt_name in required_option_names:
        if getattr(options, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    if not args:
        parser.error("No argument specified.")

    return options, args


def main():
    options, tasks = parse_options()

    dispatch_table = {
        'callback_function': bind_gen.generate_callback_functions,
        'callback_interface': bind_gen.generate_callback_interfaces,
        'dictionary': bind_gen.generate_dictionaries,
        'enumeration': bind_gen.generate_enumerations,
        'interface': bind_gen.generate_interfaces,
    }

    for task in tasks:
        if task not in dispatch_table:
            sys.exit("Unknown task: {}".format(task))

    component_reldirs = {
        web_idl.Component('core'): options.output_core_reldir,
        web_idl.Component('modules'): options.output_modules_reldir,
    }
    bind_gen.init(web_idl_database_path=options.web_idl_database,
                  root_src_dir=options.root_src_dir,
                  root_gen_dir=options.root_gen_dir,
                  component_reldirs=component_reldirs)

    task_queue = bind_gen.TaskQueue(single_process=options.single_process)

    for task in tasks:
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
        message = "Blink-V8 bindings generation: {}% done\r".format(percentage)
        print_to_console(message)

    task_queue.run(report_progress)
    print_to_console("\n")


if __name__ == '__main__':
    main()
