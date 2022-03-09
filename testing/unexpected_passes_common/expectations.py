# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to test expectations/expectation files."""

from __future__ import print_function

import collections
import datetime
import logging
import os
import re
import subprocess
import sys

import six

from typ import expectations_parser
from unexpected_passes_common import data_types
from unexpected_passes_common import result_output

FINDER_DISABLE_COMMENT_BASE = 'finder:disable'
FINDER_ENABLE_COMMENT_BASE = 'finder:enable'
FINDER_COMMENT_SUFFIX_GENERAL = '-general'
FINDER_COMMENT_SUFFIX_STALE = '-stale'
FINDER_COMMENT_SUFFIX_UNUSED = '-unused'

ALL_FINDER_SUFFIXES = frozenset([
    FINDER_COMMENT_SUFFIX_GENERAL,
    FINDER_COMMENT_SUFFIX_STALE,
    FINDER_COMMENT_SUFFIX_UNUSED,
])

FINDER_DISABLE_COMMENT_GENERAL = (FINDER_DISABLE_COMMENT_BASE +
                                  FINDER_COMMENT_SUFFIX_GENERAL)
FINDER_DISABLE_COMMENT_STALE = (FINDER_DISABLE_COMMENT_BASE +
                                FINDER_COMMENT_SUFFIX_STALE)
FINDER_DISABLE_COMMENT_UNUSED = (FINDER_DISABLE_COMMENT_BASE +
                                 FINDER_COMMENT_SUFFIX_UNUSED)
FINDER_ENABLE_COMMENT_GENERAL = (FINDER_ENABLE_COMMENT_BASE +
                                 FINDER_COMMENT_SUFFIX_GENERAL)
FINDER_ENABLE_COMMENT_STALE = (FINDER_ENABLE_COMMENT_BASE +
                               FINDER_COMMENT_SUFFIX_STALE)
FINDER_ENABLE_COMMENT_UNUSED = (FINDER_ENABLE_COMMENT_BASE +
                                FINDER_COMMENT_SUFFIX_UNUSED)

FINDER_DISABLE_COMMENTS = frozenset([
    FINDER_DISABLE_COMMENT_GENERAL, FINDER_DISABLE_COMMENT_STALE,
    FINDER_DISABLE_COMMENT_UNUSED
])

FINDER_ENABLE_COMMENTS = frozenset([
    FINDER_ENABLE_COMMENT_GENERAL,
    FINDER_ENABLE_COMMENT_STALE,
    FINDER_ENABLE_COMMENT_UNUSED,
])

ALL_FINDER_COMMENTS = frozenset(FINDER_DISABLE_COMMENTS
                                | FINDER_ENABLE_COMMENTS)

GIT_BLAME_REGEX = re.compile(
    r'^[\w\s]+\(.+(?P<date>\d\d\d\d-\d\d-\d\d)[^\)]+\)(?P<content>.*)$',
    re.DOTALL)
EXPECTATION_LINE_REGEX = re.compile(r'^.*\[ .* \] .* \[ \w* \].*$', re.DOTALL)

# pylint: disable=useless-object-inheritance


class RemovalType(object):
  STALE = FINDER_COMMENT_SUFFIX_STALE
  UNUSED = FINDER_COMMENT_SUFFIX_UNUSED


