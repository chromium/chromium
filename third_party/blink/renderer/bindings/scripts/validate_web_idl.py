# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Runs the validator of Web IDL files using web_idl.Database.
"""

import optparse
import sys

import validator
import validator.rules
import web_idl


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option("--web_idl_database",
                      type="string",
                      help="filepath of the input database")
    parser.add_option("--idl_syntax_known_issues",
                      type="string",
                      help="filepath of the idl errors already known")
    parser.add_option("--output",
                      type="string",
                      help="filepath of the file for the purpose of timestamp")
    options, args = parser.parse_args()

    required_option_names = [
        "web_idl_database",
        "idl_syntax_known_issues",
    ]
    for required_option_name in required_option_names:
        if getattr(options, required_option_name) is None:
            parser.error(
                "--{} is a required option.".format(required_option_name))

    return options, args


def create_known_issues(filepath):
    known_issues = {}
    with open(filepath) as file_obj:
        for line_number, line in enumerate(file_obj):
            comment_start = line.find("#")
            if comment_start != -1:
                line = line[:comment_start]

            blocks = line.split()
            if not blocks:
                continue
            assert len(blocks) == 2, (
                "{}: {}\n"
                "A line should have exactly 2 items, "
                "RULE_NAME and DOTTED_IDL_NAME (except for a comment with '#')"
                .format(filepath, line_number + 1))
            rule_name = blocks[0]
            target_path = blocks[1]
            known_issues.setdefault(target_path, []).append(rule_name)
    return known_issues


def should_suppress_error(known_issues, rule, target_path):
    return rule.__class__.__name__ in known_issues.get(target_path, [])


def main():
    options, args = parse_options()

    known_issues = create_known_issues(options.idl_syntax_known_issues)
    error_counts = [0]
    skipped_error_counts = [0]

    def report_error(rule, target, target_type, error_message):
        error_counts[0] += 1
        target_path = target_type.get_target_path(target)
        if should_suppress_error(known_issues, rule, target_path):
            skipped_error_counts[0] += 1
        else:
            debug_infos = target_type.get_debug_info_list(target)
            sys.stderr.write("violated rule: {}\n".format(
                rule.__class__.__name__))
            sys.stderr.write("target path  : {}\n".format(target_path))
            for i, debug_info in enumerate(debug_infos):
                if i == 0:
                    sys.stderr.write("related files: {}\n".format(
                        str(debug_info.location)))
                else:
                    sys.stderr.write("               {}\n".format(
                        str(debug_info.location)))
            sys.stderr.write("error message: {}\n\n".format(error_message))

    # Register rules
    rule_store = validator.RuleStore()
    validator.rules.register_all_rules(rule_store)

    # Validate
    database = web_idl.file_io.read_pickle_file(options.web_idl_database)
    validator_instance = validator.Validator(database)
    validator_instance.execute(rule_store, report_error)

    # Report errors
    if error_counts[0] - skipped_error_counts[0] > 0:
        sys.exit("Error: Some IDL files violate the Web IDL rules")

    # Create a file for the purpose of timestamp of a successful completion.
    if options.output:
        with open(options.output, mode="w") as file_obj:
            file_obj.write("""\
# This file was created just for the purpose of timestamp of a successful
# completion, mainly in order to corporate with a build system.


""")
            file_obj.write("Command line arguments:\n")
            file_obj.write("  --web_idl_database = {}\n".format(
                options.web_idl_database))
            file_obj.write("  --idl_syntax_known_issues = {}\n".format(
                options.idl_syntax_known_issues))
            file_obj.write("Results:\n  No new error\n")


if __name__ == '__main__':
    main()
