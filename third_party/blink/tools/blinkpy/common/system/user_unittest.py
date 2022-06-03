# Copyright (C) 2010 Research in Motion Ltd. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Research in Motion Ltd. nor the names of its
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

import unittest

from blinkpy.common.system.output_capture import OutputCapture
from blinkpy.common.system.user import User


class UserTest(unittest.TestCase):
    def setUp(self):
        self.repeats_remaining = None

    def test_prompt_repeat(self):
        self.repeats_remaining = 2

        def mock_raw_input(_):
            self.repeats_remaining -= 1
            if not self.repeats_remaining:
                return 'example user response'
            return None

        self.assertEqual(
            User.prompt(
                'input',
                repeat=self.repeats_remaining,
                input_func=mock_raw_input), 'example user response')

    def test_prompt_when_exceeded_repeats(self):
        self.repeats_remaining = 2

        def mock_raw_input(_):
            self.repeats_remaining -= 1
            return None

        self.assertIsNone(
            User.prompt(
                'input',
                repeat=self.repeats_remaining,
                input_func=mock_raw_input))

    def test_prompt_with_list(self):
        def run_prompt_test(inputs, expected_result,
                            can_choose_multiple=False):
            def mock_raw_input(_):
                return inputs.pop(0)

            output_capture = OutputCapture()
            actual_result = output_capture.assert_outputs(
                self,
                User.prompt_with_list,
                args=['title', ['foo', 'bar']],
                kwargs={
                    'can_choose_multiple': can_choose_multiple,
                    'input_func': mock_raw_input
                },
                expected_stdout='title\n 1. foo\n 2. bar\n')
            self.assertEqual(actual_result, expected_result)
            self.assertEqual(len(inputs), 0)

        run_prompt_test(['1'], 'foo')
        run_prompt_test(['badinput', '2'], 'bar')

        run_prompt_test(['1,2'], ['foo', 'bar'], can_choose_multiple=True)
        run_prompt_test(['  1,  2   '], ['foo', 'bar'],
                        can_choose_multiple=True)
        run_prompt_test(['all'], ['foo', 'bar'], can_choose_multiple=True)
        run_prompt_test([''], ['foo', 'bar'], can_choose_multiple=True)
        run_prompt_test(['  '], ['foo', 'bar'], can_choose_multiple=True)
        run_prompt_test(['badinput', 'all'], ['foo', 'bar'],
                        can_choose_multiple=True)

    def check_confirm(self, expected_message, expected_out, default,
                      user_input):
        def mock_raw_input(message):
            self.assertEqual(expected_message, message)
            return user_input

        out = User().confirm(default=default, input_func=mock_raw_input)
        self.assertEqual(expected_out, out)

    def test_confirm_input_yes(self):
        self.check_confirm(
            expected_message='Continue? [Y/n]: ',
            expected_out=True,
            default=User.DEFAULT_YES,
            user_input='y')
        self.check_confirm(
            expected_message='Continue? [y/N]: ',
            expected_out=True,
            default=User.DEFAULT_NO,
            user_input=' y ')
        self.check_confirm(
            expected_message='Continue? [y/N]: ',
            expected_out=True,
            default=User.DEFAULT_NO,
            user_input='yes')
        self.check_confirm(
            expected_message='Continue? [y/N]: ',
            expected_out=True,
            default=User.DEFAULT_NO,
            user_input='y')

    def test_confirm_expect_input_no(self):
        self.check_confirm(
            expected_message='Continue? [Y/n]: ',
            expected_out=False,
            default=User.DEFAULT_YES,
            user_input='n')
        self.check_confirm(
            expected_message='Continue? [y/N]: ',
            expected_out=False,
            default=User.DEFAULT_NO,
            user_input='n')
        self.check_confirm(
            expected_message='Continue? [y/N]: ',
            expected_out=False,
            default=User.DEFAULT_NO,
            user_input=' no ')

    def test_confirm_use_default(self):
        self.check_confirm(
            expected_message='Continue? [Y/n]: ',
            expected_out=True,
            default=User.DEFAULT_YES,
            user_input='')
        self.check_confirm(
            expected_message='Continue? [y/N]: ',
            expected_out=False,
            default=User.DEFAULT_NO,
            user_input='')

    def test_confirm_not_y_means_no(self):
        self.check_confirm(
            expected_message='Continue? [Y/n]: ',
            expected_out=False,
            default=User.DEFAULT_YES,
            user_input='q')
        self.check_confirm(
            expected_message='Continue? [y/N]: ',
            expected_out=False,
            default=User.DEFAULT_NO,
            user_input='q')
