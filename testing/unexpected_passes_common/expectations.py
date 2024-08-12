# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to test expectations/expectation files."""

import collections
import copy
import datetime
import logging
import os
import re
import subprocess
from typing import Dict, FrozenSet, Iterable, List, Optional, Set, Tuple, Union

import six

from typ import expectations_parser
from unexpected_passes_common import data_types

FINDER_DISABLE_COMMENT_BASE = 'finder:disable'
FINDER_ENABLE_COMMENT_BASE = 'finder:enable'
FINDER_COMMENT_SUFFIX_GENERAL = '-general'
FINDER_COMMENT_SUFFIX_STALE = '-stale'
FINDER_COMMENT_SUFFIX_UNUSED = '-unused'
FINDER_COMMENT_SUFFIX_NARROWING = '-narrowing'

FINDER_GROUP_COMMENT_START = 'finder:group-start'
FINDER_GROUP_COMMENT_END = 'finder:group-end'

ALL_FINDER_START_ANNOTATION_BASES = frozenset([
    FINDER_DISABLE_COMMENT_BASE,
    FINDER_GROUP_COMMENT_START,
])

ALL_FINDER_END_ANNOTATION_BASES = frozenset([
    FINDER_ENABLE_COMMENT_BASE,
    FINDER_GROUP_COMMENT_END,
])

ALL_FINDER_DISABLE_SUFFIXES = frozenset([
    FINDER_COMMENT_SUFFIX_GENERAL,
    FINDER_COMMENT_SUFFIX_STALE,
    FINDER_COMMENT_SUFFIX_UNUSED,
    FINDER_COMMENT_SUFFIX_NARROWING,
])

FINDER_DISABLE_COMMENT_GENERAL = (FINDER_DISABLE_COMMENT_BASE +
                                  FINDER_COMMENT_SUFFIX_GENERAL)
FINDER_DISABLE_COMMENT_STALE = (FINDER_DISABLE_COMMENT_BASE +
                                FINDER_COMMENT_SUFFIX_STALE)
FINDER_DISABLE_COMMENT_UNUSED = (FINDER_DISABLE_COMMENT_BASE +
                                 FINDER_COMMENT_SUFFIX_UNUSED)
FINDER_DISABLE_COMMENT_NARROWING = (FINDER_DISABLE_COMMENT_BASE +
                                    FINDER_COMMENT_SUFFIX_NARROWING)
FINDER_ENABLE_COMMENT_GENERAL = (FINDER_ENABLE_COMMENT_BASE +
                                 FINDER_COMMENT_SUFFIX_GENERAL)
FINDER_ENABLE_COMMENT_STALE = (FINDER_ENABLE_COMMENT_BASE +
                               FINDER_COMMENT_SUFFIX_STALE)
FINDER_ENABLE_COMMENT_UNUSED = (FINDER_ENABLE_COMMENT_BASE +
                                FINDER_COMMENT_SUFFIX_UNUSED)
FINDER_ENABLE_COMMENT_NARROWING = (FINDER_ENABLE_COMMENT_BASE +
                                   FINDER_COMMENT_SUFFIX_NARROWING)

FINDER_DISABLE_COMMENTS = frozenset([
    FINDER_DISABLE_COMMENT_GENERAL,
    FINDER_DISABLE_COMMENT_STALE,
    FINDER_DISABLE_COMMENT_UNUSED,
    FINDER_DISABLE_COMMENT_NARROWING,
])

FINDER_ENABLE_COMMENTS = frozenset([
    FINDER_ENABLE_COMMENT_GENERAL,
    FINDER_ENABLE_COMMENT_STALE,
    FINDER_ENABLE_COMMENT_UNUSED,
    FINDER_ENABLE_COMMENT_NARROWING,
])

FINDER_ENABLE_DISABLE_PAIRS = frozenset([
    (FINDER_DISABLE_COMMENT_GENERAL, FINDER_ENABLE_COMMENT_GENERAL),
    (FINDER_DISABLE_COMMENT_STALE, FINDER_ENABLE_COMMENT_STALE),
    (FINDER_DISABLE_COMMENT_UNUSED, FINDER_ENABLE_COMMENT_UNUSED),
    (FINDER_DISABLE_COMMENT_NARROWING, FINDER_ENABLE_COMMENT_NARROWING),
])

