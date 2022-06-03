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
import unittest

from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.tool.commands.command import Command


class TrivialCommand(Command):
    name = "trivial"
    help_text = "help text"
    long_help = "trivial command long help text"
    show_in_main_help = True

    def __init__(self, **kwargs):
        super(TrivialCommand, self).__init__(**kwargs)

    def execute(self, options, args, tool):
        pass


class DummyTool(object):
    def name(self):
        return "dummy-tool"


class CommandTest(unittest.TestCase):
    def test_name_with_arguments_with_two_args(self):
        class TrivialCommandWithArgs(TrivialCommand):
            argument_names = "ARG1 ARG2"

        command = TrivialCommandWithArgs()
        self.assertEqual(command.name_with_arguments(), "trivial ARG1 ARG2")

    def test_name_with_arguments_with_options(self):
        command = TrivialCommand(options=[optparse.make_option("--my_option")])
        self.assertEqual(command.name_with_arguments(), "trivial [options]")

    def test_parse_required_arguments(self):
        # Unit test for protected method - pylint: disable=protected-access
        self.assertEqual(
            Command._parse_required_arguments("ARG1 ARG2"), ["ARG1", "ARG2"])
        self.assertEqual(
            Command._parse_required_arguments("[ARG1] [ARG2]"), [])
        self.assertEqual(
            Command._parse_required_arguments("[ARG1] ARG2"), ["ARG2"])
        # Note: We might make our arg parsing smarter in the future and allow this type of arguments string.
        with self.assertRaises(Exception):
            Command._parse_required_arguments("[ARG1 ARG2]")

    def test_required_arguments(self):
        class TrivialCommandWithRequiredAndOptionalArgs(TrivialCommand):
            argument_names = "ARG1 ARG2 [ARG3]"

        two_required_arguments = TrivialCommandWithRequiredAndOptionalArgs()
        expected_logs = (
            "2 arguments required, 1 argument provided.  Provided: 'foo'  Required: ARG1 ARG2\n"
            "See 'dummy-tool help trivial' for usage.\n")
        exit_code = OutputCapture().assert_outputs(
            self,
            two_required_arguments.check_arguments_and_execute,
            [None, ["foo"], DummyTool()],
            expected_logs=expected_logs)
        self.assertEqual(exit_code, 1)
