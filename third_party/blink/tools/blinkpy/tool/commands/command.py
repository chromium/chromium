# Copyright (c) 2016 Google Inc. All rights reserved.
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

import optparse
import os
import logging
import sys
from typing import Collection, Set

from blinkpy.tool.grammar import pluralize
from blinkpy.web_tests.port.base import Port

_log = logging.getLogger(__name__)


class Command(object):
    # These class variables can be overridden in subclasses to set specific command behavior.
    name = None
    show_in_main_help = False
    help_text = None
    argument_names = None
    long_help = None

    def __init__(self, options=None, requires_local_commits=False):
        self.required_arguments = self._parse_required_arguments(
            self.argument_names)
        self.options = options
        self.requires_local_commits = requires_local_commits
        # option_parser can be overridden by the tool using set_option_parser
        # This default parser will be used for standalone_help printing.
        self.option_parser = HelpPrintingOptionParser(
            usage=optparse.SUPPRESS_USAGE,
            add_help_option=False,
            option_list=self.options)

    def _exit(self, code):
        sys.exit(code)

    # This design is slightly awkward, but we need the
    # the tool to be able to create and modify the option_parser
    # before it knows what Command to run.
    def set_option_parser(self, option_parser):
        self.option_parser = option_parser
        self._add_options_to_parser()

    def _add_options_to_parser(self):
        options = self.options or []
        for option in options:
            self.option_parser.add_option(option)

    @staticmethod
    def _parse_required_arguments(argument_names):
        required_args = []
        if not argument_names:
            return required_args
        split_args = argument_names.split(' ')
        for argument in split_args:
            if argument[0] == '[':
                # For now our parser is rather dumb.  Do some minimal validation that
                # we haven't confused it.
                if argument[-1] != ']':
                    raise Exception(
                        'Failure to parse argument string %s.  Argument %s is missing ending ]'
                        % (argument_names, argument))
            else:
                required_args.append(argument)
        return required_args

    def name_with_arguments(self):
        usage_string = self.name
        if self.options:
            usage_string += ' [options]'
        if self.argument_names:
            usage_string += ' ' + self.argument_names
        return usage_string

    def parse_args(self, args):
        return self.option_parser.parse_args(args)

    def check_arguments_and_execute(self, options, args, tool=None):
        if len(args) < len(self.required_arguments):
            _log.error(
                "%s required, %s provided.  Provided: %s  Required: %s\nSee '%s help %s' for usage.",
                pluralize('argument', len(self.required_arguments)),
                pluralize('argument', len(args)), "'%s'" % ' '.join(args),
                ' '.join(self.required_arguments), tool.name(), self.name)
            return 1
        return self.execute(options, args, tool) or 0

    def standalone_help(self):
        help_text = self.name_with_arguments().ljust(
            len(self.name_with_arguments()) + 3) + self.help_text + '\n\n'
        if self.long_help:
            help_text += '%s\n\n' % self.long_help
        help_text += self.option_parser.format_option_help(
            optparse.IndentedHelpFormatter())
        return help_text

    def execute(self, options, args, tool):
        raise NotImplementedError('subclasses must implement')

    # main() exists so that Commands can be turned into stand-alone scripts.
    # Other parts of the code will likely require modification to work stand-alone.
    def main(self, args=None):
        options, args = self.parse_args(args)
        # Some commands might require a dummy tool
        return self.check_arguments_and_execute(options, args)


class HelpPrintingOptionParser(optparse.OptionParser):
    def __init__(self, epilog_method=None, *args, **kwargs):
        self.epilog_method = epilog_method
        optparse.OptionParser.__init__(self, *args, **kwargs)

    def error(self, msg):
        self.print_usage(sys.stderr)
        error_message = '%s: error: %s\n' % (self.get_prog_name(), msg)
        # This method is overridden to add this one line to the output:
        error_message += '\nType \'%s --help\' to see usage.\n' % \
            self.get_prog_name()
        self.exit(1, error_message)

    # We override format_epilog to avoid the default formatting which would paragraph-wrap the epilog
    # and also to allow us to compute the epilog lazily instead of in the constructor (allowing it to be context sensitive).
    def format_epilog(self, epilog):  # pylint: disable=unused-argument
        if self.epilog_method:
            return '\n%s\n' % self.epilog_method()
        return ''


def check_file_option(option, _opt_str, value, parser):
    if value:
        value = os.path.expanduser(value)
        if not os.path.isfile(value):
            raise optparse.OptionValueError('%s is not a regular file.' %
                                            value)
    setattr(parser.values, option.dest, value)


def check_dir_option(option, _opt_str, value, parser):
    if value:
        value = os.path.expanduser(value)
        if not os.path.isdir(value):
            raise optparse.OptionValueError('%s is not a directory.' % value)
    setattr(parser.values, option.dest, value)


def resolve_test_patterns(port: Port,
                          test_patterns: Collection[str]) -> Set[str]:
    tests = set()
    for pattern in sorted(test_patterns):
        resolved_tests = port.tests([pattern])
        if not resolved_tests:
            _log.warning(
                '%r does not represent any tests and may be misspelled.',
                pattern)
        tests.update(resolved_tests)
    return tests