FINDER_GROUP_COMMENTS = frozenset([
    FINDER_GROUP_COMMENT_START,
    FINDER_GROUP_COMMENT_END,
])

ALL_FINDER_COMMENTS = frozenset(FINDER_DISABLE_COMMENTS
                                | FINDER_ENABLE_COMMENTS
                                | FINDER_GROUP_COMMENTS)

GIT_BLAME_REGEX = re.compile(
    r'^[\w\s]+\(.+(?P<date>\d\d\d\d-\d\d-\d\d)[^\)]+\)(?P<content>.*)$',
    re.DOTALL)
TAG_GROUP_REGEX = re.compile(r'# tags: \[([^\]]*)\]', re.MULTILINE | re.DOTALL)

# Annotation comment start (with optional leading whitespace) pattern.
ANNOTATION_COMMENT_START_PATTERN = r' *# '
# Pattern for matching optional description text after an annotation.
ANNOTATION_OPTIONAL_TRAILING_TEXT_PATTERN = r'[^\n]*\n'
# Pattern for matching required description text after an annotation.
ANNOTATION_REQUIRED_TRAILING_TEXT_PATTERN = r'[^\n]+\n'
# Pattern for matching blank or comment lines.
BLANK_OR_COMMENT_LINES_PATTERN = r'(?:\s*| *#[^\n]*\n)*'
# Looks for cases of the group start and end comments with nothing but optional
# whitespace between them.
ALL_STALE_COMMENT_REGEXES = set()
for start_comment, end_comment in FINDER_ENABLE_DISABLE_PAIRS:
  ALL_STALE_COMMENT_REGEXES.add(
      re.compile(
          ANNOTATION_COMMENT_START_PATTERN + start_comment +
          ANNOTATION_OPTIONAL_TRAILING_TEXT_PATTERN +
          BLANK_OR_COMMENT_LINES_PATTERN + ANNOTATION_COMMENT_START_PATTERN +
          end_comment + r'\n', re.MULTILINE | re.DOTALL))
ALL_STALE_COMMENT_REGEXES.add(
    re.compile(
        ANNOTATION_COMMENT_START_PATTERN + FINDER_GROUP_COMMENT_START +
        ANNOTATION_REQUIRED_TRAILING_TEXT_PATTERN +
        BLANK_OR_COMMENT_LINES_PATTERN + ANNOTATION_COMMENT_START_PATTERN +
        FINDER_GROUP_COMMENT_END + r'\n', re.MULTILINE | re.DOTALL))
ALL_STALE_COMMENT_REGEXES = frozenset(ALL_STALE_COMMENT_REGEXES)

# pylint: disable=useless-object-inheritance

# TODO(crbug.com/358591565): Refactor this to remove the need for global
# statements.
_registered_instance = None


def GetInstance() -> 'Expectations':
  return _registered_instance


def RegisterInstance(instance: 'Expectations') -> None:
  global _registered_instance  # pylint: disable=global-statement
  assert _registered_instance is None
  assert isinstance(instance, Expectations)
  _registered_instance = instance


def ClearInstance() -> None:
  global _registered_instance  # pylint: disable=global-statement
  _registered_instance = None


class RemovalType(object):
  STALE = FINDER_COMMENT_SUFFIX_STALE
  UNUSED = FINDER_COMMENT_SUFFIX_UNUSED
  NARROWING = FINDER_COMMENT_SUFFIX_NARROWING


