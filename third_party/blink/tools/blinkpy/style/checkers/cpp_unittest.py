# -*- coding: utf-8; -*-
#
# Copyright (C) 2011 Google Inc. All rights reserved.
# Copyright (C) 2009 Torch Mobile Inc.
# Copyright (C) 2009 Apple Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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
#    * Neither the name of Google Inc. nor the names of its
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

"""Unit test for cpp_style.py."""

# FIXME: Add a good test that tests UpdateIncludeState.

import os
import random
import re
import unittest

from blinkpy.common.system.filesystem import FileSystem
from blinkpy.style.checkers import cpp as cpp_style
from blinkpy.style.checkers.cpp import CppChecker
from blinkpy.style.filter import FilterConfiguration

# This class works as an error collector and replaces cpp_style.Error
# function for the unit tests.  We also verify each category we see
# is in STYLE_CATEGORIES, to help keep that list up to date.


class ErrorCollector(object):

    def __init__(self, assert_fn, filter=None, lines_to_check=None):
        """assert_fn: a function to call when we notice a problem.
           filter: filters the errors that we are concerned about.
        """
        self._assert_fn = assert_fn
        self._errors = []
        self._lines_to_check = lines_to_check
        self._all_style_categories = CppChecker.categories
        if not filter:
            filter = FilterConfiguration()
        self._filter = filter

    def __call__(self, line_number, category, confidence, message):
        self._assert_fn(category in self._all_style_categories,
                        'Message "%s" has category "%s",'
                        ' which is not in STYLE_CATEGORIES' % (message, category))

        if self._lines_to_check and not line_number in self._lines_to_check:
            return False

        if self._filter.should_check(category, ''):
            self._errors.append('%s  [%s] [%d]' % (message, category, confidence))
        return True

    def results(self):
        if len(self._errors) < 2:
            return ''.join(self._errors)  # Most tests expect to have a string.
        else:
            return self._errors  # Let's give a list if there is more than one.

    def result_list(self):
        return self._errors


class CppFunctionsTest(unittest.TestCase):

    """Supports testing functions that do not need CppStyleTestBase."""

    def test_is_c_or_objective_c(self):
        clean_lines = cpp_style.CleansedLines([''])
        clean_objc_lines = cpp_style.CleansedLines(['#import "header.h"'])
        self.assertTrue(cpp_style._FileState(clean_lines, 'c').is_c_or_objective_c())
        self.assertTrue(cpp_style._FileState(clean_lines, 'm').is_c_or_objective_c())
        self.assertFalse(cpp_style._FileState(clean_lines, 'cpp').is_c_or_objective_c())
        self.assertFalse(cpp_style._FileState(clean_lines, 'cc').is_c_or_objective_c())
        self.assertFalse(cpp_style._FileState(clean_lines, 'h').is_c_or_objective_c())
        self.assertTrue(cpp_style._FileState(clean_objc_lines, 'h').is_c_or_objective_c())

    def test_single_line_view(self):
        start_position = cpp_style.Position(row=1, column=1)
        end_position = cpp_style.Position(row=3, column=1)
        single_line_view = cpp_style.SingleLineView(['0', 'abcde', 'fgh', 'i'], start_position, end_position)
        self.assertEqual(single_line_view.single_line, 'bcde fgh i')

        start_position = cpp_style.Position(row=0, column=3)
        end_position = cpp_style.Position(row=0, column=4)
        single_line_view = cpp_style.SingleLineView(['abcdef'], start_position, end_position)
        self.assertEqual(single_line_view.single_line, 'd')

        start_position = cpp_style.Position(row=0, column=0)
        end_position = cpp_style.Position(row=3, column=2)
        single_line_view = cpp_style.SingleLineView(['""', '""', '""'], start_position, end_position)
        self.assertEqual(single_line_view.single_line, '""')


class CppStyleTestBase(unittest.TestCase):
    """Provides some useful helper functions for cpp_style tests.

    Attributes:
      min_confidence: An integer that is the current minimum confidence
                      level for the tests.
    """

    # FIXME: Refactor the unit tests so the confidence level is passed
    #        explicitly, just like it is in the real code.
    min_confidence = 1

    # Helper function to avoid needing to explicitly pass confidence
    # in all the unit test calls to cpp_style.process_file_data().
    def process_file_data(self, filename, file_extension, lines, error, fs=None):
        """Call cpp_style.process_file_data() with the min_confidence."""
        return cpp_style.process_file_data(filename, file_extension, lines,
                                           error, self.min_confidence, fs)

    def perform_lint(self, code, filename, basic_error_rules, fs=None, lines_to_check=None):
        error_collector = ErrorCollector(self.assertTrue, FilterConfiguration(basic_error_rules), lines_to_check)
        lines = code.split('\n')
        extension = filename.split('.')[1]
        self.process_file_data(filename, extension, lines, error_collector, fs)
        return error_collector.results()

    # Perform lint on single line of input and return the error message.
    def perform_single_line_lint(self, code, filename):
        basic_error_rules = ('-build/header_guard',
                             '-legal/copyright',
                             '-readability/fn_size',
                             '-readability/parameter_name',
                             '-readability/pass_ptr',
                             '-whitespace/ending_newline')
        return self.perform_lint(code, filename, basic_error_rules)

    # Perform lint over multiple lines and return the error message.
    def perform_multi_line_lint(self, code, file_extension):
        basic_error_rules = ('-build/header_guard',
                             '-legal/copyright',
                             '-readability/parameter_name',
                             '-whitespace/ending_newline')
        return self.perform_lint(code, 'test.' + file_extension, basic_error_rules)

    # Only keep some errors related to includes, namespaces and rtti.
    def perform_language_rules_check(self, filename, code, lines_to_check=None):
        basic_error_rules = ('-',
                             '+build/include',
                             '+build/include_order')
        return self.perform_lint(code, filename, basic_error_rules, lines_to_check=lines_to_check)

    # Only keep function length errors.
    def perform_function_lengths_check(self, code):
        basic_error_rules = ('-',
                             '+readability/fn_size')
        return self.perform_lint(code, 'test.cpp', basic_error_rules)

    # Only keep pass ptr errors.
    def perform_pass_ptr_check(self, code):
        basic_error_rules = ('-',
                             '+readability/pass_ptr')
        return self.perform_lint(code, 'test.cpp', basic_error_rules)

    # Only keep leaky pattern errors.
    def perform_leaky_pattern_check(self, code):
        basic_error_rules = ('-',
                             '+runtime/leaky_pattern')
        return self.perform_lint(code, 'test.cpp', basic_error_rules)

    # Only include what you use errors.
    def perform_include_what_you_use(self, code, filename='foo.h', fs=None):
        basic_error_rules = ('-',
                             '+build/include_what_you_use')
        return self.perform_lint(code, filename, basic_error_rules, fs)

    def perform_avoid_static_cast_of_objects(self, code, filename='foo.cpp', fs=None):
        basic_error_rules = ('-',
                             '+runtime/casting')
        return self.perform_lint(code, filename, basic_error_rules, fs)

    # Perform lint and compare the error message with "expected_message".
    def assert_lint(self, code, expected_message, file_name='foo.cpp'):
        self.assertEqual(expected_message, self.perform_single_line_lint(code, file_name))

    def assert_lint_one_of_many_errors_re(self, code, expected_message_re, file_name='foo.cpp'):
        messages = self.perform_single_line_lint(code, file_name)
        for message in messages:
            if re.search(expected_message_re, message):
                return

        self.assertEqual(expected_message_re, messages)

    def assert_multi_line_lint(self, code, expected_message, file_name='foo.h'):
        file_extension = file_name[file_name.rfind('.') + 1:]
        self.assertEqual(expected_message, self.perform_multi_line_lint(code, file_extension))

    def assert_multi_line_lint_re(self, code, expected_message_re, file_name='foo.h'):
        file_extension = file_name[file_name.rfind('.') + 1:]
        message = self.perform_multi_line_lint(code, file_extension)
        if not re.search(expected_message_re, message):
            self.fail('Message was:\n' + message + 'Expected match to "' + expected_message_re + '"')

    def assert_language_rules_check(self, file_name, code, expected_message, lines_to_check=None):
        self.assertEqual(expected_message,
                         self.perform_language_rules_check(file_name, code, lines_to_check))

    def assert_include_what_you_use(self, code, expected_message):
        self.assertEqual(expected_message,
                         self.perform_include_what_you_use(code))

    def assert_positions_equal(self, position, tuple_position):
        """Checks if the two positions are equal.

        position: a cpp_style.Position object.
        tuple_position: a tuple (row, column) to compare against.
        """
        self.assertEqual(position, cpp_style.Position(tuple_position[0], tuple_position[1]),
                         'position %s, tuple_position %s' % (position, tuple_position))


