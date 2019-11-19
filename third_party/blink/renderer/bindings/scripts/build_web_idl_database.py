# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Builds Web IDL database.

Web IDL database is a Python object that supports a variety of accessors to
IDL definitions such as IDL interface and IDL attribute.
"""

import optparse
import sys

import web_idl


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--output', type='string',
                      help="filepath of the resulting database")
    parser.add_option('--runtime_enabled_features', type='string',
                      action='append',
                      help="filepath to runtime_enabled_features.json5")
    options, args = parser.parse_args()

    required_option_names = ('output', 'runtime_enabled_features')
    for opt_name in required_option_names:
        if getattr(options, opt_name) is None:
            parser.error("--{} is a required option.".format(opt_name))

    if not args:
        parser.error("No argument specified.")

    return options, args


def main():
    options, filepaths = parse_options()

    web_idl.init(
        runtime_enabled_features_paths=options.runtime_enabled_features)

    was_error_reported = [False]

    def report_error(message):
        was_error_reported[0] = True
        sys.stderr.writelines([message, "\n"])

    database = web_idl.build_database(filepaths=filepaths,
                                      report_error=report_error)

    if was_error_reported[0]:
        sys.exit("Aborted due to error.")

    database.write_to_file(options.output)


if __name__ == '__main__':
    main()