class Expectations(object):
  def __init__(self):
    self._cached_tag_groups = {}

  def CreateTestExpectationMap(
      self, expectation_files: Optional[Union[str, List[str]]],
      tests: Optional[Iterable[str]],
      grace_period: datetime.timedelta) -> data_types.TestExpectationMap:
    """Creates an expectation map based off a file or list of tests.

    Args:
      expectation_files: A filepath or list of filepaths to expectation files to
          read from, or None. If a filepath is specified, |tests| must be None.
      tests: An iterable of strings containing test names to check. If
          specified, |expectation_file| must be None.
      grace_period: A datetime.timedelta specifying how many days old an
          expectation must be in order to be parsed, i.e. how many days old an
          expectation must be before it is a candidate for removal/modification.

    Returns:
      A data_types.TestExpectationMap, although all its BuilderStepMap contents
      will be empty.
    """

    def AddContentToMap(content: str, ex_map: data_types.TestExpectationMap,
                        expectation_file_name: str) -> None:
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
        # Normalize to '/' as the path separator.
        expectation_file_name = os.path.normpath(ef).replace(os.path.sep, '/')
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

  def _GetNonRecentExpectationContent(self, expectation_file_path: str,
                                      num_days: datetime.timedelta) -> str:
    """Gets content from |expectation_file_path| older than |num_days| days.

    Args:
      expectation_file_path: A string containing a filepath pointing to an
          expectation file.
      num_days: A datetime.timedelta containing how old an expectation in the
          given expectation file must be to be included.

    Returns:
      The contents of the expectation file located at |expectation_file_path|
      as a string with any recent expectations removed.
    """
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
    with open(os.devnull, 'w', newline='', encoding='utf-8') as devnull:
      blame_output = subprocess.check_output(cmd,
                                             stderr=devnull).decode('utf-8')
    for line in blame_output.splitlines(True):
      match = GIT_BLAME_REGEX.match(line)
      assert match
      date = match.groupdict()['date']
      line_content = match.groupdict()['content']
      stripped_line_content = line_content.strip()
      # Auto-add comments and blank space, otherwise only add if the grace
      # period has expired.
      if not stripped_line_content or stripped_line_content.startswith('#'):
        content += line_content
      else:
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
    return content

  def RemoveExpectationsFromFile(self,
                                 expectations: List[data_types.Expectation],
                                 expectation_file: str,
                                 removal_type: str) -> Set[str]:
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

    with open(expectation_file, encoding='utf-8') as f:
      input_contents = f.read()

    group_to_expectations, expectation_to_group = (
        self._GetExpectationGroupsFromFileContent(expectation_file,
                                                  input_contents))
    disable_annotated_expectations = (
        self._GetDisableAnnotatedExpectationsFromFile(expectation_file,
                                                      input_contents))

    output_contents = ''
    removed_urls = set()
    removed_lines = set()
    num_removed_lines = 0
    for line_number, line in enumerate(input_contents.splitlines(True)):
      # Auto-add any comments or empty lines
      stripped_line = line.strip()
      if _IsCommentOrBlankLine(stripped_line):
        output_contents += line
        continue

      current_expectation = self._CreateExpectationFromExpectationFileLine(
          line, expectation_file)

      # Add any lines containing expectations that don't match any of the given
      # expectations to remove.
      if any(e for e in expectations if e == current_expectation):
        # Skip any expectations that match if we're in a disable block or there
        # is an inline disable comment.
        disable_block_suffix, disable_block_reason = (
            disable_annotated_expectations.get(current_expectation,
                                               (None, None)))
        if disable_block_suffix and _DisableSuffixIsRelevant(
            disable_block_suffix, removal_type):
          output_contents += line
          logging.info(
              'Would have removed expectation %s, but it is inside a disable '
              'block or has an inline disable with reason %s', stripped_line,
              disable_block_reason)
        elif _ExpectationPartOfNonRemovableGroup(current_expectation,
                                                 group_to_expectations,
                                                 expectation_to_group,
                                                 expectations):
          output_contents += line
          logging.info(
              'Would have removed expectation %s, but it is part of group "%s" '
              'whose members are not all removable.', stripped_line,
              expectation_to_group[current_expectation])
        else:
          bug = current_expectation.bug
          if bug:
            # It's possible to have multiple whitespace-separated bugs per
            # expectation, so treat each one separately.
            removed_urls |= set(bug.split())
          # Record that we've removed this line. By subtracting the number of
          # lines we've already removed, we keep the line numbers relative to
          # the content we're outputting rather than relative to the input
          # content. This also has the effect of automatically compressing
          # contiguous blocks of removal into a single line number.
          removed_lines.add(line_number - num_removed_lines)
          num_removed_lines += 1
      else:
        output_contents += line

    header_length = len(
        self._GetExpectationFileTagHeader(expectation_file).splitlines(True))
    output_contents = _RemoveStaleComments(output_contents, removed_lines,
                                           header_length)

    with open(expectation_file, 'w', newline='', encoding='utf-8') as f:
      f.write(output_contents)

    return removed_urls

  def _GetDisableAnnotatedExpectationsFromFile(
      self, expectation_file: str,
      content: str) -> Dict[data_types.Expectation, Tuple[str, str]]:
    """Extracts expectations which are affected by disable annotations.

    Args:
      expectation_file: A filepath pointing to an expectation file.
      content: A string containing the contents of |expectation_file|.

    Returns:
      A dict mapping data_types.Expectation to (disable_suffix, disable_reason).
      If an expectation is present in this dict, it is affected by a disable
      annotation of some sort. |disable_suffix| is a string specifying which
      type of annotation is applicable, while |disable_reason| is a string
      containing the comment/reason why the disable annotation is present.
    """
    in_disable_block = False
    disable_block_reason = ''
    disable_block_suffix = ''
    disable_annotated_expectations = {}
    for line in content.splitlines(True):
      stripped_line = line.strip()
      # Look for cases of disable/enable blocks.
      if _IsCommentOrBlankLine(stripped_line):
        # Only allow one enable/disable per line.
        assert len([c for c in ALL_FINDER_COMMENTS if c in line]) <= 1
        if _LineContainsDisableComment(line):
          if in_disable_block:
            raise RuntimeError(
                'Invalid expectation file %s - contains a disable comment "%s" '
                'that is in another disable block.' %
                (expectation_file, stripped_line))
          in_disable_block = True
          disable_block_reason = _GetDisableReasonFromComment(line)
          disable_block_suffix = _GetFinderCommentSuffix(line)
        elif _LineContainsEnableComment(line):
          if not in_disable_block:
            raise RuntimeError(
                'Invalid expectation file %s - contains an enable comment "%s" '
                'that is outside of a disable block.' %
                (expectation_file, stripped_line))
          in_disable_block = False
        continue

      current_expectation = self._CreateExpectationFromExpectationFileLine(
          line, expectation_file)

      if in_disable_block:
        disable_annotated_expectations[current_expectation] = (
            disable_block_suffix, disable_block_reason)
      elif _LineContainsDisableComment(line):
        disable_block_reason = _GetDisableReasonFromComment(line)
        disable_block_suffix = _GetFinderCommentSuffix(line)
        disable_annotated_expectations[current_expectation] = (
            disable_block_suffix, disable_block_reason)
    return disable_annotated_expectations

  def _GetExpectationGroupsFromFileContent(
      self, expectation_file: str, content: str
  ) -> Tuple[Dict[str, Set[data_types.Expectation]], Dict[data_types.
                                                          Expectation, str]]:
    """Extracts all groups of expectations from an expectationfile.

    Args:
      expectation_file: A filepath pointing to an expectation file.
      content: A string containing the contents of |expectation_file|.

    Returns:
      A tuple (group_to_expectations, expectation_to_group).
      |group_to_expectations| is a dict of group names to sets of
      data_type.Expectations that belong to that group. |expectation_to_group|
      is the same, but mapped the other way from data_type.Expectations to group
      names.
    """
    group_to_expectations = collections.defaultdict(set)
    expectation_to_group = {}
    group_name = None

    for line in content.splitlines():
      stripped_line = line.strip()
      # Possibly starting/ending a group.
      if _IsCommentOrBlankLine(stripped_line):
        if _LineContainsGroupStartComment(stripped_line):
          # Start of a new group.
          if group_name:
            raise RuntimeError(
                'Invalid expectation file %s - contains a group comment "%s" '
                'that is inside another group block.' %
                (expectation_file, stripped_line))
          group_name = _GetGroupNameFromCommentLine(stripped_line)
        elif _LineContainsGroupEndComment(stripped_line):
          # End of current group.
          if not group_name:
            raise RuntimeError(
                'Invalid expectation file %s - contains a group comment "%s" '
                'without a group start comment.' %
                (expectation_file, stripped_line))
          group_name = None
      elif group_name:
        # Currently in a group.
        e = self._CreateExpectationFromExpectationFileLine(
            stripped_line, expectation_file)
        group_to_expectations[group_name].add(e)
        expectation_to_group[e] = group_name
      # If we aren't in a group, do nothing.
    return group_to_expectations, expectation_to_group

  def _CreateExpectationFromExpectationFileLine(self, line: str,
                                                expectation_file: str
                                                ) -> data_types.Expectation:
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

  def _GetExpectationFileTagHeader(self, expectation_file: str) -> str:
    """Gets the tag header used for expectation files.

    Args:
      expectation_file: A filepath pointing to an expectation file to get the
          tag header from.

    Returns:
      A string containing an expectation file header, i.e. the comment block at
      the top of the file defining possible tags and expected results.
    """
    raise NotImplementedError()

  def ParseTaggedTestListContent(self, content: str
                                 ) -> expectations_parser.TaggedTestListParser:
    """Helper to parse typ expectation files.

    This allows subclasses to avoid adding typ to PYTHONPATH.
    """
    return expectations_parser.TaggedTestListParser(content)

  def FilterToKnownTags(self, tags: Iterable[str]) -> Set[str]:
    """Filters |tags| to only include tags known to expectation files.

    Args:
      tags: An iterable of strings containing tags.

    Returns:
      A set containing the elements of |tags| with any tags that are not defined
      in any expectation files removed.
    """
    return self._GetKnownTags() & set(tags)

  def _GetKnownTags(self) -> Set[str]:
    """Gets all known/defined tags from expectation files.

    Returns:
      A set of strings containing all known/defined tags from expectation files.
    """
    raise NotImplementedError()

  def _FilterToMostSpecificTypTags(self, typ_tags: FrozenSet[str],
                                   expectation_file: str) -> FrozenSet[str]:
    """Filters |typ_tags| to the most specific set.

    Assumes that the tags in |expectation_file| are ordered from least specific
    to most specific within each tag group.

    Args:
      typ_tags: A frozenset of strings containing the typ tags to filter.
      expectations_file: A string containing a filepath pointing to the
          expectation file to filter tags with.

    Returns:
      A frozenset containing the contents of |typ_tags| with only the most
      specific tag from each group remaining.
    """
    # The logic for this function was lifted from the GPU/Blink flake finders,
    # so there may be room to share code between the two.

    if expectation_file not in self._cached_tag_groups:
      with open(expectation_file, encoding='utf-8') as infile:
        contents = infile.read()
      tag_groups = []
      for match in TAG_GROUP_REGEX.findall(contents):
        tag_groups.append(match.lower().strip().replace('#', '').split())
      self._cached_tag_groups[expectation_file] = tag_groups
    tag_groups = self._cached_tag_groups[expectation_file]

    num_matches = 0
    tags_in_same_group = collections.defaultdict(list)
    for tag in typ_tags:
      for index, tag_group in enumerate(tag_groups):
        if tag in tag_group:
          tags_in_same_group[index].append(tag)
          num_matches += 1
          break
    if num_matches != len(typ_tags):
      all_tags = set()
      for group in tag_groups:
        all_tags |= set(group)
      raise RuntimeError('Found tags not in expectation file %s: %s' %
                         (expectation_file, ' '.join(set(typ_tags) - all_tags)))

    filtered_tags = set()
    for index, tags in tags_in_same_group.items():
      if len(tags) == 1:
        filtered_tags.add(tags[0])
      else:
        tag_group = tag_groups[index]
        best_index = -1
        for t in tags:
          i = tag_group.index(t)
          if i > best_index:
            best_index = i
        filtered_tags.add(tag_group[best_index])
    return frozenset(filtered_tags)

  def _ConsolidateKnownOverlappingTags(self, typ_tags: FrozenSet[str]
                                       ) -> FrozenSet[str]:
    """Consolidates tags that are known to overlap/cause issues.

    One known example of this would be dual GPU machines that report tags for
    both GPUs.
    """
    return typ_tags

  def NarrowSemiStaleExpectationScope(
      self, stale_expectation_map: data_types.TestExpectationMap) -> Set[str]:
    """Narrows the scope of expectations in |stale_expectation_map|.

    Expectations are modified such that they only apply to configurations that
    need them, to the best extent possible. If scope narrowing is not possible,
    e.g. the same hardware/software combination reports fully passing on one bot
    but reports some failures on another bot, the expectation will not be
    modified.

    Args:
      stale_expectation_map: A data_types.TestExpectationMap containing
          semi-stale expectations.

    Returns:
      A set of strings containing URLs of bugs associated with the modified
      expectations.
    """
    modified_urls = set()
    cached_disable_annotated_expectations = {}
    for expectation_file, e, builder_map in (
        stale_expectation_map.IterBuilderStepMaps()):
      # Check if the current annotation has scope narrowing disabled.
      if expectation_file not in cached_disable_annotated_expectations:
        with open(expectation_file, encoding='utf-8') as infile:
          disable_annotated_expectations = (
              self._GetDisableAnnotatedExpectationsFromFile(
                  expectation_file, infile.read()))
          cached_disable_annotated_expectations[
              expectation_file] = disable_annotated_expectations
      disable_block_suffix, disable_block_reason = (
          cached_disable_annotated_expectations[expectation_file].get(
              e, ('', '')))
      if _DisableSuffixIsRelevant(disable_block_suffix, RemovalType.NARROWING):
        logging.info(
            'Skipping semi-stale narrowing check for expectation %s since it '
            'has a narrowing disable annotation with reason %s',
            e.AsExpectationFileString(), disable_block_reason)
        continue

      skip_to_next_expectation = False

      pass_tag_sets = set()
      fail_tag_sets = set()
      # Determine which tags sets failures can occur on vs. tag sets that
      # don't have any failures.
      for builder, step, build_stats in builder_map.IterBuildStats():
        if len(build_stats.tag_sets) > 1:
          # This shouldn't really be happening during normal operation, but is
          # expected to happen if a configuration changes, e.g. an OS was
          # upgraded. In these cases, the old data will eventually age out and
          # we will stop getting multiple tag sets.
          logging.warning(
              'Step %s on builder %s produced multiple tag sets: %s. Not '
              'narrowing expectation scope for expectation %s.', step, builder,
              build_stats.tag_sets, e.AsExpectationFileString())
          skip_to_next_expectation = True
          break
        if build_stats.NeverNeededExpectation(e):
          pass_tag_sets |= build_stats.tag_sets
        else:
          fail_tag_sets |= build_stats.tag_sets
      if skip_to_next_expectation:
        continue

      # Remove all instances of tags that are shared between all sets other than
      # the tags that were used by the expectation, as they are redundant.
      common_tags = set()
      for ts in pass_tag_sets:
        common_tags |= ts
        # We only need one initial tag set, but sets do not have a way of
        # retrieving a single element other than pop(), which removes the
        # element, which we don't want.
        break
      for ts in pass_tag_sets | fail_tag_sets:
        common_tags &= ts
      common_tags -= e.tags
      pass_tag_sets = {ts - common_tags for ts in pass_tag_sets}
      fail_tag_sets = {ts - common_tags for ts in fail_tag_sets}

      # Calculate new tag sets that should be functionally equivalent to the
      # single, more broad tag set that we are replacing. This is done by
      # checking if the intersection between any pairs of fail tag sets are
      # still distinct from any pass tag sets, i.e. if the intersection between
      # fail tag sets is still a valid fail tag set. If so, the original sets
      # are replaced by the intersection.
      new_tag_sets = set()
      covered_fail_tag_sets = set()
      for fail_tags in fail_tag_sets:
        if any(fail_tags <= pt for pt in pass_tag_sets):
          logging.warning(
              'Unable to determine what makes failing configs unique for %s, '
              'not narrowing expectation scope.', e.AsExpectationFileString())
          skip_to_next_expectation = True
          break
        if fail_tags in covered_fail_tag_sets:
          continue
        tag_set_to_add = fail_tags
        for ft in fail_tag_sets:
          if ft in covered_fail_tag_sets:
            continue
          intersection = tag_set_to_add & ft
          if any(intersection <= pt for pt in pass_tag_sets):
            # Intersection is too small, as it also covers a passing tag set.
            continue
          if any(intersection <= cft for cft in covered_fail_tag_sets):
            # Both the intersection and some tag set from new_tag_sets
            # apply to the same original failing tag set,
            # which means if we add the intersection to new_tag_sets,
            # they will conflict on the bot from the original failing tag set.
            # The above check works because new_tag_sets and
            # covered_fail_tag_sets are updated together below.
            continue
          tag_set_to_add = intersection
        new_tag_sets.add(tag_set_to_add)
        covered_fail_tag_sets.update(cft for cft in fail_tag_sets
                                     if tag_set_to_add <= cft)
      if skip_to_next_expectation:
        continue

      # Remove anything we know could be problematic, e.g. causing expectation
      # file parsing errors.
      new_tag_sets = {
          self._ConsolidateKnownOverlappingTags(nts)
          for nts in new_tag_sets
      }
      new_tag_sets = {
          self._FilterToMostSpecificTypTags(nts, expectation_file)
          for nts in new_tag_sets
      }

      # Replace the existing expectation with our new ones.
      with open(expectation_file, encoding='utf-8') as infile:
        file_contents = infile.read()
      line, _ = self._GetExpectationLine(e, file_contents, expectation_file)
      modified_urls |= set(e.bug.split())
      expectation_strs = []
      for new_tags in new_tag_sets:
        expectation_copy = copy.copy(e)
        expectation_copy.tags = new_tags
        expectation_strs.append(expectation_copy.AsExpectationFileString())
      expectation_strs.sort()
      replacement_lines = '\n'.join(expectation_strs)
      file_contents = file_contents.replace(line, replacement_lines)
      with open(expectation_file, 'w', newline='', encoding='utf-8') as outfile:
        outfile.write(file_contents)

    return modified_urls

  def _GetExpectationLine(self, expectation: data_types.Expectation,
                          file_contents: str, expectation_file: str
                          ) -> Union[Tuple[None, None], Tuple[str, int]]:
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

  def FindOrphanedBugs(self, affected_urls: Iterable[str]) -> Set[str]:
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
      with open(ef, encoding='utf-8') as infile:
        contents = infile.read()
      for url in affected_urls:
        if url in seen_bugs:
          continue
        if url in contents:
          seen_bugs.add(url)
    return set(affected_urls) - seen_bugs

  def GetExpectationFilepaths(self) -> List[str]:
    """Gets all the filepaths to expectation files of interest.

    Returns:
      A list of strings, each element being a filepath pointing towards an
      expectation file.
    """
    raise NotImplementedError()


