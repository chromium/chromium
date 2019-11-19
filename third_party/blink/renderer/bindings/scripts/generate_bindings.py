# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Runs the bindings code generator for the given tasks.
"""

import optparse
import sys

import bind_gen
import web_idl


def parse_options():
    parser = optparse.OptionParser(usage="%prog [options] TASK...")
    parser.add_option('--web_idl_database', type='string',
                      help="filepath of the input database")
    parser.add_option('--output_dir_core', type='string',
                      help="outout directory for 'core' component")
    parser.add_option('--output_dir_modules', type='string',
                      help="outout directory for 'modules' component")
    options, args = parser.parse_args()

    required_option_names = (
        'web_idl_database', 'output_dir_core', 'output_dir_modules')
    for opt_name in required_option_names:
        if getattr(options, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    if not args:
        parser.error("No argument specified.")

    return options, args


def main():
    options, tasks = parse_options()

    dispatch_table = {
        'dictionary': bind_gen.generate_dictionaries,
        'example': bind_gen.run_example,
        'interface': bind_gen.generate_interfaces,
    }

    for task in tasks:
        if task not in dispatch_table:
            sys.exit("Unknown task: {}".format(task))

    web_idl_database = web_idl.Database.read_from_file(options.web_idl_database)
    output_dirs = {
        web_idl.Component('core'): options.output_dir_core,
        web_idl.Component('modules'): options.output_dir_modules,
    }

    bind_gen.init(output_dirs)

    for task in tasks:
        dispatch_table[task](web_idl_database=web_idl_database,
                             output_dirs=output_dirs)


if __name__ == '__main__':
    main()
