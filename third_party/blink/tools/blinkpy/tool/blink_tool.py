# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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
"""blink_tool.py is a tool with multiple sub-commands with different purposes.

It has commands for printing expectations, fetching new test baselines, etc.
These commands don't necessarily have anything to do with each other.
"""

import logging
import optparse
import sys
from concurrent.futures import ThreadPoolExecutor

# pylint: disable=cyclic-import; `rebaseline_cl -> rebaseline` false positive
from blinkpy.common.host import Host
from blinkpy.tool.commands.analyze_baselines import AnalyzeBaselines
from blinkpy.tool.commands.command import HelpPrintingOptionParser
from blinkpy.tool.commands.help_command import HelpCommand
from blinkpy.tool.commands.optimize_baselines import OptimizeBaselines
from blinkpy.tool.commands.pretty_diff import PrettyDiff
from blinkpy.tool.commands.queries import CrashLog
from blinkpy.tool.commands.queries import PrintBaselines
from blinkpy.tool.commands.queries import PrintExpectations
from blinkpy.tool.commands.rebaseline import Rebaseline
from blinkpy.tool.commands.rebaseline_cl import RebaselineCL

_log = logging.getLogger(__name__)


class BlinkTool(Host):
    # FIXME: It might make more sense if this class had a Host attribute
    # instead of being a Host subclass.

    global_options = [
        optparse.make_option(
            '-v',
            '--verbose',
            action='store_true',
            dest='verbose',
            default=False,
            help='enable all logging'),
    ]

    def __init__(self, path):
        super().__init__()
        self._path = path
        io_pool = ThreadPoolExecutor(max_workers=RebaselineCL.MAX_WORKERS)
        self.commands = [
            AnalyzeBaselines(),
            CrashLog(),
            OptimizeBaselines(),
            PrettyDiff(),
            PrintBaselines(),
            PrintExpectations(),
            Rebaseline(),
            RebaselineCL(self, io_pool),
        ]
        self.help_command = HelpCommand(tool=self)
        self.commands.append(self.help_command)

    def __reduce__(self):
        return (self.__class__, (self._path, ))

    def main(self, argv=None):
        argv = argv or sys.argv
        (command_name, args) = self._split_command_name_from_args(argv[1:])

        option_parser = self._create_option_parser()
        self._add_global_options(option_parser)

        command = self.command_by_name(command_name) or self.help_command
        if not command:
            option_parser.error('%s is not a recognized command' %
                                command_name)

        command.set_option_parser(option_parser)
        (options, args) = command.parse_args(args)

        result = command.check_arguments_and_execute(options, args, self)
        return result

    def path(self):
        return self._path

    @staticmethod
    def _split_command_name_from_args(args):
        # Assume the first argument which doesn't start with "-" is the command name.
        command_index = 0
        for arg in args:
            if arg[0] != '-':
                break
            command_index += 1
        else:
            return (None, args[:])

        command = args[command_index]
        return (command, args[:command_index] + args[command_index + 1:])

    def _create_option_parser(self):
        usage = 'Usage: %prog [options] COMMAND [ARGS]'
        name = optparse.OptionParser().get_prog_name()
        return HelpPrintingOptionParser(
            epilog_method=self.help_command.help_epilog,
            prog=name,
            usage=usage)

    def _add_global_options(self, option_parser):
        global_options = self.global_options or []
        for option in global_options:
            option_parser.add_option(option)

    def name(self):
        return optparse.OptionParser().get_prog_name()

    def should_show_in_main_help(self, command):
        return command.show_in_main_help

    def command_by_name(self, command_name):
        for command in self.commands:
            if command_name == command.name:
                return command
        return None