class FunctionDetectionTest(CppStyleTestBase):

    def perform_function_detection(self, lines, function_information, detection_line=0):
        clean_lines = cpp_style.CleansedLines(lines)
        function_state = cpp_style._FunctionState(5)
        error_collector = ErrorCollector(self.assertTrue)
        cpp_style.detect_functions(clean_lines, detection_line, function_state, error_collector)
        if not function_information:
            self.assertEqual(function_state.in_a_function, False)
            return
        self.assertEqual(function_state.in_a_function, True)
        self.assertEqual(function_state.current_function, function_information['name'] + '()')
        self.assertEqual(function_state.is_pure, function_information['is_pure'])
        self.assertEqual(function_state.is_declaration, function_information['is_declaration'])
        self.assert_positions_equal(function_state.function_name_start_position,
                                    function_information['function_name_start_position'])
        self.assert_positions_equal(function_state.parameter_start_position, function_information['parameter_start_position'])
        self.assert_positions_equal(function_state.parameter_end_position, function_information['parameter_end_position'])
        self.assert_positions_equal(function_state.body_start_position, function_information['body_start_position'])
        self.assert_positions_equal(function_state.end_position, function_information['end_position'])
        expected_parameters = function_information.get('parameter_list')
        if expected_parameters:
            actual_parameters = function_state.parameter_list()
            self.assertEqual(len(actual_parameters), len(expected_parameters))
            for index in range(len(expected_parameters)):
                actual_parameter = actual_parameters[index]
                expected_parameter = expected_parameters[index]
                self.assertEqual(actual_parameter.type, expected_parameter['type'])
                self.assertEqual(actual_parameter.name, expected_parameter['name'])
                self.assertEqual(actual_parameter.row, expected_parameter['row'])

    def test_basic_function_detection(self):
        self.perform_function_detection(
            ['void theTestFunctionName(int) {',
             '}'],
            {'name': 'theTestFunctionName',
             'function_name_start_position': (0, 5),
             'parameter_start_position': (0, 24),
             'parameter_end_position': (0, 29),
             'body_start_position': (0, 30),
             'end_position': (1, 1),
             'is_pure': False,
             'is_declaration': False})

    def test_function_declaration_detection(self):
        self.perform_function_detection(
            ['void aFunctionName(int);'],
            {'name': 'aFunctionName',
             'function_name_start_position': (0, 5),
             'parameter_start_position': (0, 18),
             'parameter_end_position': (0, 23),
             'body_start_position': (0, 23),
             'end_position': (0, 24),
             'is_pure': False,
             'is_declaration': True})

        self.perform_function_detection(
            ['CheckedInt<T> operator /(const CheckedInt<T> &lhs, const CheckedInt<T> &rhs);'],
            {'name': 'operator /',
             'function_name_start_position': (0, 14),
             'parameter_start_position': (0, 24),
             'parameter_end_position': (0, 76),
             'body_start_position': (0, 76),
             'end_position': (0, 77),
             'is_pure': False,
             'is_declaration': True})

        self.perform_function_detection(
            ['CheckedInt<T> operator -(const CheckedInt<T> &lhs, const CheckedInt<T> &rhs);'],
            {'name': 'operator -',
             'function_name_start_position': (0, 14),
             'parameter_start_position': (0, 24),
             'parameter_end_position': (0, 76),
             'body_start_position': (0, 76),
             'end_position': (0, 77),
             'is_pure': False,
             'is_declaration': True})

        self.perform_function_detection(
            ['CheckedInt<T> operator !=(const CheckedInt<T> &lhs, const CheckedInt<T> &rhs);'],
            {'name': 'operator !=',
             'function_name_start_position': (0, 14),
             'parameter_start_position': (0, 25),
             'parameter_end_position': (0, 77),
             'body_start_position': (0, 77),
             'end_position': (0, 78),
             'is_pure': False,
             'is_declaration': True})

        self.perform_function_detection(
            ['CheckedInt<T> operator +(const CheckedInt<T> &lhs, const CheckedInt<T> &rhs);'],
            {'name': 'operator +',
             'function_name_start_position': (0, 14),
             'parameter_start_position': (0, 24),
             'parameter_end_position': (0, 76),
             'body_start_position': (0, 76),
             'end_position': (0, 77),
             'is_pure': False,
             'is_declaration': True})

    def test_pure_function_detection(self):
        self.perform_function_detection(
            ['virtual void theTestFunctionName(int = 0);'],
            {'name': 'theTestFunctionName',
             'function_name_start_position': (0, 13),
             'parameter_start_position': (0, 32),
             'parameter_end_position': (0, 41),
             'body_start_position': (0, 41),
             'end_position': (0, 42),
             'is_pure': False,
             'is_declaration': True})

        self.perform_function_detection(
            ['virtual void theTestFunctionName(int) = 0;'],
            {'name': 'theTestFunctionName',
             'function_name_start_position': (0, 13),
             'parameter_start_position': (0, 32),
             'parameter_end_position': (0, 37),
             'body_start_position': (0, 41),
             'end_position': (0, 42),
             'is_pure': True,
             'is_declaration': True})

        # Hopefully, no one writes code like this but it is a tricky case.
        self.perform_function_detection(
            ['virtual void theTestFunctionName(int)',
             ' = ',
             ' 0 ;'],
            {'name': 'theTestFunctionName',
             'function_name_start_position': (0, 13),
             'parameter_start_position': (0, 32),
             'parameter_end_position': (0, 37),
             'body_start_position': (2, 3),
             'end_position': (2, 4),
             'is_pure': True,
             'is_declaration': True})

    def test_ignore_macros(self):
        self.perform_function_detection(['void aFunctionName(int); \\'], None)

    def test_non_functions(self):
        # This case exposed an error because the open brace was in quotes.
        self.perform_function_detection(
            ['asm(',
             '  "stmdb sp!, {r1-r3}" "\n"',
             ');'],
            # This isn't a function but it looks like one to our simple
            # algorithm and that is ok.
            {'name': 'asm',
             'function_name_start_position': (0, 0),
             'parameter_start_position': (0, 3),
             'parameter_end_position': (2, 1),
             'body_start_position': (2, 1),
             'end_position': (2, 2),
             'is_pure': False,
             'is_declaration': True})

        # Simple test case with something that is not a function.
        self.perform_function_detection(['class Stuff;'], None)


