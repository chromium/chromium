# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Runs the validator of Web IDL files using web_idl.Database.
"""

import optparse

import validator
import validator.rules
import web_idl


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option("--web_idl_database",
                      type="string",
                      help="filepath of the input database")
    options, args = parser.parse_args()

    opt_name = "web_idl_database"
    if getattr(options, opt_name) is None:
        parser.error("--{} is a required option.".format(opt_name))

    return options, args


def main():
    options, args = parse_options()

    # Register rules
    rule_store = validator.RuleStore()
    validator.rules.register_all_rules(rule_store)

    # Validate
    database = web_idl.file_io.read_pickle_file(options.web_idl_database)
    validator_instance = validator.Validator(database)
    validator_instance.execute(rule_store)


if __name__ == '__main__':
    main()