class Expectations(object):
  def CreateTestExpectationMap(self, expectation_files, tests, grace_period):
    """Creates an expectation map based off a file or list of tests.

    Args:
      expectation_files: A filepath or list of filepaths to expectation files to
          read from, or None. If a filepath is specified, |tests| must be None.
      tests: An iterable of strings containing test names to check. If
          specified, |expectation_file| must be None.
      grace_period: An int specifying how many days old an expectation must
          be in order to be parsed, i.e. how many days old an expectation must
          be before it is a candidate for removal/modification.

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
        # Expectations that only have a Pass expectation (usually used to
        # override a broader, failing expectation) are not handled by the
        # unexpected pass finder, so ignore those.
        if e.raw_results == ['Pass']:
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
        content = self._GetNonRecentExpectationContent(expectation_file_name,
                                                       grace_period)
        AddContentToMap(content, expectation_map, expectation_file_name)
    else:
      expectation_file_name = ''
      content = '# results: [ RetryOnFailure ]\n'
      for t in tests:
        content += '%s [ RetryOnFailure ]\n' % t
      AddContentToMap(content, expectation_map, expectation_file_name)

    return expectation_map

  def _GetNonRecentExpectationContent(self, expectation_file_path, num_days):
    """Gets content from |expectation_file_path| older than |num_days| days.

    Args:
      expectation_file_path: A string containing a filepath pointing to an
          expectation file.
      num_days: An int containing how old an expectation in the given
          expectation file must be to be included.

    Returns:
      The contents of the expectation file located at |expectation_file_path|
      as a string with any recent expectations removed.
    """
    num_days = datetime.timedelta(days=num_days)
    content = ''
    # `git blame` output is normally in the format:
    # revision optional_filename (author date time timezone lineno) line_content
    # The --porcelain option is meant to be more machine readable, but is much
    # more difficult to parse for what we need to do here. In order to
    # guarantee that the filename won't be included in the output (by default,
    # it will be shown if there is content from a renamed file), pass -c to
    # use the same format as `git annotate`, which is:
    # revision (author date time timezone lineno)line_content
    # (Note the lack of space between the ) and the content).
    cmd = ['git', 'blame', '-c', expectation_file_path]
    with open(os.devnull, 'w') as devnull:
      blame_output = subprocess.check_output(cmd,
                                             stderr=devnull).decode('utf-8')
    for line in blame_output.splitlines(True):
      match = GIT_BLAME_REGEX.match(line)
      assert match
      date = match.groupdict()['date']
      line_content = match.groupdict()['content']
      if EXPECTATION_LINE_REGEX.match(line):
        if six.PY2:
          date_parts = date.split('-')
          date = datetime.date(year=int(date_parts[0]),
                               month=int(date_parts[1]),
                               day=int(date_parts[2]))
        else:
          date = datetime.date.fromisoformat(date)
        date_diff = datetime.date.today() - date
        if date_diff > num_days:
          content += line_content
        else:
          logging.debug('Omitting expectation %s because it is too new',
                        line_content.rstrip())
      else:
        content += line_content
    return content

  def RemoveExpectationsFromFile(self, expectations, expectation_file,
                                 removal_type):
    """Removes lines corresponding to |expectations| from |expectation_file|.

    Ignores any lines that match but are within a disable block or have an
    inline disable comment.

    Args:
      expectations: A list of data_types.Expectations to remove.
      expectation_file: A filepath pointing to an expectation file to remove
          lines from.
      removal_type: A RemovalType enum corresponding to the type of expectations
          being removed.

    Returns:
      A set of strings containing URLs of bugs associated with the removed
      expectations.
    """

    with open(expectation_file) as f:
      input_contents = f.read()

    output_contents = ''
    in_disable_block = False
    disable_block_reason = ''
    disable_block_suffix = ''
    removed_urls = set()
    for line in input_contents.splitlines(True):
      # Auto-add any comments or empty lines
      stripped_line = line.strip()
      if _IsCommentOrBlankLine(stripped_line):
        output_contents += line
        # Only allow one enable/disable per line.
        assert len([c for c in ALL_FINDER_COMMENTS if c in line]) <= 1
        # Handle disable/enable block comments.
        if _LineContainsDisableComment(line):
          if in_disable_block:
            raise RuntimeError(
                'Invalid expectation file %s - contains a disable comment "%s" '
                'that is in another disable block.' %
                (expectation_file, stripped_line))
          in_disable_block = True
          disable_block_reason = _GetDisableReasonFromComment(line)
          disable_block_suffix = _GetFinderCommentSuffix(line)
        if _LineContainsEnableComment(line):
          if not in_disable_block:
            raise RuntimeError(
                'Invalid expectation file %s - contains an enable comment "%s" '
                'that is outside of a disable block.' %
                (expectation_file, stripped_line))
          in_disable_block = False
        continue

      current_expectation = self._CreateExpectationFromExpectationFileLine(
          line, expectation_file)

      # Add any lines containing expectations that don't match any of the given
      # expectations to remove.
      if any(e for e in expectations if e == current_expectation):
        # Skip any expectations that match if we're in a disable block or there
        # is an inline disable comment.
        if in_disable_block and _DisableSuffixIsRelevant(
            disable_block_suffix, removal_type):
          output_contents += line
          logging.info(
              'Would have removed expectation %s, but inside a disable block '
              'with reason %s', stripped_line, disable_block_reason)
        elif _LineContainsRelevantDisableComment(line, removal_type):
          output_contents += line
          logging.info(
              'Would have removed expectation %s, but it has an inline disable '
              'comment with reason %s',
              stripped_line.split('#')[0], _GetDisableReasonFromComment(line))
        else:
          bug = current_expectation.bug
          if bug:
            # It's possible to have multiple whitespace-separated bugs per
            # expectation, so treat each one separately.
            removed_urls |= set(bug.split())
      else:
        output_contents += line

    with open(expectation_file, 'w') as f:
      f.write(output_contents)

    return removed_urls

  def _CreateExpectationFromExpectationFileLine(self, line, expectation_file):
    """Creates a data_types.Expectation from |line|.

    Args:
      line: A string containing a single line from an expectation file.
      expectation_file: A filepath pointing to an expectation file |line| came
          from.

    Returns:
      A data_types.Expectation containing the same information as |line|.
    """
    header = self._GetExpectationFileTagHeader(expectation_file)
    single_line_content = header + line
    list_parser = expectations_parser.TaggedTestListParser(single_line_content)
    assert len(list_parser.expectations) == 1
    typ_expectation = list_parser.expectations[0]
    return data_types.Expectation(typ_expectation.test, typ_expectation.tags,
                                  typ_expectation.raw_results,
                                  typ_expectation.reason)

  def _GetExpectationFileTagHeader(self, expectation_file):
    """Gets the tag header used for expectation files.

    Args:
      expectation_file: A filepath pointing to an expectation file to get the
          tag header from.

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
      line, line_number = self._GetExpectationLine(e, file_contents,
                                                   expectation_file)
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
                                                       expectation_file,
                                                       RemovalType.STALE)
    for e in expectations_to_modify:
      modified_urls |= set(e.bug.split())
    return modified_urls

  def _GetExpectationLine(self, expectation, file_contents, expectation_file):
    """Gets the line and line number of |expectation| in |file_contents|.

    Args:
      expectation: A data_types.Expectation.
      file_contents: A string containing the contents read from an expectation
          file.
      expectation_file: A string containing the path to the expectation file
          that |file_contents| came from.

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
      current_expectation = self._CreateExpectationFromExpectationFileLine(
          line, expectation_file)
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


def _LineContainsDisableComment(line):
  return FINDER_DISABLE_COMMENT_BASE in line


def _LineContainsEnableComment(line):
  return FINDER_ENABLE_COMMENT_BASE in line


def _GetFinderCommentSuffix(line):
  """Gets the suffix of the finder comment on the given line.

  Examples:
    'foo  # finder:disable' -> ''
    'foo  # finder:disable-stale some_reason' -> '-stale'
  """
  target_str = None
  if _LineContainsDisableComment(line):
    target_str = FINDER_DISABLE_COMMENT_BASE
  elif _LineContainsEnableComment(line):
    target_str = FINDER_ENABLE_COMMENT_BASE
  else:
    raise RuntimeError('Given line %s did not have a finder comment.' % line)
  line = line[line.find(target_str):]
  line = line.split()[0]
  suffix = line.replace(target_str, '')
  assert suffix in ALL_FINDER_SUFFIXES
  return suffix


def _LineContainsRelevantDisableComment(line, removal_type):
  """Returns whether the given line contains a relevant disable comment.

  Args:
    line: A string containing the line to check.
    removal_type: A RemovalType enum corresponding to the type of expectations
        being removed.

  Returns:
    A bool denoting whether |line| contains a relevant disable comment given
    |removal_type|.
  """
  if FINDER_DISABLE_COMMENT_GENERAL in line:
    return True
  if FINDER_DISABLE_COMMENT_BASE + removal_type in line:
    return True
  return False


def _DisableSuffixIsRelevant(suffix, removal_type):
  """Returns whether the given suffix is relevant given the removal type.

  Args:
    suffix: A string containing a disable comment suffix.
    removal_type: A RemovalType enum corresponding to the type of expectations
        being removed.

  Returns:
    True if suffix is relevant and its disable request should be honored.
  """
  if suffix == FINDER_COMMENT_SUFFIX_GENERAL:
    return True
  if suffix == removal_type:
    return True
  return False


def _GetDisableReasonFromComment(line):
  suffix = _GetFinderCommentSuffix(line)
  return line.split(FINDER_DISABLE_COMMENT_BASE + suffix, 1)[1].strip()


def _IsCommentOrBlankLine(line):
  return (not line or line.startswith('#'))


def _get_input(prompt):
  if sys.version_info[0] == 2:
    return raw_input(prompt)
  return input(prompt)
