#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(PARENT_DIR)

import quote

verbose = False


# Wrapped versions of the functions that we're testing, so that during
# debugging we can more easily see what their inputs were.

def VerboseQuote(in_string, specials, *args, **kwargs):
  if verbose:
    sys.stdout.write('Invoking quote(%s, %s, %s)\n' %
                     (repr(in_string), repr(specials),
                      ', '.join([repr(a) for a in args] +
                                [repr(k) + ':' + repr(v)
                                 for k, v in kwargs])))
  return quote.quote(in_string, specials, *args, **kwargs)


def VerboseUnquote(in_string, specials, *args, **kwargs):
  if verbose:
    sys.stdout.write('Invoking unquote(%s, %s, %s)\n' %
                     (repr(in_string), repr(specials),
                      ', '.join([repr(a) for a in args] +
                                [repr(k) + ':' + repr(v)
                                 for k, v in kwargs])))
  return quote.unquote(in_string, specials, *args, **kwargs)


class TestQuote(unittest.TestCase):
  # test utilities
  def generic_test(self, fn, in_args, expected_out_obj):
    actual = apply(fn, in_args)
    self.assertEqual(actual, expected_out_obj)

  def check_invertible(self, in_string, specials, escape='\\'):
    q = VerboseQuote(in_string, specials, escape)
    qq = VerboseUnquote(q, specials, escape)
    self.assertEqual(''.join(qq), in_string)

  def run_test_tuples(self, test_tuples):
    for func, in_args, expected in test_tuples:
      self.generic_test(func, in_args, expected)

  def testQuote(self):
    test_tuples = [[VerboseQuote,
                    ['foo, bar, baz, and quux too!', 'abc'],
                    'foo, \\b\\ar, \\b\\az, \\and quux too!'],
                   [VerboseQuote,
                    ['when \\ appears in the input', 'a'],
                    'when \\\\ \\appe\\ars in the input']]
    self.run_test_tuples(test_tuples)

  def testUnquote(self):
    test_tuples = [[VerboseUnquote,
                    ['key\\:still_key:value\\:more_value', ':'],
                    ['key:still_key', ':', 'value:more_value']],
                   [VerboseUnquote,
                    ['about that sep\\ar\\ator in the beginning', 'ab'],
                    ['', 'ab', 'out th', 'a', 't separator in the ',
                     'b', 'eginning']],
                   [VerboseUnquote,
                    ['the rain in spain fall\\s ma\\i\\nly on the plains',
                     'ins'],
                    ['the ra', 'in', ' ', 'in', ' ', 's', 'pa', 'in',
                     ' falls mainly o', 'n', ' the pla', 'ins']],
                   ]
    self.run_test_tuples(test_tuples)

  def testInvertible(self):
    self.check_invertible('abcdefg', 'bc')
    self.check_invertible('a\\bcdefg', 'bc')
    self.check_invertible('ab\\cdefg', 'bc')
    self.check_invertible('\\ab\\cdefg', 'abc')
    self.check_invertible('abcde\\fg', 'efg')
    self.check_invertible('a\\b', '')


# Invoke this file directly for simple manual testing.  For running
# the unittests, use the -t flag.  Any flags to be passed to the
# unittest module should be passed as after the argparse processing,
# e.g., "quote_test.py -t -- -v" to pass the -v flag to the unittest
# module.

def main(args):
  global verbose
  parser = argparse.ArgumentParser()
  parser.add_argument('-s', '--special-chars',
                      dest='special_chars', default=':',
                      help='Special characters to quote (default is ":")')
  parser.add_argument('-q', '--quote', dest='quote', default='\\',
                      help='Quote or escape character (default is "\")')
  parser.add_argument('-u', '--unquote-input', dest='unquote_input',
                      action='store_true', help='Unquote command line argument')
  parser.add_argument('-v', '--verbose', dest='verbose', action='store_true',
                      help='Verbose test output')
  parser.add_argument('words', nargs='*')
  options = parser.parse_args(args)

  if options.verbose:
    verbose = True

  if not options.words:
    unittest.main()

  num_errors = 0
  for word in options.words:
    # NB: there are inputs x for which quote(unquote(x) != x, but
    # there should be no input x for which unquote(quote(x)) != x.
    if options.unquote_input:
      qq = quote.unquote(word, options.special_chars, options.quote)
      sys.stdout.write('unquote(%s) = %s\n'
                       % (word, ''.join(qq)))
      # There is no expected output for unquote -- this is just for
      # manual testing, so it is okay that we do not (and cannot)
      # update num_errors here.
    else:
      q = quote.quote(word, options.special_chars, options.quote)
      qq = quote.unquote(q, options.special_chars, options.quote)
      sys.stdout.write('quote(%s) = %s, unquote(%s) = %s\n'
                       % (word, q, q, ''.join(qq)))
      if word != ''.join(qq):
        num_errors += 1
  if num_errors > 0:
    sys.stderr.write('[  FAILED  ] %d test failures\n' % num_errors)
  return num_errors

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