def _LineContainsGroupStartComment(line: str) -> bool:
  return FINDER_GROUP_COMMENT_START in line


def _LineContainsGroupEndComment(line: str) -> bool:
  return FINDER_GROUP_COMMENT_END in line


def _LineContainsDisableComment(line: str) -> bool:
  return FINDER_DISABLE_COMMENT_BASE in line


def _LineContainsEnableComment(line: str) -> bool:
  return FINDER_ENABLE_COMMENT_BASE in line


def _GetGroupNameFromCommentLine(line: str) -> str:
  """Gets the group name from the finder comment on the given line."""
  assert FINDER_GROUP_COMMENT_START in line
  uncommented_line = line.lstrip('#').strip()
  split_line = uncommented_line.split(maxsplit=1)
  if len(split_line) != 2:
    raise RuntimeError('Given line %s did not have a group name.' % line)
  return split_line[1]


def _GetFinderCommentSuffix(line: str) -> str:
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
  assert suffix in ALL_FINDER_DISABLE_SUFFIXES
  return suffix


def _LineContainsRelevantDisableComment(line: str, removal_type: str) -> bool:
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


def _DisableSuffixIsRelevant(suffix: str, removal_type: str) -> bool:
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


def _GetDisableReasonFromComment(line: str) -> str:
  suffix = _GetFinderCommentSuffix(line)
  return line.split(FINDER_DISABLE_COMMENT_BASE + suffix, 1)[1].strip()


