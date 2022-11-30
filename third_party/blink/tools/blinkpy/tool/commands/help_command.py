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

from __future__ import print_function

import optparse

from blinkpy.tool.commands.command import Command


class HelpCommand(Command):
    name = 'help'
    help_text = 'Display information about this program or its subcommands'
    argument_names = '[COMMAND]'

    def __init__(self, tool=None):
        options = [
            optparse.make_option(
                '-a',
                '--all-commands',
                action='store_true',
                dest='show_all_commands',
                help='Print all available commands'),
        ]
        super(HelpCommand, self).__init__(options)
        # A hack used to pass --all-commands to help_epilog even though it's called by the OptionParser.
        self.show_all_commands = False
        # self._tool is used in help_epilog, so it's passed in the initializer rather than set in the execute method.
        self._tool = tool

    def help_epilog(self):
        # Only show commands which are relevant to this checkout's SCM system.  Might this be confusing to some users?
        if self.show_all_commands:
            epilog = 'All %prog commands:\n'
            relevant_commands = self._tool.commands[:]
        else:
            epilog = 'Common %prog commands:\n'
            relevant_commands = list(
                filter(self._tool.should_show_in_main_help,
                       self._tool.commands))
        longest_name_length = max(
            len(command.name) for command in relevant_commands)
        relevant_commands.sort(key=lambda a: a.name)
        command_help_texts = [
            '   %s   %s\n' % (command.name.ljust(longest_name_length),
                              command.help_text)
            for command in relevant_commands
        ]
        epilog += '%s\n' % ''.join(command_help_texts)
        epilog += "See '%prog help --all-commands' to list all commands.\n"
        epilog += "See '%prog help COMMAND' for more information on a specific command.\n"
        # Use of %prog here mimics OptionParser.expand_prog_name().
        return epilog.replace('%prog', self._tool.name())

    # FIXME: This is a hack so that we don't show --all-commands as a global option:
    def _remove_help_options(self):
        for option in self.options:
            self.option_parser.remove_option(option.get_opt_string())

    def execute(self, options, args, tool):
        if args:
            command = self._tool.command_by_name(args[0])
            if command:
                print(command.standalone_help())
                return 0

        self.show_all_commands = options.show_all_commands
        self._remove_help_options()
        self.option_parser.print_help()
        return 0