class CppStyleTest(CppStyleTestBase):

    def test_asm_lines_ignored(self):
        self.assert_lint(
            '__asm mov [registration], eax',
            '')

    # Test get line width.
    def test_get_line_width(self):
        self.assertEqual(0, cpp_style.get_line_width(''))
        self.assertEqual(10, cpp_style.get_line_width(u'x' * 10))
        self.assertEqual(16, cpp_style.get_line_width(u'都|道|府|県|支庁'))

    def test_find_next_multi_line_comment_start(self):
        self.assertEqual(1, cpp_style.find_next_multi_line_comment_start([''], 0))

        lines = ['a', 'b', '/* c']
        self.assertEqual(2, cpp_style.find_next_multi_line_comment_start(lines, 0))

        lines = ['char a[] = "/*";']  # not recognized as comment.
        self.assertEqual(1, cpp_style.find_next_multi_line_comment_start(lines, 0))

    def test_find_next_multi_line_comment_end(self):
        self.assertEqual(1, cpp_style.find_next_multi_line_comment_end([''], 0))
        lines = ['a', 'b', ' c */']
        self.assertEqual(2, cpp_style.find_next_multi_line_comment_end(lines, 0))

    def test_remove_multi_line_comments_from_range(self):
        lines = ['a', '  /* comment ', ' * still comment', ' comment */   ', 'b']
        cpp_style.remove_multi_line_comments_from_range(lines, 1, 4)
        self.assertEqual(['a', '// dummy', '// dummy', '// dummy', 'b'], lines)

    def test_position(self):
        position = cpp_style.Position(3, 4)
        self.assert_positions_equal(position, (3, 4))
        self.assertEqual(position.row, 3)
        self.assertTrue(position > cpp_style.Position(position.row - 1, position.column + 1))
        self.assertTrue(position > cpp_style.Position(position.row, position.column - 1))
        self.assertTrue(position < cpp_style.Position(position.row, position.column + 1))
        self.assertTrue(position < cpp_style.Position(position.row + 1, position.column - 1))
        self.assertEqual(position.__str__(), '(3, 4)')

    def test_rfind_in_lines(self):
        not_found_position = cpp_style.Position(10, 11)
        start_position = cpp_style.Position(2, 2)
        lines = ['ab', 'ace', 'test']
        self.assertEqual(not_found_position, cpp_style._rfind_in_lines('st', lines, start_position, not_found_position))
        self.assertTrue(cpp_style.Position(1, 1) == cpp_style._rfind_in_lines('a', lines, start_position, not_found_position))
        self.assertEqual(cpp_style.Position(2, 2), cpp_style._rfind_in_lines('(te|a)', lines, start_position, not_found_position))

    def test_close_expression(self):
        self.assertEqual(cpp_style.Position(1, -1), cpp_style.close_expression([')('], cpp_style.Position(0, 1)))
        self.assertEqual(cpp_style.Position(1, -1), cpp_style.close_expression([') ()'], cpp_style.Position(0, 1)))
        self.assertEqual(cpp_style.Position(0, 4), cpp_style.close_expression([')[)]'], cpp_style.Position(0, 1)))
        self.assertEqual(cpp_style.Position(0, 5), cpp_style.close_expression(['}{}{}'], cpp_style.Position(0, 3)))
        self.assertEqual(cpp_style.Position(1, 1), cpp_style.close_expression(['}{}{', '}'], cpp_style.Position(0, 3)))
        self.assertEqual(cpp_style.Position(2, -1), cpp_style.close_expression(['][][', ' '], cpp_style.Position(0, 3)))

    # Test the integer type.
    def test_precise_width_integer(self):
        errmsg = ('Use a precise-width integer type from <stdint.h> or <cstdint> such as uint16_t instead of %s')
        self.assert_lint('unsigned short a = 1', errmsg % 'unsigned short  [runtime/int] [1]')
        self.assert_lint('uint16_t unsignedshort = 1', '')
        self.assert_lint('signed  short a = 1', errmsg % 'signed  short  [runtime/int] [1]')
        self.assert_lint('short a = 1', errmsg % 'short  [runtime/int] [1]')
        self.assert_lint('unsigned   long long a = 1', errmsg % 'unsigned   long long  [runtime/int] [1]')
        self.assert_lint('signed long   long a = 1', errmsg % 'signed long   long  [runtime/int] [1]')
        self.assert_lint('long long a = 1', errmsg % 'long long  [runtime/int] [1]')
        self.assert_lint('uint64_t longlong = 1', '')
        self.assert_lint('unsigned long a = 1', errmsg % 'unsigned long  [runtime/int] [1]')
        self.assert_lint('signed   long a = 1', errmsg % 'signed   long  [runtime/int] [1]')
        self.assert_lint('long a = 1', errmsg % 'long  [runtime/int] [1]')
        self.assert_lint('signed int   long a = 1', errmsg % 'long  [runtime/int] [1]')
        self.assert_lint('unsigned   long   int a = 1', errmsg % 'unsigned   long  [runtime/int] [1]')
        self.assert_lint('unsigned longlong = 1', '')
        self.assert_lint('signed   int a = 1', '')
        self.assert_lint('int a = 1', '')

    # Test C-style cast cases.
    def test_cstyle_cast(self):
        self.assert_lint(
            'int a = (int)1.0;',
            'Using C-style cast.  Use static_cast<int>(...) instead'
            '  [readability/casting] [4]')
        self.assert_lint(
            'int *a = (int *)DEFINED_VALUE;',
            'Using C-style cast.  Use reinterpret_cast<int *>(...) instead'
            '  [readability/casting] [4]', 'foo.c')
        self.assert_lint(
            'uint16 a = (uint16)1.0;',
            'Using C-style cast.  Use static_cast<uint16>(...) instead'
            '  [readability/casting] [4]')
        self.assert_lint(
            'int32 a = (int32)1.0;',
            'Using C-style cast.  Use static_cast<int32>(...) instead'
            '  [readability/casting] [4]')
        self.assert_lint(
            'uint64 a = (uint64)1.0;',
            'Using C-style cast.  Use static_cast<uint64>(...) instead'
            '  [readability/casting] [4]')

    # Tests for static_cast readability.
    def test_static_cast_on_objects_with_toFoo(self):
        mock_header_contents = ['inline Foo* toFoo(Bar* bar)']
        fs = FileSystem()
        orig_read_text_file_fn = fs.read_text_file

        def mock_read_text_file_fn(path):
            return mock_header_contents

        try:
            fs.read_text_file = mock_read_text_file_fn
            message = self.perform_avoid_static_cast_of_objects(
                'Foo* x = static_cast<Foo*>(bar);',
                filename='casting.cpp',
                fs=fs)
            self.assertEqual(message, 'static_cast of class objects is not allowed. Use toFoo defined in Foo.h.'
                                      '  [runtime/casting] [4]')
        finally:
            fs.read_text_file = orig_read_text_file_fn

    def test_static_cast_on_objects_without_toFoo(self):
        mock_header_contents = ['inline FooBar* toFooBar(Bar* bar)']
        fs = FileSystem()
        orig_read_text_file_fn = fs.read_text_file

        def mock_read_text_file_fn(path):
            return mock_header_contents

        try:
            fs.read_text_file = mock_read_text_file_fn
            message = self.perform_avoid_static_cast_of_objects(
                'Foo* x = static_cast<Foo*>(bar);',
                filename='casting.cpp',
                fs=fs)
            self.assertEqual(message, 'static_cast of class objects is not allowed. Add toFoo in Foo.h and use it instead.'
                                      '  [runtime/casting] [4]')
        finally:
            fs.read_text_file = orig_read_text_file_fn

    # We cannot test this functionality because of difference of
    # function definitions.  Anyway, we may never enable this.
    #
    # Test for unnamed arguments in a method.
    # def test_check_for_unnamed_params(self):
    #   message = ('All parameters should be named in a function'
    #              '  [readability/function] [3]')
    #   self.assert_lint('virtual void A(int*) const;', message)
    #   self.assert_lint('virtual void B(void (*fn)(int*));', message)
    #   self.assert_lint('virtual void C(int*);', message)
    #   self.assert_lint('void *(*f)(void *) = x;', message)
    #   self.assert_lint('void Method(char*) {', message)
    #   self.assert_lint('void Method(char*);', message)
    #   self.assert_lint('void Method(char* /*x*/);', message)
    #   self.assert_lint('typedef void (*Method)(int32);', message)
    #   self.assert_lint('static void operator delete[](void*) throw();', message)
    #
    #   self.assert_lint('virtual void D(int* p);', '')
    #   self.assert_lint('void operator delete(void* x) throw();', '')
    #   self.assert_lint('void Method(char* x)\n{', '')
    #   self.assert_lint('void Method(char* /*x*/)\n{', '')
    #   self.assert_lint('void Method(char* x);', '')
    #   self.assert_lint('typedef void (*Method)(int32 x);', '')
    #   self.assert_lint('static void operator delete[](void* x) throw();', '')
    #   self.assert_lint('static void operator delete[](void* /*x*/) throw();', '')
    #
    # This one should technically warn, but doesn't because the function
    # pointer is confusing.
    #   self.assert_lint('virtual void E(void (*fn)(int* p));', '')

    # Test deprecated casts such as int(d)
    def test_deprecated_cast(self):
        self.assert_lint(
            'int a = int(2.2);',
            'Using deprecated casting style.  '
            'Use static_cast<int>(...) instead'
            '  [readability/casting] [4]')
        # Checks for false positives...
        self.assert_lint(
            'int a = int(); // Constructor, o.k.',
            '')
        self.assert_lint(
            'X::X() : a(int()) { } // default Constructor, o.k.',
            '')
        self.assert_lint(
            'operator bool(); // Conversion operator, o.k.',
            '')

    # The second parameter to a gMock method definition is a function signature
    # that often looks like a bad cast but should not picked up by lint.
    def test_mock_method(self):
        self.assert_lint(
            'MOCK_METHOD0(method, int());',
            '')
        self.assert_lint(
            'MOCK_CONST_METHOD1(method, float(string));',
            '')
        self.assert_lint(
            'MOCK_CONST_METHOD2_T(method, double(float, float));',
            '')

    # Test sizeof(type) cases.
    def test_sizeof_type(self):
        self.assert_lint(
            'sizeof(int);',
            'Using sizeof(type).  Use sizeof(varname) instead if possible'
            '  [runtime/sizeof] [1]')
        self.assert_lint(
            'sizeof(int *);',
            'Using sizeof(type).  Use sizeof(varname) instead if possible'
            '  [runtime/sizeof] [1]')

    # Test typedef cases.  There was a bug that cpp_style misidentified
    # typedef for pointer to function as C-style cast and produced
    # false-positive error messages.
    def test_typedef_for_pointer_to_function(self):
        self.assert_lint(
            'typedef void (*Func)(int x);',
            '')
        self.assert_lint(
            'typedef void (*Func)(int *x);',
            '')
        self.assert_lint(
            'typedef void Func(int x);',
            '')
        self.assert_lint(
            'typedef void Func(int *x);',
            '')

    def test_include_what_you_use_no_implementation_files(self):
        code = 'std::vector<int> foo;'
        self.assertEqual('Add #include <vector> for vector<>'
                         '  [build/include_what_you_use] [4]',
                         self.perform_include_what_you_use(code, 'foo.h'))
        self.assertEqual('',
                         self.perform_include_what_you_use(code, 'foo.cpp'))

    def test_include_what_you_use(self):
        self.assert_include_what_you_use(
            '''#include <vector>
               std::vector<int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <map>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <multimap>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <hash_map>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <utility>
               std::pair<int,int> foo;
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <vector>
               DECLARE_string(foobar);
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <vector>
               DEFINE_string(foobar, "", "");
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <vector>
               std::pair<int,int> foo;
            ''',
            'Add #include <utility> for pair<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               std::vector<int> foo;
            ''',
            'Add #include <vector> for vector<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include <vector>
               std::set<int> foo;
            ''',
            'Add #include <set> for set<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
              hash_map<int, int> foobar;
            ''',
            'Add #include <hash_map> for hash_map<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = std::less<int>(0,1);
            ''',
            'Add #include <functional> for less<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = min<int>(0,1);
            ''',
            'Add #include <algorithm> for min  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            'void a(const string &foobar);',
            'Add #include <string> for string  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = swap(0,1);
            ''',
            'Add #include <algorithm> for swap  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = transform(a.begin(), a.end(), b.start(), Foo);
            ''',
            'Add #include <algorithm> for transform  '
            '[build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include "base/foobar.h"
               bool foobar = min_element(a.begin(), a.end());
            ''',
            'Add #include <algorithm> for min_element  '
            '[build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''foo->swap(0,1);
               foo.swap(0,1);
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include <string>
               void a(const std::multimap<int,string> &foobar);
            ''',
            'Add #include <map> for multimap<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include <queue>
               void a(const std::priority_queue<int> &foobar);
            ''',
            '')
        self.assert_include_what_you_use(
            '''#include "base/basictypes.h"
                #include "base/port.h"
                #include <assert.h>
                #include <string>
                #include <vector>
                vector<string> hajoa;''', '')
        self.assert_include_what_you_use(
            '''#include <string>
               int i = numeric_limits<int>::max()
            ''',
            'Add #include <limits> for numeric_limits<>'
            '  [build/include_what_you_use] [4]')
        self.assert_include_what_you_use(
            '''#include <limits>
               int i = numeric_limits<int>::max()
            ''',
            '')

        # Test the UpdateIncludeState code path.
        mock_header_contents = ['#include "blah/foo.h"', '#include "blah/bar.h"']
        fs = FileSystem()
        orig_read_text_file_fn = fs.read_text_file

        def mock_read_text_file_fn(path):
            return mock_header_contents

        try:
            fs.read_text_file = mock_read_text_file_fn
            message = self.perform_include_what_you_use(
                '#include "config.h"\n'
                '#include "blah/a.h"\n',
                filename='blah/a.cpp',
                fs=fs)
            self.assertEqual(message, '')

            mock_header_contents = ['#include <set>']
            message = self.perform_include_what_you_use(
                '''#include "config.h"
                   #include "blah/a.h"

                   std::set<int> foo;''',
                filename='blah/a.cpp',
                fs=fs)
            self.assertEqual(message, '')

            # If there's just a .cpp and the header can't be found then it's ok.
            message = self.perform_include_what_you_use(
                '''#include "config.h"
                   #include "blah/a.h"

                   std::set<int> foo;''',
                filename='blah/a.cpp')
            self.assertEqual(message, '')

            # Make sure we find the headers with relative paths.
            mock_header_contents = ['']
            message = self.perform_include_what_you_use(
                '''#include "config.h"
                   #include "%s%sa.h"

                   std::set<int> foo;''' % (os.path.basename(os.getcwd()), os.path.sep),
                filename='a.cpp',
                fs=fs)
            self.assertEqual(message, 'Add #include <set> for set<>  '
                             '[build/include_what_you_use] [4]')
        finally:
            fs.read_text_file = orig_read_text_file_fn

    def test_files_belong_to_same_module(self):
        f = cpp_style.files_belong_to_same_module
        self.assertEqual((True, ''), f('a.cpp', 'a.h'))
        self.assertEqual((True, ''), f('base/google.cpp', 'base/google.h'))
        self.assertEqual((True, ''), f('base/google_test.cpp', 'base/google.h'))
        self.assertEqual((True, ''),
                         f('base/google_unittest.cpp', 'base/google.h'))
        self.assertEqual((True, ''),
                         f('base/internal/google_unittest.cpp',
                           'base/public/google.h'))
        self.assertEqual((True, 'xxx/yyy/'),
                         f('xxx/yyy/base/internal/google_unittest.cpp',
                           'base/public/google.h'))
        self.assertEqual((True, 'xxx/yyy/'),
                         f('xxx/yyy/base/google_unittest.cpp',
                           'base/public/google.h'))
        self.assertEqual((True, ''),
                         f('base/google_unittest.cpp', 'base/google-inl.h'))
        self.assertEqual((True, '/home/build/google3/'),
                         f('/home/build/google3/base/google.cpp', 'base/google.h'))

        self.assertEqual((False, ''),
                         f('/home/build/google3/base/google.cpp', 'basu/google.h'))
        self.assertEqual((False, ''), f('a.cpp', 'b.h'))

    def test_cleanse_line(self):
        self.assertEqual('int foo = 0;  ',
                         cpp_style.cleanse_comments('int foo = 0;  // danger!'))
        self.assertEqual('int o = 0;',
                         cpp_style.cleanse_comments('int /* foo */ o = 0;'))
        self.assertEqual('foo(int a, int b);',
                         cpp_style.cleanse_comments('foo(int a /* abc */, int b);'))
        self.assertEqual('f(a, b);',
                         cpp_style.cleanse_comments('f(a, /* name */ b);'))
        self.assertEqual('f(a, b);',
                         cpp_style.cleanse_comments('f(a /* name */, b);'))
        self.assertEqual('f(a, b);',
                         cpp_style.cleanse_comments('f(a, /* name */b);'))

    def test_raw_strings(self):
        self.assert_multi_line_lint(
            '''
            void Func() {
                static const char kString[] = R"(
                  #endif  <- invalid preprocessor should be ignored
                  */      <- invalid comment should be ignored too
                )";
            }''',
            '')
        self.assert_multi_line_lint(
            '''
            void Func() {
                const char s[] = R"TrueDelimiter(
                    )"
                    )FalseDelimiter"
                    )TrueDelimiter";
            }''',
            '')
        self.assert_multi_line_lint(
            '''
            void Func() {
                char char kString[] = R"(  ";" )";
            }''',
            '')
        self.assert_multi_line_lint(
            '''
            static const char kRawString[] = R"(
                \tstatic const int kLineWithTab = 1;
                static const int kLineWithTrailingWhiteSpace = 1;\x20

                 void WeirdNumberOfSpacesAtLineStart() {
                    string x;
                    x += StrCat("Use StrAppend instead");
                }

                void BlankLineAtEndOfBlock() {
                    // TODO incorrectly formatted
                    //Badly formatted comment

                }

            )";''',
            '')

    def test_multi_line_comments(self):
        # missing explicit is bad
        self.assert_multi_line_lint(
            r'''int a = 0;
                /* multi-liner
                class Foo {
                Foo(int f);  // should cause a lint warning in code
                }
            */ ''',
            '')
    # Test non-explicit single-argument constructors
    def test_explicit_single_argument_constructors(self):
        # missing explicit is bad
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(int f);
            };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # missing explicit is bad, even with whitespace
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo (int f);
            };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # missing explicit, with distracting comment, is still bad
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(int f); // simpler than Foo(blargh, blarg)
            };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # missing explicit, with qualified classname
        self.assert_multi_line_lint(
            '''\
            class Qualifier::AnotherOne::Foo {
                Foo(int f);
            };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # structs are caught as well.
        self.assert_multi_line_lint(
            '''\
            struct Foo {
                Foo(int f);
            };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # Templatized classes are caught as well.
        self.assert_multi_line_lint(
            '''\
            template<typename T> class Foo {
                Foo(int f);
            };''',
            'Single-argument constructors should be marked explicit.'
            '  [runtime/explicit] [5]')
        # proper style is okay
        self.assert_multi_line_lint(
            '''\
            class Foo {
                explicit Foo(int f);
            };''',
            '')
        # two argument constructor is okay
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(int f, int b);
            };''',
            '')
        # two argument constructor, across two lines, is okay
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(int f,
                    int b);
            };''',
            '')
        # non-constructor (but similar name), is okay
        self.assert_multi_line_lint(
            '''\
            class Foo {
                aFoo(int f);
            };''',
            '')
        # constructor with void argument is okay
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(void);
            };''',
            '')
        # single argument method is okay
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Bar(int b);
            };''',
            '')
        # comments should be ignored
        self.assert_multi_line_lint(
            '''\
            class Foo {
            // Foo(int f);
            };''',
            '')
        # single argument function following class definition is okay
        # (okay, it's not actually valid, but we don't want a false positive)
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(int f, int b);
            };
            Foo(int f);''',
            '')
        # single argument function is okay
        self.assert_multi_line_lint(
            '''static Foo(int f);''',
            '')
        # single argument copy constructor is okay.
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(const Foo&);
            };''',
            '')
        self.assert_multi_line_lint(
            '''\
            class Foo {
                Foo(Foo&);
            };''',
            '')

    def test_slash_star_comment_on_single_line(self):
        self.assert_multi_line_lint(
            '''/* static */ Foo(int f);''',
            '')
        self.assert_multi_line_lint(
            '''/*/ static */  Foo(int f);''',
            '')

    # Test suspicious usage of memset. Specifically, a 0
    # as the final argument is almost certainly an error.
    def test_suspicious_usage_of_memset(self):
        # Normal use is okay.
        self.assert_lint(
            '  memset(buf, 0, sizeof(buf))',
            '')

        # A 0 as the final argument is almost certainly an error.
        self.assert_lint(
            '  memset(buf, sizeof(buf), 0)',
            'Did you mean "memset(buf, 0, sizeof(buf))"?'
            '  [runtime/memset] [4]')
        self.assert_lint(
            '  memset(buf, xsize * ysize, 0)',
            'Did you mean "memset(buf, 0, xsize * ysize)"?'
            '  [runtime/memset] [4]')

        # There is legitimate test code that uses this form.
        # This is okay since the second argument is a literal.
        self.assert_lint(
            "    memset(buf, 'y', 0)",
            '')
        self.assert_lint(
            '  memset(buf, 4, 0)',
            '')
        self.assert_lint(
            '  memset(buf, -1, 0)',
            '')
        self.assert_lint(
            '  memset(buf, 0xF1, 0)',
            '')
        self.assert_lint(
            '  memset(buf, 0xcd, 0)',
            '')

    def test_check_posix_threading(self):
        self.assert_lint('sctime_r()', '')
        self.assert_lint('strtok_r()', '')
        self.assert_lint('  strtok_r(foo, ba, r)', '')
        self.assert_lint('brand()', '')
        self.assert_lint('_rand()', '')
        self.assert_lint('.rand()', '')
        self.assert_lint('>rand()', '')
        self.assert_lint('rand()',
                         'Consider using rand_r(...) instead of rand(...)'
                         ' for improved thread safety.'
                         '  [runtime/threadsafe_fn] [2]')
        self.assert_lint('strtok()',
                         'Consider using strtok_r(...) '
                         'instead of strtok(...)'
                         ' for improved thread safety.'
                         '  [runtime/threadsafe_fn] [2]')

    # Test potential format string bugs like printf(foo).
    def test_format_strings(self):
        self.assert_lint('printf("foo")', '')
        self.assert_lint('printf("foo: %s", foo)', '')
        self.assert_lint('DocidForPrintf(docid)', '')  # Should not trigger.
        self.assert_lint(
            'printf(foo)',
            'Potential format string bug. Do printf("%s", foo) instead.'
            '  [runtime/printf] [4]')
        self.assert_lint(
            'printf(foo.c_str())',
            'Potential format string bug. '
            'Do printf("%s", foo.c_str()) instead.'
            '  [runtime/printf] [4]')
        self.assert_lint(
            'printf(foo->c_str())',
            'Potential format string bug. '
            'Do printf("%s", foo->c_str()) instead.'
            '  [runtime/printf] [4]')
        self.assert_lint(
            'StringPrintf(foo)',
            'Potential format string bug. Do StringPrintf("%s", foo) instead.'
            ''
            '  [runtime/printf] [4]')

    # Variable-length arrays are not permitted.
    def test_variable_length_array_detection(self):
        errmsg = ('Do not use variable-length arrays.  Use an appropriately named '
                  "('k' followed by CamelCase) compile-time constant for the size."
                  '  [runtime/arrays] [1]')

        self.assert_lint('int a[any_old_variable];', errmsg)
        self.assert_lint('int doublesize[some_var * 2];', errmsg)
        self.assert_lint('int a[afunction()];', errmsg)
        self.assert_lint('int a[function(kMaxFooBars)];', errmsg)
        self.assert_lint('bool aList[items_->size()];', errmsg)
        self.assert_lint('namespace::Type buffer[len+1];', errmsg)

        self.assert_lint('int a[64];', '')
        self.assert_lint('int a[0xFF];', '')
        self.assert_lint('int first[256], second[256];', '')
        self.assert_lint('int arrayName[kCompileTimeConstant];', '')
        self.assert_lint('char buf[somenamespace::kBufSize];', '')
        self.assert_lint('int arrayName[ALL_CAPS];', '')
        self.assert_lint('AClass array1[foo::bar::ALL_CAPS];', '')
        self.assert_lint('int a[kMaxStrLen + 1];', '')
        self.assert_lint('int a[sizeof(foo)];', '')
        self.assert_lint('int a[sizeof(*foo)];', '')
        self.assert_lint('int a[sizeof foo];', '')
        self.assert_lint('int a[sizeof(struct Foo)];', '')
        self.assert_lint('int a[128 - sizeof(const bar)];', '')
        self.assert_lint('int a[(sizeof(foo) * 4)];', '')
        self.assert_lint('delete a[some_var];', '')
        self.assert_lint('return a[some_var];', '')

    # Brace usage
    def test_braces(self):
        # Braces shouldn't be followed by a ; unless they're defining a struct
        # or initializing an array
        self.assert_lint('int a[3] = { 1, 2, 3 };', '')
        self.assert_lint(
            '''\
            const int foo[] =
                {1, 2, 3 };''',
            '')
        # For single line, unmatched '}' with a ';' is ignored (not enough context)
        self.assert_multi_line_lint(
            '''\
            int a[3] = { 1,
                2,
                3 };''',
            '')
        self.assert_multi_line_lint(
            '''\
            int a[2][3] = { { 1, 2 },
                { 3, 4 } };''',
            '')
        self.assert_multi_line_lint(
            '''\
            int a[2][3] =
                { { 1, 2 },
                { 3, 4 } };''',
            '')

    # CHECK/EXPECT_TRUE/EXPECT_FALSE replacements
    def test_check_check(self):
        self.assert_lint('CHECK(x == 42)',
                         'Consider using CHECK_EQ(a, b) instead of CHECK(a == b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x != 42)',
                         'Consider using CHECK_NE(a, b) instead of CHECK(a != b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x >= 42)',
                         'Consider using CHECK_GE(a, b) instead of CHECK(a >= b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x > 42)',
                         'Consider using CHECK_GT(a, b) instead of CHECK(a > b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x <= 42)',
                         'Consider using CHECK_LE(a, b) instead of CHECK(a <= b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x < 42)',
                         'Consider using CHECK_LT(a, b) instead of CHECK(a < b)'
                         '  [readability/check] [2]')

        self.assert_lint('DCHECK(x == 42)',
                         'Consider using DCHECK_EQ(a, b) instead of DCHECK(a == b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x != 42)',
                         'Consider using DCHECK_NE(a, b) instead of DCHECK(a != b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x >= 42)',
                         'Consider using DCHECK_GE(a, b) instead of DCHECK(a >= b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x > 42)',
                         'Consider using DCHECK_GT(a, b) instead of DCHECK(a > b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x <= 42)',
                         'Consider using DCHECK_LE(a, b) instead of DCHECK(a <= b)'
                         '  [readability/check] [2]')
        self.assert_lint('DCHECK(x < 42)',
                         'Consider using DCHECK_LT(a, b) instead of DCHECK(a < b)'
                         '  [readability/check] [2]')

        self.assert_lint(
            'EXPECT_TRUE("42" == x)',
            'Consider using EXPECT_EQ(a, b) instead of EXPECT_TRUE(a == b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE("42" != x)',
            'Consider using EXPECT_NE(a, b) instead of EXPECT_TRUE(a != b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE(+42 >= x)',
            'Consider using EXPECT_GE(a, b) instead of EXPECT_TRUE(a >= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE_M(-42 > x)',
            'Consider using EXPECT_GT_M(a, b) instead of EXPECT_TRUE_M(a > b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE_M(42U <= x)',
            'Consider using EXPECT_LE_M(a, b) instead of EXPECT_TRUE_M(a <= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE_M(42L < x)',
            'Consider using EXPECT_LT_M(a, b) instead of EXPECT_TRUE_M(a < b)'
            '  [readability/check] [2]')

        self.assert_lint(
            'EXPECT_FALSE(x == 42)',
            'Consider using EXPECT_NE(a, b) instead of EXPECT_FALSE(a == b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_FALSE(x != 42)',
            'Consider using EXPECT_EQ(a, b) instead of EXPECT_FALSE(a != b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_FALSE(x >= 42)',
            'Consider using EXPECT_LT(a, b) instead of EXPECT_FALSE(a >= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'ASSERT_FALSE(x > 42)',
            'Consider using ASSERT_LE(a, b) instead of ASSERT_FALSE(a > b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'ASSERT_FALSE(x <= 42)',
            'Consider using ASSERT_GT(a, b) instead of ASSERT_FALSE(a <= b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'ASSERT_FALSE_M(x < 42)',
            'Consider using ASSERT_GE_M(a, b) instead of ASSERT_FALSE_M(a < b)'
            '  [readability/check] [2]')

        self.assert_lint('CHECK(some_iterator == obj.end())', '')
        self.assert_lint('EXPECT_TRUE(some_iterator == obj.end())', '')
        self.assert_lint('EXPECT_FALSE(some_iterator == obj.end())', '')

        self.assert_lint('CHECK(CreateTestFile(dir, (1 << 20)));', '')
        self.assert_lint('CHECK(CreateTestFile(dir, (1 >> 20)));', '')

        self.assert_lint('CHECK(x<42)',
                         'Consider using CHECK_LT(a, b) instead of CHECK(a < b)'
                         '  [readability/check] [2]')
        self.assert_lint('CHECK(x>42)',
                         'Consider using CHECK_GT(a, b) instead of CHECK(a > b)'
                         '  [readability/check] [2]')

        self.assert_lint(
            '  EXPECT_TRUE(42 < x) // Random comment.',
            'Consider using EXPECT_LT(a, b) instead of EXPECT_TRUE(a < b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'EXPECT_TRUE( 42 < x )',
            'Consider using EXPECT_LT(a, b) instead of EXPECT_TRUE(a < b)'
            '  [readability/check] [2]')
        self.assert_lint(
            'CHECK("foo" == "foo")',
            'Consider using CHECK_EQ(a, b) instead of CHECK(a == b)'
            '  [readability/check] [2]')

        self.assert_lint('CHECK_EQ("foo", "foo")', '')

    def test_no_spaces_in_function_calls(self):
        self.assert_lint('TellStory(1, 3);',
                         '')
        self.assert_lint('TellStory(1 /* wolf */, 3 /* pigs */);',
                         '')
        self.assert_multi_line_lint('#endif\n    );',
                                    '')

    def test_invalid_utf8(self):
        def do_test(self, raw_bytes, has_invalid_utf8):
            error_collector = ErrorCollector(self.assertTrue)
            self.process_file_data('foo.cpp', 'cpp',
                                   unicode(raw_bytes, 'utf8', 'replace').split('\n'),
                                   error_collector)
            # The warning appears only once.
            self.assertEqual(
                int(has_invalid_utf8),
                error_collector.results().count(
                    'Line contains invalid UTF-8'
                    ' (or Unicode replacement character).'
                    '  [readability/utf8] [5]'))

        do_test(self, 'Hello world\n', False)
        do_test(self, '\xe9\x8e\xbd\n', False)
        do_test(self, '\xe9x\x8e\xbd\n', True)
        # This is the encoding of the replacement character itself (which
        # you can see by evaluating codecs.getencoder('utf8')(u'\ufffd')).
        do_test(self, '\xef\xbf\xbd\n', True)

    def test_is_blank_line(self):
        self.assertTrue(cpp_style.is_blank_line(''))
        self.assertTrue(cpp_style.is_blank_line(' '))
        self.assertTrue(cpp_style.is_blank_line(' \t\r\n'))
        self.assertTrue(not cpp_style.is_blank_line('int a;'))
        self.assertTrue(not cpp_style.is_blank_line('{'))

    def test_not_alabel(self):
        self.assert_lint('MyVeryLongNamespace::MyVeryLongClassName::', '')

    def test_build_header_guard(self):
        file_path = 'mydir/Foo.h'

        # We can't rely on our internal stuff to get a sane path on the open source
        # side of things, so just parse out the suggested header guard. This
        # doesn't allow us to test the suggested header guard, but it does let us
        # test all the other header tests.
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'h', [], error_collector)
        expected_guard = ''
        matcher = re.compile(
            r'No \#ifndef header guard found\, suggested CPP variable is\: ([A-Za-z_0-9]+) ')
        for error in error_collector.result_list():
            matches = matcher.match(error)
            if matches:
                expected_guard = matches.group(1)
                break

        # Make sure we extracted something for our header guard.
        self.assertNotEqual(expected_guard, '')

        # Wrong guard
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'h',
                               ['#ifndef FOO_H', '#define FOO_H'], error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(
                '#ifndef header guard has wrong style, please use: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # No define
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'h',
                               ['#ifndef %s' % expected_guard], error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(
                'No #ifndef header guard found, suggested CPP variable is: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # Mismatched define
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'h',
                               ['#ifndef %s' % expected_guard,
                                '#define FOO_H'],
                               error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(
                'No #ifndef header guard found, suggested CPP variable is: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # No header guard errors
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'h',
                               ['#ifndef %s' % expected_guard,
                                '#define %s' % expected_guard,
                                '#endif // %s' % expected_guard],
                               error_collector)
        for line in error_collector.result_list():
            if line.find('build/header_guard') != -1:
                self.fail('Unexpected error: %s' % line)

        # Completely incorrect header guard
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'h',
                               ['#ifndef FOO',
                                '#define FOO',
                                '#endif  // FOO'],
                               error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(
                '#ifndef header guard has wrong style, please use: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # Special case for flymake
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data('mydir/Foo_flymake.h', 'h',
                               ['#ifndef %s' % expected_guard,
                                '#define %s' % expected_guard,
                                '#endif // %s' % expected_guard],
                               error_collector)
        for line in error_collector.result_list():
            if line.find('build/header_guard') != -1:
                self.fail('Unexpected error: %s' % line)

        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data('mydir/Foo_flymake.h', 'h', [], error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(
                'No #ifndef header guard found, suggested CPP variable is: %s'
                '  [build/header_guard] [5]' % expected_guard),
            error_collector.result_list())

        # Verify that we don't blindly suggest the WTF prefix for all headers.
        self.assertFalse(expected_guard.startswith('WTF_'))

        # Verify that the Chromium-style header guard is allowed.
        header_guard_filter = FilterConfiguration(('-', '+build/header_guard'))
        error_collector = ErrorCollector(self.assertTrue, header_guard_filter)
        self.process_file_data('Source/foo/testname.h', 'h',
                               ['#ifndef SOURCE_FOO_TESTNAME_H_',
                                '#define SOURCE_FOO_TESTNAME_H_'],
                               error_collector)
        self.assertEqual(0, len(error_collector.result_list()),
                         error_collector.result_list())

        # Verify that we suggest the Chromium-style header guard.
        error_collector = ErrorCollector(self.assertTrue, header_guard_filter)
        self.process_file_data('renderer/platform/wtf/auto_reset.h', 'h',
                               ['#ifndef BAD_auto_reset_h', '#define BAD_auto_reset_h'],
                               error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(
                '#ifndef header guard has wrong style, please use: '
                'RENDERER_PLATFORM_WTF_AUTO_RESET_H_  [build/header_guard] [5]'),
            error_collector.result_list())

    def assert_lintLogCodeOnError(self, code, expected_message):
        # Special assert_lint which logs the input code on error.
        result = self.perform_single_line_lint(code, 'foo.cpp')
        if result != expected_message:
            self.fail('For code: "%s"\nGot: "%s"\nExpected: "%s"'
                      % (code, result, expected_message))

    def test_legal_copyright(self):
        legal_copyright_message = (
            'No copyright message found.  '
            'You should have a line: "Copyright [year] <Copyright Owner>"'
            '  [legal/copyright] [5]')

        copyright_line = '// Copyright 2008 Google Inc. All Rights Reserved.'

        file_path = 'mydir/googleclient/foo.cpp'

        # There should be a copyright message in the first 10 lines
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'cpp', [], error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(legal_copyright_message))

        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(
            file_path, 'cpp',
            ['' for _ in range(10)] + [copyright_line],
            error_collector)
        self.assertEqual(
            1,
            error_collector.result_list().count(legal_copyright_message))

        # Test that warning isn't issued if Copyright line appears early enough.
        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(file_path, 'cpp', [copyright_line], error_collector)
        for message in error_collector.result_list():
            if message.find('legal/copyright') != -1:
                self.fail('Unexpected error: %s' % message)

        error_collector = ErrorCollector(self.assertTrue)
        self.process_file_data(
            file_path, 'cpp',
            ['' for _ in range(9)] + [copyright_line],
            error_collector)
        for message in error_collector.result_list():
            if message.find('legal/copyright') != -1:
                self.fail('Unexpected error: %s' % message)

    def test_invalid_increment(self):
        self.assert_lint('*count++;',
                         'Changing pointer instead of value (or unused value of '
                         'operator*).  [runtime/invalid_increment] [5]')

    # Integral bitfields must be declared with either signed or unsigned keyword.
    def test_plain_integral_bitfields(self):
        errmsg = ('Please declare integral type bitfields with either signed or unsigned.  [runtime/bitfields] [5]')

        self.assert_lint('int a : 30;', errmsg)
        self.assert_lint('mutable int a : 14;', errmsg)
        self.assert_lint('const char a : 6;', errmsg)
        self.assert_lint('int a = 1 ? 0 : 30;', '')

    # Bitfields which are not declared unsigned or bool will generate a warning.
    def test_unsigned_bool_bitfields(self):
        def errmsg(member, name, bit_type):
            return ('Member %s of class %s defined as a bitfield of type %s. '
                    'Please declare all bitfields as unsigned.  [runtime/bitfields] [4]'
                    % (member, name, bit_type))

        def warning_bitfield_test(member, name, bit_type, bits):
            self.assert_multi_line_lint('class %s {\n%s %s: %d;\n}\n'
                                        % (name, bit_type, member, bits),
                                        errmsg(member, name, bit_type))

        def safe_bitfield_test(member, name, bit_type, bits):
            self.assert_multi_line_lint('class %s {\n%s %s: %d;\n}\n'
                                        % (name, bit_type, member, bits),
                                        '')

        warning_bitfield_test('a', 'A', 'int32_t', 25)
        warning_bitfield_test('m_someField', 'SomeClass', 'signed', 4)
        warning_bitfield_test('m_someField', 'SomeClass', 'SomeEnum', 2)

        safe_bitfield_test('a', 'A', 'unsigned', 22)
        safe_bitfield_test('m_someField', 'SomeClass', 'bool', 1)
        safe_bitfield_test('m_someField', 'SomeClass', 'unsigned', 2)

        # Declarations in 'Expected' or 'SameSizeAs' classes are OK.
        warning_bitfield_test('m_bitfields', 'SomeClass', 'int32_t', 32)
        safe_bitfield_test('m_bitfields', 'ExpectedSomeClass', 'int32_t', 32)
        safe_bitfield_test('m_bitfields', 'SameSizeAsSomeClass', 'int32_t', 32)


class CleansedLinesTest(unittest.TestCase):

    def test_init(self):
        lines = ['Line 1',
                 'Line 2',
                 'Line 3 // Comment test',
                 'Line 4 "foo"']

        clean_lines = cpp_style.CleansedLines(lines)
        self.assertEqual(lines, clean_lines.raw_lines)
        self.assertEqual(4, clean_lines.num_lines())

        self.assertEqual(['Line 1',
                          'Line 2',
                          'Line 3 ',
                          'Line 4 "foo"'],
                         clean_lines.lines)

        self.assertEqual(['Line 1',
                          'Line 2',
                          'Line 3 ',
                          'Line 4 ""'],
                         clean_lines.elided)

    def test_init_empty(self):
        clean_lines = cpp_style.CleansedLines([])
        self.assertEqual([], clean_lines.raw_lines)
        self.assertEqual(0, clean_lines.num_lines())

    def test_collapse_strings(self):
        collapse = cpp_style.CleansedLines.collapse_strings
        self.assertEqual('""', collapse('""'))             # ""     (empty)
        self.assertEqual('"""', collapse('"""'))           # """    (bad)
        self.assertEqual('""', collapse('"xyz"'))          # "xyz"  (string)
        self.assertEqual('""', collapse('"\\\""'))         # "\""   (string)
        self.assertEqual('""', collapse('"\'"'))           # "'"    (string)
        self.assertEqual('"\"', collapse('"\"'))           # "\"    (bad)
        self.assertEqual('""', collapse('"\\\\"'))         # "\\"   (string)
        self.assertEqual('"', collapse('"\\\\\\"'))        # "\\\"  (bad)
        self.assertEqual('""', collapse('"\\\\\\\\"'))     # "\\\\" (string)

        self.assertEqual('\'\'', collapse('\'\''))         # ''     (empty)
        self.assertEqual('\'\'', collapse('\'a\''))        # 'a'    (char)
        self.assertEqual('\'\'', collapse('\'\\\'\''))     # '\''   (char)
        self.assertEqual('\'', collapse('\'\\\''))         # '\'    (bad)
        self.assertEqual('', collapse('\\012'))            # '\012' (char)
        self.assertEqual('', collapse('\\xfF0'))           # '\xfF0' (char)
        self.assertEqual('', collapse('\\n'))              # '\n' (char)
        self.assertEqual('\\#', collapse('\\#'))           # '\#' (bad)

        self.assertEqual('StringReplace(body, "", "");',
                         collapse('StringReplace(body, "\\\\", "\\\\\\\\");'))
        self.assertEqual('\'\' ""',
                         collapse('\'"\' "foo"'))
        self.assertEqual('""', collapse('"a" "b" "c"'))


class OrderOfIncludesTest(CppStyleTestBase):

    def setUp(self):
        self.include_state = cpp_style._IncludeState()

        # Cheat os.path.abspath called in FileInfo class.
        self.os_path_abspath_orig = os.path.abspath
        os.path.abspath = lambda value: value

    def tearDown(self):
        os.path.abspath = self.os_path_abspath_orig

    def test_try_drop_common_suffixes(self):
        self.assertEqual('foo/foo', cpp_style._drop_common_suffixes('foo/foo-inl.h'))
        self.assertEqual('foo/bar/foo',
                         cpp_style._drop_common_suffixes('foo/bar/foo_inl.h'))
        self.assertEqual('foo/foo', cpp_style._drop_common_suffixes('foo/foo.cpp'))
        self.assertEqual('foo/foo_unusualinternal',
                         cpp_style._drop_common_suffixes('foo/foo_unusualinternal.h'))
        self.assertEqual('',
                         cpp_style._drop_common_suffixes('_test.cpp'))
        self.assertEqual('test',
                         cpp_style._drop_common_suffixes('test.cpp'))


class OrderOfIncludesTest(CppStyleTestBase):

    def setUp(self):
        self.include_state = cpp_style._IncludeState()

        # Cheat os.path.abspath called in FileInfo class.
        self.os_path_abspath_orig = os.path.abspath
        self.os_path_isfile_orig = os.path.isfile
        os.path.abspath = lambda value: value

    def tearDown(self):
        os.path.abspath = self.os_path_abspath_orig
        os.path.isfile = self.os_path_isfile_orig


class CheckForFunctionLengthsTest(CppStyleTestBase):

    def setUp(self):
        # Reducing these thresholds for the tests speeds up tests significantly.
        self.old_normal_trigger = cpp_style._FunctionState._NORMAL_TRIGGER
        self.old_test_trigger = cpp_style._FunctionState._TEST_TRIGGER

        cpp_style._FunctionState._NORMAL_TRIGGER = 10
        cpp_style._FunctionState._TEST_TRIGGER = 25

    def tearDown(self):
        cpp_style._FunctionState._NORMAL_TRIGGER = self.old_normal_trigger
        cpp_style._FunctionState._TEST_TRIGGER = self.old_test_trigger

    # FIXME: Eliminate the need for this function.
    def set_min_confidence(self, min_confidence):
        """Set new test confidence and return old test confidence."""
        old_min_confidence = self.min_confidence
        self.min_confidence = min_confidence
        return old_min_confidence

    def assert_function_lengths_check(self, code, expected_message):
        """Check warnings for long function bodies are as expected.

        Args:
          code: C++ source code expected to generate a warning message.
          expected_message: Message expected to be generated by the C++ code.
        """
        self.assertEqual(expected_message,
                         self.perform_function_lengths_check(code))

    def trigger_lines(self, error_level):
        """Return number of lines needed to trigger a function length warning.

        Args:
          error_level: --v setting for cpp_style.

        Returns:
          Number of lines needed to trigger a function length warning.
        """
        return cpp_style._FunctionState._NORMAL_TRIGGER * 2 ** error_level

    def trigger_test_lines(self, error_level):
        """Return number of lines needed to trigger a test function length warning.

        Args:
          error_level: --v setting for cpp_style.

        Returns:
          Number of lines needed to trigger a test function length warning.
        """
        return cpp_style._FunctionState._TEST_TRIGGER * 2 ** error_level

    def assert_function_length_check_definition(self, lines, error_level):
        """Generate long function definition and check warnings are as expected.

        Args:
          lines: Number of lines to generate.
          error_level:  --v setting for cpp_style.
        """
        trigger_level = self.trigger_lines(self.min_confidence)
        self.assert_function_lengths_check(
            'void test(int x)' + self.function_body(lines),
            ('Small and focused functions are preferred: '
             'test() has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]'
             % (lines, trigger_level, error_level)))

    def assert_function_length_check_definition_ok(self, lines):
        """Generate shorter function definition and check no warning is produced.

        Args:
          lines: Number of lines to generate.
        """
        self.assert_function_lengths_check(
            'void test(int x)' + self.function_body(lines),
            '')

    def assert_function_length_check_at_error_level(self, error_level):
        """Generate and check function at the trigger level for --v setting.

        Args:
          error_level: --v setting for cpp_style.
        """
        self.assert_function_length_check_definition(self.trigger_lines(error_level),
                                                     error_level)

    def assert_function_length_check_below_error_level(self, error_level):
        """Generate and check function just below the trigger level for --v setting.

        Args:
          error_level: --v setting for cpp_style.
        """
        self.assert_function_length_check_definition(self.trigger_lines(error_level) - 1,
                                                     error_level - 1)

    def assert_function_length_check_above_error_level(self, error_level):
        """Generate and check function just above the trigger level for --v setting.

        Args:
          error_level: --v setting for cpp_style.
        """
        self.assert_function_length_check_definition(self.trigger_lines(error_level) + 1,
                                                     error_level)

    def function_body(self, number_of_lines):
        return ' {\n' + '  this_is_just_a_test();\n' * number_of_lines + '}'

    def function_body_with_no_lints(self, number_of_lines):
        return ' {\n' + '  this_is_just_a_test();  // NOLINT\n' * number_of_lines + '}'

    # Test line length checks.
    def test_function_length_check_declaration(self):
        self.assert_function_lengths_check(
            'void test();',  # Not a function definition
            '')

    def test_function_length_check_declaration_with_block_following(self):
        self.assert_function_lengths_check(
            ('void test();\n'
             + self.function_body(66)),  # Not a function definition
            '')

    def test_function_length_check_class_definition(self):
        self.assert_function_lengths_check(  # Not a function definition
            'class Test' + self.function_body(66) + ';',
            '')

    def test_function_length_check_trivial(self):
        self.assert_function_lengths_check(
            'void test() {}',  # Not counted
            '')

    def test_function_length_check_empty(self):
        self.assert_function_lengths_check(
            'void test() {\n}',
            '')

    def test_function_length_check_definition_below_severity0(self):
        old_min_confidence = self.set_min_confidence(0)
        self.assert_function_length_check_definition_ok(self.trigger_lines(0) - 1)
        self.set_min_confidence(old_min_confidence)

    def test_function_length_check_definition_at_severity0(self):
        old_min_confidence = self.set_min_confidence(0)
        self.assert_function_length_check_definition_ok(self.trigger_lines(0))
        self.set_min_confidence(old_min_confidence)

    def test_function_length_check_definition_above_severity0(self):
        old_min_confidence = self.set_min_confidence(0)
        self.assert_function_length_check_above_error_level(0)
        self.set_min_confidence(old_min_confidence)

    def test_function_length_check_definition_below_severity1v0(self):
        old_min_confidence = self.set_min_confidence(0)
        self.assert_function_length_check_below_error_level(1)
        self.set_min_confidence(old_min_confidence)

    def test_function_length_check_definition_at_severity1v0(self):
        old_min_confidence = self.set_min_confidence(0)
        self.assert_function_length_check_at_error_level(1)
        self.set_min_confidence(old_min_confidence)

    def test_function_length_check_definition_below_severity1(self):
        self.assert_function_length_check_definition_ok(self.trigger_lines(1) - 1)

    def test_function_length_check_definition_at_severity1(self):
        self.assert_function_length_check_definition_ok(self.trigger_lines(1))

    def test_function_length_check_definition_above_severity1(self):
        self.assert_function_length_check_above_error_level(1)

    def test_function_length_check_definition_severity1_plus_indented(self):
        error_level = 1
        error_lines = self.trigger_lines(error_level) + 1
        trigger_level = self.trigger_lines(self.min_confidence)
        indent_spaces = '  '
        self.assert_function_lengths_check(
            re.sub(r'(?m)^(.)', indent_spaces + r'\1',
                   'void test_indent(int x)\n' + self.function_body(error_lines)),
            ('Small and focused functions are preferred: '
             'test_indent() has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_plus_blanks(self):
        error_level = 1
        error_lines = self.trigger_lines(error_level) + 1
        trigger_level = self.trigger_lines(self.min_confidence)
        self.assert_function_lengths_check(
            'void test_blanks(int x)' + self.function_body(error_lines),
            ('Small and focused functions are preferred: '
             'test_blanks() has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_complex_definition_severity1(self):
        error_level = 1
        error_lines = self.trigger_lines(error_level) + 1
        trigger_level = self.trigger_lines(self.min_confidence)
        self.assert_function_lengths_check(
            ('my_namespace::my_other_namespace::MyVeryLongTypeName<Type1, bool func(const Element*)>*\n'
             'my_namespace::my_other_namespace<Type3, Type4>::~MyFunction<Type5<Type6, Type7> >(int arg1, char* arg2)'
             + self.function_body(error_lines)),
            ('Small and focused functions are preferred: '
             'my_namespace::my_other_namespace<Type3, Type4>::~MyFunction<Type5<Type6, Type7> >()'
             ' has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_for_test(self):
        error_level = 1
        error_lines = self.trigger_test_lines(error_level) + 1
        trigger_level = self.trigger_test_lines(self.min_confidence)
        self.assert_function_lengths_check(
            'TEST_F(Test, Mutator)' + self.function_body(error_lines),
            ('Small and focused functions are preferred: '
             'TEST_F(Test, Mutator) has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_for_split_line_test(self):
        error_level = 1
        error_lines = self.trigger_test_lines(error_level) + 1
        trigger_level = self.trigger_test_lines(self.min_confidence)
        self.assert_function_lengths_check(
            ('TEST_F(GoogleUpdateRecoveryRegistryProtectedTest,\n'
             '    FixGoogleUpdate_AllValues_MachineApp)'  # note: 4 spaces
             + self.function_body(error_lines)),
            ('Small and focused functions are preferred: '
             'TEST_F(GoogleUpdateRecoveryRegistryProtectedTest, '  # 1 space
             'FixGoogleUpdate_AllValues_MachineApp) has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_for_bad_test_doesnt_break(self):
        error_level = 1
        error_lines = self.trigger_test_lines(error_level) + 1
        # Since the function name isn't valid, the function detection algorithm
        # will skip it, so no error is produced.
        self.assert_function_lengths_check(
            ('TEST_F('
             + self.function_body(error_lines)),
            '')

    def test_function_length_check_definition_severity1_with_embedded_no_lints(self):
        error_level = 1
        error_lines = self.trigger_lines(error_level) + 1
        trigger_level = self.trigger_lines(self.min_confidence)
        self.assert_function_lengths_check(
            'void test(int x)' + self.function_body_with_no_lints(error_lines),
            ('Small and focused functions are preferred: '
             'test() has %d non-comment lines '
             '(error triggered by exceeding %d lines).'
             '  [readability/fn_size] [%d]')
            % (error_lines, trigger_level, error_level))

    def test_function_length_check_definition_severity1_with_no_lint(self):
        self.assert_function_lengths_check(
            ('void test(int x)' + self.function_body(self.trigger_lines(1))
             + '  // NOLINT -- long function'),
            '')

    def test_function_length_check_definition_below_severity2(self):
        self.assert_function_length_check_below_error_level(2)

    def test_function_length_check_definition_severity2(self):
        self.assert_function_length_check_at_error_level(2)

    def test_function_length_check_definition_above_severity2(self):
        self.assert_function_length_check_above_error_level(2)

    def test_function_length_check_definition_below_severity3(self):
        self.assert_function_length_check_below_error_level(3)

    def test_function_length_check_definition_severity3(self):
        self.assert_function_length_check_at_error_level(3)

    def test_function_length_check_definition_above_severity3(self):
        self.assert_function_length_check_above_error_level(3)

    def test_function_length_check_definition_below_severity4(self):
        self.assert_function_length_check_below_error_level(4)

    def test_function_length_check_definition_severity4(self):
        self.assert_function_length_check_at_error_level(4)

    def test_function_length_check_definition_above_severity4(self):
        self.assert_function_length_check_above_error_level(4)

    def test_function_length_check_definition_below_severity5(self):
        self.assert_function_length_check_below_error_level(5)

    def test_function_length_check_definition_at_severity5(self):
        self.assert_function_length_check_at_error_level(5)

    def test_function_length_check_definition_above_severity5(self):
        self.assert_function_length_check_above_error_level(5)

    def test_function_length_check_definition_huge_lines(self):
        # 5 is the limit
        self.assert_function_length_check_definition(self.trigger_lines(6), 5)

    def test_function_length_not_determinable(self):
        # Macro invocation without terminating semicolon.
        self.assert_function_lengths_check(
            'MACRO(arg)',
            '')

        # Macro with underscores
        self.assert_function_lengths_check(
            'MACRO_WITH_UNDERSCORES(arg1, arg2, arg3)',
            '')

        self.assert_function_lengths_check(
            'NonMacro(arg)',
            'Lint failed to find start of function body.'
            '  [readability/fn_size] [5]')


class NoNonVirtualDestructorsTest(CppStyleTestBase):

    def test_no_error(self):
        self.assert_multi_line_lint(
            '''\
                class Foo {
                    virtual ~Foo();
                    virtual void foo();
                };''',
            '')

        self.assert_multi_line_lint(
            '''\
                class Foo {
                    virtual inline ~Foo();
                    virtual void foo();
                };''',
            '')

        self.assert_multi_line_lint(
            '''\
                class Foo {
                    inline virtual ~Foo();
                    virtual void foo();
                };''',
            '')

        self.assert_multi_line_lint(
            '''\
                class Foo::Goo {
                    virtual ~Goo();
                    virtual void goo();
                };''',
            '')
        self.assert_multi_line_lint(
            'class MyClass {\n'
            '  int getIntValue() { DCHECK(m_ptr); return *m_ptr; }\n'
            '};\n',
            '')

        self.assert_multi_line_lint(
            '''\
                class Qualified::Goo : public Foo {
                    virtual void goo();
                };''',
            '')

    def test_no_destructor_when_virtual_needed(self):
        self.assert_multi_line_lint_re(
            '''\
                class Foo {
                    virtual void foo();
                };''',
            'The class Foo probably needs a virtual destructor')

    def test_destructor_non_virtual_when_virtual_needed(self):
        self.assert_multi_line_lint_re(
            '''\
                class Foo {
                    ~Foo();
                    virtual void foo();
                };''',
            'The class Foo probably needs a virtual destructor')

    def test_no_warn_when_derived(self):
        self.assert_multi_line_lint(
            '''\
                class Foo : public Goo {
                    virtual void foo();
                };''',
            '')

    def test_internal_braces(self):
        self.assert_multi_line_lint_re(
            '''\
                class Foo {
                    enum Goo {
                        Goo
                    };
                    virtual void foo();
                };''',
            'The class Foo probably needs a virtual destructor')

    def test_inner_class_needs_virtual_destructor(self):
        self.assert_multi_line_lint_re(
            '''\
                class Foo {
                    class Goo {
                        virtual void goo();
                    };
                };''',
            'The class Goo probably needs a virtual destructor')

    def test_outer_class_needs_virtual_destructor(self):
        self.assert_multi_line_lint_re(
            '''\
                class Foo {
                    class Goo {
                    };
                    virtual void foo();
                };''',
            'The class Foo probably needs a virtual destructor')

    def test_qualified_class_needs_virtual_destructor(self):
        self.assert_multi_line_lint_re(
            '''\
                class Qualified::Foo {
                    virtual void foo();
                };''',
            'The class Qualified::Foo probably needs a virtual destructor')

    def test_multi_line_declaration_no_error(self):
        self.assert_multi_line_lint_re(
            '''\
                class Foo
                    : public Goo {
                    virtual void foo();
                };''',
            '')

    def test_multi_line_declaration_with_error(self):
        self.assert_multi_line_lint(
            '''\
                class Foo
                {
                    virtual void foo();
                };''',
            'The class Foo probably needs a virtual destructor due to having '
            'virtual method(s), one declared at line 3.  [runtime/virtual] [4]')


class PassPtrTest(CppStyleTestBase):
    # For http://webkit.org/coding/RefPtr.html

    def assert_pass_ptr_check(self, code, expected_message):
        """Check warnings for Pass*Ptr are as expected.

        Args:
          code: C++ source code expected to generate a warning message.
          expected_message: Message expected to be generated by the C++ code.
        """
        self.assertEqual(expected_message,
                         self.perform_pass_ptr_check(code))

    def test_pass_ref_ptr_return_value(self):
        self.assert_pass_ptr_check(
            'scoped_refptr<Type1>\n'
            'myFunction(int)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'scoped_refptr<Type1> myFunction(int)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'scoped_refptr<Type1> myFunction();\n',
            '')
        self.assert_pass_ptr_check(
            'Ownscoped_refptr<Type1> myFunction();\n',
            '')

    def test_ref_ptr_parameter_value(self):
        self.assert_pass_ptr_check(
            'int myFunction(scoped_refptr<Type1>)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'int myFunction(scoped_refptr<Type1>&)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'int myFunction(scoped_refptr<Type1>*)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'int myFunction(scoped_refptr<Type1>* = 0)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'int myFunction(scoped_refptr<Type1>*    =  0)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'int myFunction(scoped_refptr<Type1>* = nullptr)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'int myFunction(scoped_refptr<Type1>*    =  nullptr)\n'
            '{\n'
            '}',
            '')

    def test_own_ptr_parameter_value(self):
        self.assert_pass_ptr_check(
            'int myFunction(PassOwnPtr<Type1>)\n'
            '{\n'
            '}',
            '')
        self.assert_pass_ptr_check(
            'int myFunction(OwnPtr<Type1>& simple)\n'
            '{\n'
            '}',
            '')

    def test_ref_ptr_member_variable(self):
        self.assert_pass_ptr_check(
            'class Foo {'
            '  scoped_refptr<Type1> m_other;\n'
            '};\n',
            '')


class WebKitStyleTest(CppStyleTestBase):

    # for https://www.chromium.org/blink/coding-style

    def test_line_breaking(self):
        # 2. An else statement should go on the same line as a preceding
        #   close brace if one is present, else it should line up with the
        #   if statement.
        self.assert_multi_line_lint(
            'if (condition) {\n'
            '  doSomething();\n'
            '  doSomethingAgain();\n'
            '} else {\n'
            '  doSomethingElse();\n'
            '  doSomethingElseAgain();\n'
            '}\n',
            '')
        self.assert_multi_line_lint(
            'if (condition)\n'
            '  doSomething();\n'
            'else\n'
            '  doSomethingElse();\n',
            '')
        self.assert_multi_line_lint(
            'if (condition) {\n'
            '  doSomething();\n'
            '} else {\n'
            '  doSomethingElse();\n'
            '  doSomethingElseAgain();\n'
            '}\n',
            '')
        self.assert_multi_line_lint(
            '#define TEST_ASSERT(expression) do { if (!(expression)) { '
            'TestsController::shared().testFailed(__FILE__, __LINE__, #expression); '
            'return; } } while (0)\n',
            '')
        # FIXME: currently we only check first conditional, so we cannot detect errors in next ones.
        self.assert_multi_line_lint(
            'WTF_MAKE_FAST_ALLOCATED;\n',
            '')
        self.assert_multi_line_lint(
            'if (condition) doSomething(); else {\n'
            '  doSomethingElse();\n'
            '}\n',
            'If one part of an if-else statement uses curly braces, the other part must too.  [whitespace/braces] [4]')
        self.assert_multi_line_lint(
            'void func()\n'
            '{\n'
            '  while (condition) { }\n'
            '  return 0;\n'
            '}\n',
            '')

    def test_braces(self):
        # 3. Curly braces are not required for single-line conditionals and
        #    loop bodies, but are required for single-statement bodies that
        #    span multiple lines.

        #
        # Positive tests
        #
        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  statement1();\n'
            'else\n'
            '  statement2();\n',
            '')

        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  statement1();\n'
            'else if (condition2)\n'
            '  statement2();\n',
            '')

        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  statement1();\n'
            'else if (condition2)\n'
            '  statement2();\n'
            'else\n'
            '  statement3();\n',
            '')

        self.assert_multi_line_lint(
            'for (; foo; bar)\n'
            '  int foo;\n',
            '')

        self.assert_multi_line_lint(
            'for (; foo; bar) {\n'
            '  int foo;\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'foreach (foo, foos) {\n'
            '  int bar;\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'foreach (foo, foos)\n'
            '  int bar;\n',
            '')

        self.assert_multi_line_lint(
            'while (true) {\n'
            '  int foo;\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'while (true)\n'
            '  int foo;\n',
            '')

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  statement1();\n'
            '} else {\n'
            '  statement2();\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  statement1();\n'
            '} else if (condition2) {\n'
            '  statement2();\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  statement1();\n'
            '} else if (condition2) {\n'
            '  statement2();\n'
            '} else {\n'
            '  statement3();\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  statement1();\n'
            '  statement1_2();\n'
            '} else if (condition2) {\n'
            '  statement2();\n'
            '  statement2_2();\n'
            '}\n',
            '')

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  statement1();\n'
            '  statement1_2();\n'
            '} else if (condition2) {\n'
            '  statement2();\n'
            '  statement2_2();\n'
            '} else {\n'
            '  statement3();\n'
            '  statement3_2();\n'
            '}\n',
            '')

        #
        # Negative tests
        #

        self.assert_multi_line_lint(
            'if (condition)\n'
            '  doSomething(\n'
            '      spanningMultipleLines);\n',
            'A conditional or loop body must use braces if the statement is more than one line long.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition)\n'
            '  // Single-line comment\n'
            '  doSomething();\n',
            'A conditional or loop body must use braces if the statement is more than one line long.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  statement1();\n'
            'else if (condition2)\n'
            '  // Single-line comment\n'
            '  statement2();\n',
            'A conditional or loop body must use braces if the statement is more than one line long.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  statement1();\n'
            'else if (condition2)\n'
            '  statement2();\n'
            'else\n'
            '  // Single-line comment\n'
            '  statement3();\n',
            'A conditional or loop body must use braces if the statement is more than one line long.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'for (; foo; bar)\n'
            '  // Single-line comment\n'
            '  int foo;\n',
            'A conditional or loop body must use braces if the statement is more than one line long.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'foreach (foo, foos)\n'
            '  // Single-line comment\n'
            '  int bar;\n',
            'A conditional or loop body must use braces if the statement is more than one line long.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'while (true)\n'
            '  // Single-line comment\n'
            '  int foo;\n'
            '\n',
            'A conditional or loop body must use braces if the statement is more than one line long.  [whitespace/braces] [4]')

        # 4. If one part of an if-else statement uses curly braces, the
        #    other part must too.

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  doSomething1();\n'
            '  doSomething1_2();\n'
            '} else if (condition2)\n'
            '  doSomething2();\n'
            'else\n'
            '  doSomething3();\n',
            'If one part of an if-else statement uses curly braces, the other part must too.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  doSomething1();\n'
            'else if (condition2) {\n'
            '  doSomething2();\n'
            '  doSomething2_2();\n'
            '} else\n'
            '  doSomething3();\n',
            'If one part of an if-else statement uses curly braces, the other part must too.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  doSomething1();\n'
            '} else if (condition2) {\n'
            '  doSomething2();\n'
            '  doSomething2_2();\n'
            '} else\n'
            '  doSomething3();\n',
            'If one part of an if-else statement uses curly braces, the other part must too.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  doSomething1();\n'
            'else if (condition2)\n'
            '  doSomething2();\n'
            'else {\n'
            '  doSomething3();\n'
            '  doSomething3_2();\n'
            '}\n',
            'If one part of an if-else statement uses curly braces, the other part must too.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition1) {\n'
            '  doSomething1();\n'
            '  doSomething1_2();\n'
            '} else if (condition2)\n'
            '  doSomething2();\n'
            'else {\n'
            '  doSomething3();\n'
            '  doSomething3_2();\n'
            '}\n',
            'If one part of an if-else statement uses curly braces, the other part must too.  [whitespace/braces] [4]')

        self.assert_multi_line_lint(
            'if (condition1)\n'
            '  doSomething1();\n'
            'else if (condition2) {\n'
            '  doSomething2();\n'
            '  doSomething2_2();\n'
            '} else {\n'
            '  doSomething3();\n'
            '  doSomething3_2();\n'
            '}\n',
            'If one part of an if-else statement uses curly braces, the other part must too.  [whitespace/braces] [4]')

    def test_null_false_zero(self):
        # Tests for true/false and null/non-null should be done without
        # equality comparisons.
        self.assert_lint(
            'if (string != NULL)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')
        self.assert_lint(
            'if (p == nullptr)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')
        self.assert_lint(
            'if (condition == true)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')
        self.assert_lint(
            'if (myVariable != /* Why would anyone put a comment here? */ false)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')

        self.assert_lint(
            'if (NULL == thisMayBeNull)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')
        self.assert_lint(
            'if (nullptr /* funny place for a comment */ == p)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')
        self.assert_lint(
            'if (true != anotherCondition)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')
        self.assert_lint(
            'if (false == myBoolValue)',
            'Tests for true/false and null/non-null should be done without equality comparisons.'
            '  [readability/comparison_to_boolean] [5]')

        self.assert_lint(
            'if (fontType == trueType)',
            '')
        self.assert_lint(
            'if (othertrue == fontType)',
            '')
        self.assert_lint(
            'if (LIKELY(foo == 0))',
            '')
        self.assert_lint(
            'if (UNLIKELY(foo == 0))',
            '')
        self.assert_lint(
            'if ((a - b) == 0.5)',
            '')
        self.assert_lint(
            'if (0.5 == (a - b))',
            '')

    def test_using_std_swap_ignored(self):
        self.assert_lint(
            'using std::swap;',
            '',
            'foo.cpp')

    def test_ctype_fucntion(self):
        self.assert_lint(
            'int i = isascii(8);',
            'Use equivalent function in <wtf/ASCIICType.h> instead of the '
            'isascii() function.  [runtime/ctype_function] [4]',
            'foo.cpp')

    def test_redundant_virtual(self):
        self.assert_lint('virtual void fooMethod() override;',
                         '"virtual" is redundant since function is already declared as "override"  [readability/inheritance] [4]')
        self.assert_lint('virtual void fooMethod(\n) override {}',
                         '"virtual" is redundant since function is already declared as "override"  [readability/inheritance] [4]')
        self.assert_lint('virtual void fooMethod() final;',
                         '"virtual" is redundant since function is already declared as "final"  [readability/inheritance] [4]')
        self.assert_lint('virtual void fooMethod(\n) final {}',
                         '"virtual" is redundant since function is already declared as "final"  [readability/inheritance] [4]')

    def test_redundant_override(self):
        self.assert_lint('void fooMethod() override final;',
                         '"override" is redundant since function is already declared as "final"  [readability/inheritance] [4]')
        self.assert_lint('void fooMethod(\n) override final {}',
                         '"override" is redundant since function is already declared as "final"  [readability/inheritance] [4]')
        self.assert_lint('void fooMethod() final override;',
                         '"override" is redundant since function is already declared as "final"  [readability/inheritance] [4]')
        self.assert_lint('void fooMethod(\n) final override {}',
                         '"override" is redundant since function is already declared as "final"  [readability/inheritance] [4]')


class CppCheckerTest(unittest.TestCase):

    """Tests CppChecker class."""

    def mock_handle_style_error(self):
        pass

    def _checker(self):
        return CppChecker('foo', 'h', self.mock_handle_style_error, 3)

    def test_init(self):
        """Test __init__ constructor."""
        checker = self._checker()
        self.assertEqual(checker.file_extension, 'h')
        self.assertEqual(checker.file_path, 'foo')
        self.assertEqual(checker.handle_style_error, self.mock_handle_style_error)
        self.assertEqual(checker.min_confidence, 3)

    def test_eq(self):
        """Test __eq__ equality function."""
        checker1 = self._checker()
        checker2 = self._checker()

        # == calls __eq__.
        self.assertTrue(checker1 == checker2)

        def mock_handle_style_error2(self):
            pass

        # Verify that a difference in any argument cause equality to fail.
        checker = CppChecker('foo', 'h', self.mock_handle_style_error, 3)
        self.assertFalse(checker == CppChecker('bar', 'h', self.mock_handle_style_error, 3))
        self.assertFalse(checker == CppChecker('foo', 'c', self.mock_handle_style_error, 3))
        self.assertFalse(checker == CppChecker('foo', 'h', mock_handle_style_error2, 3))
        self.assertFalse(checker == CppChecker('foo', 'h', self.mock_handle_style_error, 4))

    def test_ne(self):
        """Test __ne__ inequality function."""
        checker1 = self._checker()
        checker2 = self._checker()

        # != calls __ne__.
        # By default, __ne__ always returns true on different objects.
        # Thus, just check the distinguishing case to verify that the
        # code defines __ne__.
        self.assertFalse(checker1 != checker2)