def _IsCommentOrBlankLine(line: str) -> bool:
  return not line or line.startswith('#')


def _ExpectationPartOfNonRemovableGroup(
    current_expectation: data_types.Expectation,
    group_to_expectations: Dict[str, Set[data_types.Expectation]],
    expectation_to_group: Dict[data_types.Expectation, str],
    removable_expectations: List[data_types.Expectation]):
  """Determines if the given expectation is part of a non-removable group.

  This is the case if the expectation is part of a group, but not all
  expectations in that group are marked as removable.

  Args:
    current_expectation: A data_types.Expectation that is being checked.
    group_to_expectations: A dict mapping group names to sets of expectations
        contained within that group.
    expectation_to_group: A dict mapping an expectation to the group name it
        belongs to.
    removable_expectations: A list of all expectations that are removable.
  """
  # Since we'll only ever be using this to check for inclusion, use a set
  # for efficiency.
  removable_expectations = set(removable_expectations)

  group_name = expectation_to_group.get(current_expectation)
  if not group_name:
    return False

  all_expectations_in_group = group_to_expectations[group_name]
  group_removable = all_expectations_in_group <= removable_expectations
  return not group_removable


def _RemoveStaleComments(content: str, removed_lines: Set[int],
                         header_length: int) -> str:
  """Attempts to remove stale contents from the given expectation file content.

  Args:
    content: A string containing the contents of an expectation file.
    removed_lines: A set of ints denoting which line numbers were removed in
        the process of creating |content|.
    header_length: An int denoting how many lines long the tag header is.

  Returns:
    A copy of |content| with various stale comments removed, e.g. group blocks
    if the group has been removed.
  """
  # Look for the case where we've removed an entire block of expectations that
  # were preceded by a comment, which we should remove.
  comment_line_numbers_to_remove = []
  split_content = content.splitlines(True)
  for rl in removed_lines:
    found_trailing_annotation = False
    found_starting_annotation = False
    # Check for the end of the file, a blank line, or a comment after the block
    # we've removed.
    if rl < len(split_content):
      stripped_line = split_content[rl].strip()
      if stripped_line and not stripped_line.startswith('#'):
        # We found an expectation, so the entire expectation block wasn't
        # removed.
        continue
      if any(annotation in stripped_line
             for annotation in ALL_FINDER_END_ANNOTATION_BASES):
        found_trailing_annotation = True
    # Look for a comment block immediately preceding the block we removed.
    comment_line_number = rl - 1
    while comment_line_number != header_length - 1:
      stripped_line = split_content[comment_line_number].strip()
      if stripped_line.startswith('#'):
        # If we find what should be a trailing annotation, stop immediately so
        # we don't accidentally remove it and create an orphan earlier in the
        # file.
        if any(annotation in stripped_line
               for annotation in ALL_FINDER_END_ANNOTATION_BASES):
          break
        if any(annotation in stripped_line
               for annotation in ALL_FINDER_START_ANNOTATION_BASES):
          # If we've already found a starting annotation, skip past this line.
          # This is to handle the case of nested annotations, e.g. a
          # disable-narrowing block inside of a group block. We'll find the
          # inner-most block here and remove it. Any outer blocks will be
          # removed as part of the lingering stale annotation removal later on.
          # If we don't skip past these outer annotations, then we get left with
          # orphaned trailing annotations.
          if found_starting_annotation:
            comment_line_number -= 1
            continue
          found_starting_annotation = True
          # If we found a starting annotation but not a trailing annotation, we
          # shouldn't remove the starting one, as that would cause the trailing
          # one that is later in the file to be orphaned. We also don't want to
          # continue and remove comments above that since it is assumedly still
          # valid.
          if found_starting_annotation and not found_trailing_annotation:
            break
        comment_line_numbers_to_remove.append(comment_line_number)
        comment_line_number -= 1
      else:
        break
    # In the event that we found both a start and trailing annotation, we need
    # to also remove the trailing one.
    if found_trailing_annotation and found_starting_annotation:
      comment_line_numbers_to_remove.append(rl)

  # Actually remove the comments we found above.
  for i in comment_line_numbers_to_remove:
    split_content[i] = ''
  if comment_line_numbers_to_remove:
    content = ''.join(split_content)

  # Remove any lingering cases of stale annotations that we can easily detect.
  for regex in ALL_STALE_COMMENT_REGEXES:
    for match in regex.findall(content):
      content = content.replace(match, '')

  return content
