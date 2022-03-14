#!/usr/bin/env python3
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a dictionary for libFuzzer or AFL-based fuzzer.

Invoked manually using a fuzzer binary and target format/protocol specification.
Works better for text formats or protocols. For binary ones may be useless.
"""

import argparse
import HTMLParser
import io
import logging
import os
import re
import shutil
import string
import subprocess
import sys
import tempfile


ENCODING_TYPES = ['ascii', 'utf_16_be', 'utf_16_le', 'utf_32_be', 'utf_32_le']
MIN_STRING_LENGTH = 4


def DecodeHTML(html_data):
  """HTML-decoding of the data."""
  html_parser = HTMLParser.HTMLParser()
  data = html_parser.unescape(html_data.decode('ascii', 'ignore'))
  return data.encode('ascii', 'ignore')


def EscapeDictionaryElement(element):
  """Escape all unprintable and control characters in an element."""
  element_escaped = element.encode('string_escape')
  # Remove escaping for single quote because it breaks libFuzzer.
  element_escaped = element_escaped.replace('\\\'', '\'')
  # Add escaping for double quote.
  element_escaped = element_escaped.replace('"', '\\"')
  return element_escaped


def ExtractWordsFromBinary(filepath, min_length=MIN_STRING_LENGTH):
  """Extract words (splitted strings) from a binary executable file."""
  rodata = PreprocessAndReadRodata(filepath)
  words = []

  strings_re = re.compile(r'[^\x00-\x1F\x7F-\xFF]{%d,}' % min_length)
  # Use different encodings for strings extraction.
  for encoding in ENCODING_TYPES:
    data = rodata.decode(encoding, 'ignore').encode('ascii', 'ignore')
    raw_strings = strings_re.findall(data)
    for splitted_line in map(lambda line: line.split(), raw_strings):
      words += splitted_line

  return set(words)


def ExtractWordsFromLines(lines):
  """Extract all words from a list of strings."""
  words = set()
  for line in lines:
    for word in line.split():
      words.add(word)

  return words


def ExtractWordsFromSpec(filepath, is_html):
  """Extract words from a specification."""
  data = ReadSpecification(filepath, is_html)
  words = data.split()
  return set(words)


def FindIndentedText(text):
  """Find space-indented text blocks, e.g. code or data samples in RFCs."""
  lines = text.split('\n')
  indented_blocks = []
  current_block = ''
  previous_number_of_spaces = 0

  # Go through every line and concatenate space-indented blocks into lines.
  for i in xrange(0, len(lines), 1):
    if not lines[i]:
      # Ignore empty lines.
      continue

    # Space-indented text blocks have more leading spaces than regular text.
    n = FindNumberOfLeadingSpaces(lines[i])

    if n > previous_number_of_spaces:
      # Beginning of a space-indented text block, start concatenation.
      current_block = lines[i][n : ]
    elif n == previous_number_of_spaces and current_block:
      # Or continuation of a space-indented text block, concatenate lines.
      current_block += '\n' + lines[i][n : ]

    if n < previous_number_of_spaces and current_block:
      # Current line is not indented, save previously concatenated lines.
      indented_blocks.append(current_block)
      current_block = ''

    previous_number_of_spaces = n

  return indented_blocks


def FindNumberOfLeadingSpaces(line):
  """Calculate number of leading whitespace characters in the string."""
  n = 0
  while n < len(line) and line[n].isspace():
    n += 1

  return n


def GenerateDictionary(path_to_binary, path_to_spec, strategy, is_html=False):
  """Generate a dictionary for given pair of fuzzer binary and specification."""
  for filepath in [path_to_binary, path_to_spec]:
    if not os.path.exists(filepath):
      logging.error('%s doesn\'t exist. Exit.', filepath)
      sys.exit(1)

  words_from_binary = ExtractWordsFromBinary(path_to_binary)
  words_from_spec = ExtractWordsFromSpec(path_to_spec, is_html)

  dictionary_words = set()

  if 'i' in strategy:
    # Strategy i: only words which are common for binary and for specification.
    dictionary_words = words_from_binary.intersection(words_from_spec)

  if 'q' in strategy:
    # Strategy q: add words from all quoted strings from specification.
    # TODO(mmoroz): experimental and very noisy. Not recommended to use.
    spec_data = ReadSpecification(path_to_spec, is_html)
    quoted_strings = FindIndentedText(spec_data)
    quoted_words = ExtractWordsFromLines(quoted_strings)
    dictionary_words = dictionary_words.union(quoted_words)

  if 'u' in strategy:
    # Strategy u: add all uppercase words from specification.
    uppercase_words = set(w for w in words_from_spec if w.isupper())
    dictionary_words = dictionary_words.union(uppercase_words)

  return dictionary_words


def PreprocessAndReadRodata(filepath):
  """Create a stripped copy of the binary and extract .rodata section."""
  stripped_file = tempfile.NamedTemporaryFile(prefix='.stripped_')
  stripped_filepath = stripped_file.name
  shutil.copyfile(filepath, stripped_filepath)

  # Strip all symbols to reduce amount of redundant strings.
  strip_cmd = ['strip', '--strip-all', stripped_filepath]
  result = subprocess.call(strip_cmd)
  if result:
    logging.warning('Failed to strip the binary. Using the original version.')
    stripped_filepath = filepath

  # Extract .rodata section to reduce amount of redundant strings.
  rodata_file = tempfile.NamedTemporaryFile(prefix='.rodata_')
  rodata_filepath = rodata_file.name
  objcopy_cmd = ['objcopy', '-j', '.rodata', stripped_filepath, rodata_filepath]

  # Hide output from stderr since objcopy prints a warning.
  with open(os.devnull, 'w') as devnull:
    result = subprocess.call(objcopy_cmd, stderr=devnull)

  if result:
    logging.warning('Failed to extract .rodata section. Using the whole file.')
    rodata_filepath = stripped_filepath

  with open(rodata_filepath) as file_handle:
    data = file_handle.read()

  stripped_file.close()
  rodata_file.close()

  return data


def ReadSpecification(filepath, is_html):
  """Read a specification file and return its contents."""
  with open(filepath, 'r') as file_handle:
    data = file_handle.read()

  if is_html:
    data = DecodeHTML(data)

  return data


def WriteDictionary(dictionary_path, dictionary):
  """Write given dictionary to a file."""
  with open(dictionary_path, 'wb') as file_handle:
    file_handle.write('# This is an automatically generated dictionary.\n')
    for word in dictionary:
      if not word:
        continue
      line = '"%s"\n' % EscapeDictionaryElement(word)
      file_handle.write(line)


def main():
  parser = argparse.ArgumentParser(description="Generate fuzzer dictionary.")
  parser.add_argument('--fuzzer', required=True,
                      help='Path to a fuzzer binary executable. It is '
                      'recommended to use a binary built with '
                      '"use_libfuzzer=false is_asan=false" to get a better '
                      'dictionary with fewer number of redundant elements.')
  parser.add_argument('--spec', required=True,
                      help='Path to a target specification (in textual form).')
  parser.add_argument('--html', default=0,
                      help='Decode HTML [01] (0 is default value): '
                      '1 - if specification has HTML entities to be decoded.')
  parser.add_argument('--out', required=True,
                      help='Path to a file to write a dictionary into.')
  parser.add_argument('--strategy', default='iu',
                      help='Generation strategy [iqu] ("iu" is default value): '
                      'i - intersection, q - quoted, u - uppercase.')
  args = parser.parse_args()

  dictionary = GenerateDictionary(args.fuzzer, args.spec, args.strategy,
                                  is_html=bool(args.html))
  WriteDictionary(args.out, dictionary)


if __name__ == '__main__':
  main()
