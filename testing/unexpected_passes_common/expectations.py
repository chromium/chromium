# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to test expectations/expectation files."""

from __future__ import print_function

import logging
import os
import sys

from typ import expectations_parser
from unexpected_passes_common import data_types
from unexpected_passes_common import result_output


FINDER_DISABLE_COMMENT = 'finder:disable'
FINDER_ENABLE_COMMENT = 'finder:enable'


class Expectations(object):
  def CreateTestExpectationMap(self, expectation_files, tests):
    """Creates an expectation map based off a file or list of tests.

    Args:
      expectation_files: A filepath or list of filepaths to expectation files to
          read from, or None. If a filepath is specified, |tests| must be None.
      tests: An iterable of strings containing test names to check. If
          specified, |expectation_file| must be None.

    Returns:
      A data_types.TestExpectationMap, although all its BuilderStepMap contents
      will be empty.
    """

    def AddContentToMap(content, ex_map, expectation_file_name):
      list_parser = expectations_parser.TaggedTestListParser(content)
      expectations_for_file = ex_map.setdefault(
          expectation_file_name, data_types.ExpectationBuilderMap())
      logging.debug('Parsed %d expectations', len(list_parser.expectations))
      for e in list_parser.expectations:
        if 'Skip' in e.raw_results:
          continue
        expectation = data_types.Expectation(e.test, e.tags, e.raw_results,
                                             e.reason)
        assert expectation not in expectations_for_file
        expectations_for_file[expectation] = data_types.BuilderStepMap()

    logging.info('Creating test expectation map')
    assert expectation_files or tests
    assert not (expectation_files and tests)

    expectation_map = data_types.TestExpectationMap()

    if expectation_files:
      if not isinstance(expectation_files, list):
        expectation_files = [expectation_files]
      for ef in expectation_files:
        expectation_file_name = os.path.normpath(ef)
        with open(ef) as f:
          content = f.read()
        AddContentToMap(content, expectation_map, expectation_file_name)
    else:
      expectation_file_name = ''
      content = '# results: [ RetryOnFailure ]\n'
      for t in tests:
        content += '%s [ RetryOnFailure ]\n' % t
      AddContentToMap(content, expectation_map, expectation_file_name)

    return expectation_map

  def RemoveExpectationsFromFile(self, expectations, expectation_file):
    """Removes lines corresponding to |expectations| from |expectation_file|.

    Ignores any lines that match but are within a disable block or have an
    inline disable comment.

    Args:
      expectations: A list of data_types.Expectations to remove.
      expectation_file: A filepath pointing to an expectation file to remove
          lines from.

    Returns:
      A set of strings containing URLs of bugs associated with the removed
      expectations.
    """

    with open(expectation_file) as f:
      input_contents = f.read()

    output_contents = ''
    in_disable_block = False
    disable_block_reason = ''
    removed_urls = set()
    for line in input_contents.splitlines(True):
      # Auto-add any comments or empty lines
      stripped_line = line.strip()
      if _IsCommentOrBlankLine(stripped_line):
        output_contents += line
        assert not (FINDER_DISABLE_COMMENT in line
                    and FINDER_ENABLE_COMMENT in line)
        # Handle disable/enable block comments.
        if FINDER_DISABLE_COMMENT in line:
          if in_disable_block:
            raise RuntimeError(
                'Invalid expectation file %s - contains a disable comment "%s" '
                'that is in another disable block.' %
                (expectation_file, stripped_line))
          in_disable_block = True
          disable_block_reason = _GetDisableReasonFromComment(line)
        if FINDER_ENABLE_COMMENT in line:
          if not in_disable_block:
            raise RuntimeError(
                'Invalid expectation file %s - contains an enable comment "%s" '
                'that is outside of a disable block.' %
                (expectation_file, stripped_line))
          in_disable_block = False
        continue

      current_expectation = self._CreateExpectationFromExpectationFileLine(line)

      # Add any lines containing expectations that don't match any of the given
      # expectations to remove.
      if any([e for e in expectations if e == current_expectation]):
        # Skip any expectations that match if we're in a disable block or there
        # is an inline disable comment.
        if in_disable_block:
          output_contents += line
          logging.info(
              'Would have removed expectation %s, but inside a disable block '
              'with reason %s', stripped_line, disable_block_reason)
        elif FINDER_DISABLE_COMMENT in line:
          output_contents += line
          logging.info(
              'Would have removed expectation %s, but it has an inline disable '
              'comment with reason %s',
              stripped_line.split('#')[0], _GetDisableReasonFromComment(line))
        else:
          bug = current_expectation.bug
          if bug:
            removed_urls.add(bug)
      else:
        output_contents += line

    with open(expectation_file, 'w') as f:
      f.write(output_contents)

    return removed_urls

  def _CreateExpectationFromExpectationFileLine(self, line):
    """Creates a data_types.Expectation from |line|.

    Args:
      line: A string containing a single line from an expectation file.

    Returns:
      A data_types.Expectation containing the same information as |line|.
    """
    header = self._GetExpectationFileTagHeader()
    single_line_content = header + line
    list_parser = expectations_parser.TaggedTestListParser(single_line_content)
    assert len(list_parser.expectations) == 1
    typ_expectation = list_parser.expectations[0]
    return data_types.Expectation(typ_expectation.test, typ_expectation.tags,
                                  typ_expectation.raw_results,
                                  typ_expectation.reason)

  def _GetExpectationFileTagHeader(self):
    """Gets the tag header used for expectation files.

    Returns:
      A string containing an expectation file header, i.e. the comment block at
      the top of the file defining possible tags and expected results.
    """
    raise NotImplementedError()

  def ModifySemiStaleExpectations(self, stale_expectation_map):
    """Modifies lines from |stale_expectation_map| in |expectation_file|.

    Prompts the user for each modification and provides debug information since
    semi-stale expectations cannot be blindly removed like fully stale ones.

    Args:
      stale_expectation_map: A data_types.TestExpectationMap containing stale
          expectations.
      file_handle: An optional open file-like object to output to. If not
          specified, stdout will be used.

    Returns:
      A set of strings containing URLs of bugs associated with the modified
      (manually modified by the user or removed by the script) expectations.
    """
    expectations_to_remove = []
    expectations_to_modify = []
    modified_urls = set()
    for expectation_file, e, builder_map in (
        stale_expectation_map.IterBuilderStepMaps()):
      with open(expectation_file) as infile:
        file_contents = infile.read()
      line, line_number = self._GetExpectationLine(e, file_contents)
      expectation_str = None
      if not line:
        logging.error(
            'Could not find line corresponding to semi-stale expectation for '
            '%s with tags %s and expected results %s', e.test, e.tags,
            e.expected_results)
        expectation_str = '[ %s ] %s [ %s ]' % (' '.join(
            e.tags), e.test, ' '.join(e.expected_results))
      else:
        expectation_str = '%s (approx. line %d)' % (line, line_number)

      str_dict = result_output.ConvertBuilderMapToPassOrderedStringDict(
          builder_map)
      print('\nSemi-stale expectation:\n%s' % expectation_str)
      result_output.RecursivePrintToFile(str_dict, 1, sys.stdout)

      response = _WaitForUserInputOnModification()
      if response == 'r':
        expectations_to_remove.append(e)
      elif response == 'm':
        expectations_to_modify.append(e)

      # It's possible that the user will introduce a typo while manually
      # modifying an expectation, which will cause a parser error. Catch that
      # now and give them chances to fix it so that they don't lose all of their
      # work due to an early exit.
      while True:
        try:
          with open(expectation_file) as infile:
            file_contents = infile.read()
          _ = expectations_parser.TaggedTestListParser(file_contents)
          break
        except expectations_parser.ParseError as error:
          logging.error('Got parser error: %s', error)
          logging.error(
              'This probably means you introduced a typo, please fix it.')
          _WaitForAnyUserInput()

      modified_urls |= self.RemoveExpectationsFromFile(expectations_to_remove,
                                                       expectation_file)
    for e in expectations_to_modify:
      modified_urls.add(e.bug)
    return modified_urls

  def _GetExpectationLine(self, expectation, file_contents):
    """Gets the line and line number of |expectation| in |file_contents|.

    Args:
      expectation: A data_types.Expectation.
      file_contents: A string containing the contents read from an expectation
          file.

    Returns:
      A tuple (line, line_number). |line| is a string containing the exact line
      in |file_contents| corresponding to |expectation|. |line_number| is an int
      corresponding to where |line| is in |file_contents|. |line_number| may be
      off if the file on disk has changed since |file_contents| was read. If a
      corresponding line cannot be found, both |line| and |line_number| are
      None.
    """
    # We have all the information necessary to recreate the expectation line and
    # line number can be pulled during the initial expectation parsing. However,
    # the information we have is not necessarily in the same order as the
    # text file (e.g. tag ordering), and line numbers can change pretty
    # dramatically between the initial parse and now due to stale expectations
    # being removed. So, parse this way in order to improve the user experience.
    file_lines = file_contents.splitlines()
    for line_number, line in enumerate(file_lines):
      if _IsCommentOrBlankLine(line.strip()):
        continue
      current_expectation = self._CreateExpectationFromExpectationFileLine(line)
      if expectation == current_expectation:
        return line, line_number + 1
    return None, None

  def FindOrphanedBugs(self, affected_urls):
    """Finds cases where expectations for bugs no longer exist.

    Args:
      affected_urls: An iterable of affected bug URLs, as returned by functions
          such as RemoveExpectationsFromFile.

    Returns:
      A set containing a subset of |affected_urls| who no longer have any
      associated expectations in any expectation files.
    """
    seen_bugs = set()

    expectation_files = self.GetExpectationFilepaths()

    for ef in expectation_files:
      with open(ef) as infile:
        contents = infile.read()
      for url in affected_urls:
        if url in seen_bugs:
          continue
        if url in contents:
          seen_bugs.add(url)
    return set(affected_urls) - seen_bugs

  def GetExpectationFilepaths(self):
    """Gets all the filepaths to expectation files of interest.

    Returns:
      A list of strings, each element being a filepath pointing towards an
      expectation file.
    """
    raise NotImplementedError()


def _WaitForAnyUserInput():
  """Waits for any user input.

  Split out for testing purposes.
  """
  _get_input('Press any key to continue')


def _WaitForUserInputOnModification():
  """Waits for user input on how to modify a semi-stale expectation.

  Returns:
    One of the following string values:
      i - Expectation should be ignored and left alone.
      m - Expectation will be manually modified by the user.
      r - Expectation should be removed by the script.
  """
  valid_inputs = ['i', 'm', 'r']
  prompt = ('How should this expectation be handled? (i)gnore/(m)anually '
            'modify/(r)emove: ')
  response = _get_input(prompt).lower()
  while response not in valid_inputs:
    print('Invalid input, valid inputs are %s' % (', '.join(valid_inputs)))
    response = _get_input(prompt).lower()
  return response


def _GetDisableReasonFromComment(line):
  return line.split(FINDER_DISABLE_COMMENT, 1)[1].strip()


def _IsCommentOrBlankLine(line):
  return (not line or line.startswith('#'))


def _get_input(prompt):
  if sys.version_info[0] == 2:
    return raw_input(prompt)
  return input(prompt)
