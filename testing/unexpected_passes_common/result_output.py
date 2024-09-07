# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to outputting script results in a human-readable format.

Also probably a good example of how to *not* write HTML.
"""

import collections
import logging
import sys
import tempfile
from typing import Any, Dict, IO, List, Optional, Set, Union

import six

from unexpected_passes_common import data_types

# Used for posting Buganizer comments.
from blinkpy.w3c import buganizer

FULL_PASS = 'Fully passed in the following'
PARTIAL_PASS = 'Partially passed in the following'
NEVER_PASS = 'Never passed in the following'

HTML_HEADER = """\
<!DOCTYPE html>
<html>
<head>
<meta content="width=device-width">
<style>
.collapsible_group {
  background-color: #757575;
  border: none;
  color: white;
  font-size:20px;
  outline: none;
  text-align: left;
  width: 100%;
}
.active_collapsible_group, .collapsible_group:hover {
  background-color: #474747;
}
.highlighted_collapsible_group {
  background-color: #008000;
  border: none;
  color: white;
  font-size:20px;
  outline: none;
  text-align: left;
  width: 100%;
}
.active_highlighted_collapsible_group, .highlighted_collapsible_group:hover {
  background-color: #004d00;
}
.content {
  background-color: #e1e4e8;
  display: none;
  padding: 0 25px;
}
button {
  user-select: text;
}
h1 {
  background-color: black;
  color: white;
}
</style>
</head>
<body>
"""

HTML_FOOTER = """\
<script>
function OnClickImpl(element) {
  let sibling = element.nextElementSibling;
  if (sibling.style.display === "block") {
    sibling.style.display = "none";
  } else {
    sibling.style.display = "block";
  }
}

function OnClick() {
  this.classList.toggle("active_collapsible_group");
  OnClickImpl(this);
}

function OnClickHighlighted() {
  this.classList.toggle("active_highlighted_collapsible_group");
  OnClickImpl(this);
}

// Repeatedly bubble up the highlighted_collapsible_group class as long as all
// siblings are highlighted.
let found_element_to_convert = false;
do {
  found_element_to_convert = false;
  // Get an initial list of all highlighted_collapsible_groups.
  let highlighted_collapsible_groups = document.getElementsByClassName(
      "highlighted_collapsible_group");
  let highlighted_list = [];
  for (elem of highlighted_collapsible_groups) {
    highlighted_list.push(elem);
  }

  // Bubble up the highlighted_collapsible_group class.
  while (highlighted_list.length) {
    elem = highlighted_list.shift();
    if (elem.tagName == 'BODY') {
      continue;
    }
    if (elem.classList.contains("content")) {
      highlighted_list.push(elem.previousElementSibling);
      continue;
    }
    if (elem.classList.contains("collapsible_group")) {
      found_element_to_convert = true;
      elem.classList.add("highlighted_collapsible_group");
      elem.classList.remove("collapsible_group");
    }

    sibling_elements = elem.parentElement.children;
    let found_non_highlighted_group = false;
    for (e of sibling_elements) {
      if (e.classList.contains("collapsible_group")) {
        found_non_highlighted_group = true;
        break
      }
    }
    if (!found_non_highlighted_group) {
      highlighted_list.push(elem.parentElement);
    }
  }
} while (found_element_to_convert);

// Apply OnClick listeners so [highlighted_]collapsible_groups properly
// shrink/expand.
let collapsible_groups = document.getElementsByClassName("collapsible_group");
for (element of collapsible_groups) {
  element.addEventListener("click", OnClick);
}

highlighted_collapsible_groups = document.getElementsByClassName(
    "highlighted_collapsible_group");
for (element of highlighted_collapsible_groups) {
  element.addEventListener("click", OnClickHighlighted);
}
</script>
</body>
</html>
"""

SECTION_STALE = 'Stale Expectations (Passed 100% Everywhere, Can Remove)'
SECTION_SEMI_STALE = ('Semi Stale Expectations (Passed 100% In Some Places, '
                      'But Not Everywhere - Can Likely Be Modified But Not '
                      'Necessarily Removed)')
SECTION_ACTIVE = ('Active Expectations (Failed At Least Once Everywhere, '
                  'Likely Should Be Left Alone)')
SECTION_UNMATCHED = ('Unmatched Results (An Expectation Existed When The Test '
                     'Ran, But No Matching One Currently Exists OR The '
                     'Expectation Is Too New)')
SECTION_UNUSED = ('Unused Expectations (Indicative Of The Configuration No '
                  'Longer Being Tested Or Tags Changing)')

MAX_BUGS_PER_LINE = 5
MAX_CHARACTERS_PER_CL_LINE = 72

BUGANIZER_COMMENT = ('The unexpected pass finder removed the last expectation '
                     'associated with this bug. An associated CL should be '
                     'landing shortly, after which this bug can be closed once '
                     'a human confirms there is no more work to be done.')

ElementType = Union[Dict[str, Any], List[str], str]
# Sample:
# {
#   expectation_file: {
#     test_name: {
#       expectation_summary: {
#         builder_name: {
#           'Fully passed in the following': [
#             step1,
#           ],
#           'Partially passed in the following': {
#             step2: [
#               failure_link,
#             ],
#           },
#           'Never passed in the following': [
#             step3,
#           ],
#         }
#       }
#     }
#   }
# }
FullOrNeverPassValue = List[str]
PartialPassValue = Dict[str, List[str]]
PassValue = Union[FullOrNeverPassValue, PartialPassValue]
BuilderToPassMap = Dict[str, Dict[str, PassValue]]
ExpectationToBuilderMap = Dict[str, BuilderToPassMap]
TestToExpectationMap = Dict[str, ExpectationToBuilderMap]
ExpectationFileStringDict = Dict[str, TestToExpectationMap]
# Sample:
# {
#   test_name: {
#     builder_name: {
#       step_name: [
#         individual_result_string_1,
#         individual_result_string_2,
#         ...
#       ],
#       ...
#     },
#     ...
#   },
#   ...
# }
StepToResultsMap = Dict[str, List[str]]
BuilderToStepMap = Dict[str, StepToResultsMap]
TestToBuilderStringDict = Dict[str, BuilderToStepMap]
# Sample:
# {
#   result_output.FULL_PASS: {
#     builder_name: [
#       step_name (total passes / total builds)
#     ],
#   },
#   result_output.NEVER_PASS: {
#     builder_name: [
#       step_name (total passes / total builds)
#     ],
#   },
#   result_output.PARTIAL_PASS: {
#     builder_name: {
#       step_name (total passes / total builds): [
#         failure links,
#       ],
#     },
#   },
# }
FullOrNeverPassStepValue = List[str]
PartialPassStepValue = Dict[str, List[str]]
PassStepValue = Union[FullOrNeverPassStepValue, PartialPassStepValue]

UnmatchedResultsType = Dict[str, data_types.ResultListType]
UnusedExpectation = Dict[str, List[data_types.Expectation]]

RemovedUrlsType = Union[List[str], Set[str]]


def OutputResults(stale_dict: data_types.TestExpectationMap,
                  semi_stale_dict: data_types.TestExpectationMap,
                  active_dict: data_types.TestExpectationMap,
                  unmatched_results: UnmatchedResultsType,
                  unused_expectations: UnusedExpectation,
                  output_format: str,
                  file_handle: Optional[IO] = None) -> None:
  """Outputs script results to |file_handle|.

  Args:
    stale_dict: A data_types.TestExpectationMap containing all the stale
        expectations.
    semi_stale_dict: A data_types.TestExpectationMap containing all the
        semi-stale expectations.
    active_dict: A data_types.TestExpectationmap containing all the active
        expectations.
    ummatched_results: Any unmatched results found while filling
        |test_expectation_map|, as returned by
        queries.FillExpectationMapFor[Ci|Try]Builders().
    unused_expectations: A dict from expectation file (str) to list of
        unmatched Expectations that were pulled out of |test_expectation_map|
    output_format: A string denoting the format to output to. Valid values are
        "print" and "html".
    file_handle: An optional open file-like object to output to. If not
        specified, a suitable default will be used.
  """
  assert isinstance(stale_dict, data_types.TestExpectationMap)
  assert isinstance(semi_stale_dict, data_types.TestExpectationMap)
  assert isinstance(active_dict, data_types.TestExpectationMap)
  logging.info('Outputting results in format %s', output_format)
  stale_str_dict = _ConvertTestExpectationMapToStringDict(stale_dict)
  semi_stale_str_dict = _ConvertTestExpectationMapToStringDict(semi_stale_dict)
  active_str_dict = _ConvertTestExpectationMapToStringDict(active_dict)
  unmatched_results_str_dict = _ConvertUnmatchedResultsToStringDict(
      unmatched_results)
  unused_expectations_str_list = _ConvertUnusedExpectationsToStringDict(
      unused_expectations)

  if output_format == 'print':
    file_handle = file_handle or sys.stdout
    if stale_dict:
      file_handle.write(SECTION_STALE + '\n')
      RecursivePrintToFile(stale_str_dict, 0, file_handle)
    if semi_stale_dict:
      file_handle.write(SECTION_SEMI_STALE + '\n')
      RecursivePrintToFile(semi_stale_str_dict, 0, file_handle)
    if active_dict:
      file_handle.write(SECTION_ACTIVE + '\n')
      RecursivePrintToFile(active_str_dict, 0, file_handle)

    if unused_expectations_str_list:
      file_handle.write('\n' + SECTION_UNUSED + '\n')
      RecursivePrintToFile(unused_expectations_str_list, 0, file_handle)
    if unmatched_results_str_dict:
      file_handle.write('\n' + SECTION_UNMATCHED + '\n')
      RecursivePrintToFile(unmatched_results_str_dict, 0, file_handle)

  elif output_format == 'html':
    should_close_file = False
    if not file_handle:
      should_close_file = True
      file_handle = tempfile.NamedTemporaryFile(delete=False,
                                                suffix='.html',
                                                mode='w')

    file_handle.write(HTML_HEADER)
    if stale_dict:
      file_handle.write('<h1>' + SECTION_STALE + '</h1>\n')
      _RecursiveHtmlToFile(stale_str_dict, file_handle)
    if semi_stale_dict:
      file_handle.write('<h1>' + SECTION_SEMI_STALE + '</h1>\n')
      _RecursiveHtmlToFile(semi_stale_str_dict, file_handle)
    if active_dict:
      file_handle.write('<h1>' + SECTION_ACTIVE + '</h1>\n')
      _RecursiveHtmlToFile(active_str_dict, file_handle)

    if unused_expectations_str_list:
      file_handle.write('\n<h1>' + SECTION_UNUSED + '</h1>\n')
      _RecursiveHtmlToFile(unused_expectations_str_list, file_handle)
    if unmatched_results_str_dict:
      file_handle.write('\n<h1>' + SECTION_UNMATCHED + '</h1>\n')
      _RecursiveHtmlToFile(unmatched_results_str_dict, file_handle)

    file_handle.write(HTML_FOOTER)
    if should_close_file:
      file_handle.close()
    print('Results available at file://%s' % file_handle.name)
  else:
    raise RuntimeError('Unsupported output format %s' % output_format)


def RecursivePrintToFile(element: ElementType, depth: int,
                         file_handle: IO) -> None:
  """Recursively prints |element| as text to |file_handle|.

  Args:
    element: A dict, list, or str/unicode to output.
    depth: The current depth of the recursion as an int.
    file_handle: An open file-like object to output to.
  """
  if element is None:
    element = str(element)
  if isinstance(element, six.string_types):
    file_handle.write(('  ' * depth) + element + '\n')
  elif isinstance(element, dict):
    for k, v in element.items():
      RecursivePrintToFile(k, depth, file_handle)
      RecursivePrintToFile(v, depth + 1, file_handle)
  elif isinstance(element, list):
    for i in element:
      RecursivePrintToFile(i, depth, file_handle)
  else:
    raise RuntimeError('Given unhandled type %s' % type(element))


def _RecursiveHtmlToFile(element: ElementType, file_handle: IO) -> None:
  """Recursively outputs |element| as HTMl to |file_handle|.

  Iterables will be output as a collapsible section containing any of the
  iterable's contents.

  Any link-like text will be turned into anchor tags.

  Args:
    element: A dict, list, or str/unicode to output.
    file_handle: An open file-like object to output to.
  """
  if isinstance(element, six.string_types):
    file_handle.write('<p>%s</p>\n' % _LinkifyString(element))
  elif isinstance(element, dict):
    for k, v in element.items():
      html_class = 'collapsible_group'
      # This allows us to later (in JavaScript) recursively highlight sections
      # that are likely of interest to the user, i.e. whose expectations can be
      # modified.
      if k and FULL_PASS in k:
        html_class = 'highlighted_collapsible_group'
      file_handle.write('<button type="button" class="%s">%s</button>\n' %
                        (html_class, k))
      file_handle.write('<div class="content">\n')
      _RecursiveHtmlToFile(v, file_handle)
      file_handle.write('</div>\n')
  elif isinstance(element, list):
    for i in element:
      _RecursiveHtmlToFile(i, file_handle)
  else:
    raise RuntimeError('Given unhandled type %s' % type(element))


def _LinkifyString(s: str) -> str:
  """Turns instances of links into anchor tags.

  Args:
    s: The string to linkify.

  Returns:
    A copy of |s| with instances of links turned into anchor tags pointing to
    the link.
  """
  for component in s.split():
    if component.startswith('http'):
      component = component.strip(',.!')
      s = s.replace(component, '<a href="%s">%s</a>' % (component, component))
  return s


def _ConvertTestExpectationMapToStringDict(
    test_expectation_map: data_types.TestExpectationMap
) -> ExpectationFileStringDict:
  """Converts |test_expectation_map| to a dict of strings for reporting.

  Args:
    test_expectation_map: A data_types.TestExpectationMap.

  Returns:
    A string dictionary representation of |test_expectation_map| in the
    following format:
    {
      expectation_file: {
        test_name: {
          expectation_summary: {
            builder_name: {
              'Fully passed in the following': [
                step1,
              ],
              'Partially passed in the following': {
                step2: [
                  failure_link,
                ],
              },
              'Never passed in the following': [
                step3,
              ],
            }
          }
        }
      }
    }
  """
  assert isinstance(test_expectation_map, data_types.TestExpectationMap)
  output_dict = {}
  # This initially looks like a good target for using
  # data_types.TestExpectationMap's iterators since there are many nested loops.
  # However, we need to reset state in different loops, and the alternative of
  # keeping all the state outside the loop and resetting under certain
  # conditions ends up being less readable than just using nested loops.
  for expectation_file, expectation_map in test_expectation_map.items():
    output_dict[expectation_file] = {}

    for expectation, builder_map in expectation_map.items():
      test_name = expectation.test
      expectation_str = _FormatExpectation(expectation)
      output_dict[expectation_file].setdefault(test_name, {})
      output_dict[expectation_file][test_name][expectation_str] = {}

      for builder_name, step_map in builder_map.items():
        output_dict[expectation_file][test_name][expectation_str][
            builder_name] = {}
        fully_passed = []
        partially_passed = {}
        never_passed = []

        for step_name, stats in step_map.items():
          if stats.NeverNeededExpectation(expectation):
            fully_passed.append(AddStatsToStr(step_name, stats))
          elif stats.AlwaysNeededExpectation(expectation):
            never_passed.append(AddStatsToStr(step_name, stats))
          else:
            assert step_name not in partially_passed
            partially_passed[step_name] = stats

        output_builder_map = output_dict[expectation_file][test_name][
            expectation_str][builder_name]
        if fully_passed:
          output_builder_map[FULL_PASS] = fully_passed
        if partially_passed:
          output_builder_map[PARTIAL_PASS] = {}
          for step_name, stats in partially_passed.items():
            s = AddStatsToStr(step_name, stats)
            output_builder_map[PARTIAL_PASS][s] = list(stats.failure_links)
        if never_passed:
          output_builder_map[NEVER_PASS] = never_passed
  return output_dict


def _ConvertUnmatchedResultsToStringDict(unmatched_results: UnmatchedResultsType
                                         ) -> TestToBuilderStringDict:
  """Converts |unmatched_results| to a dict of strings for reporting.

  Args:
    unmatched_results: A dict mapping builder names (string) to lists of
        data_types.Result who did not have a matching expectation.

  Returns:
    A string dictionary representation of |unmatched_results| in the following
    format:
    {
      test_name: {
        builder_name: {
          step_name: [
            individual_result_string_1,
            individual_result_string_2,
            ...
          ],
          ...
        },
        ...
      },
      ...
    }
  """
  output_dict = {}
  for builder, results in unmatched_results.items():
    for r in results:
      builder_map = output_dict.setdefault(r.test, {})
      step_map = builder_map.setdefault(builder, {})
      result_str = 'Got "%s" on %s with tags [%s]' % (
          r.actual_result, data_types.BuildLinkFromBuildId(
              r.build_id), ' '.join(r.tags))
      step_map.setdefault(r.step, []).append(result_str)
  return output_dict


def _ConvertUnusedExpectationsToStringDict(
    unused_expectations: UnusedExpectation) -> Dict[str, List[str]]:
  """Converts |unused_expectations| to a dict of strings for reporting.

  Args:
    unused_expectations: A dict mapping expectation file (str) to lists of
        data_types.Expectation who did not have any matching results.

  Returns:
    A string dictionary representation of |unused_expectations| in the following
    format:
    {
      expectation_file: [
        expectation1,
        expectation2,
      ],
    }
    The expectations are in a format similar to what would be present as a line
    in an expectation file.
  """
  output_dict = {}
  for expectation_file, expectations in unused_expectations.items():
    expectation_str_list = []
    for e in expectations:
      expectation_str_list.append(e.AsExpectationFileString())
    output_dict[expectation_file] = expectation_str_list
  return output_dict


def _FormatExpectation(expectation: data_types.Expectation) -> str:
  return '"%s" expectation on "%s"' % (' '.join(
      expectation.expected_results), ' '.join(expectation.tags))


def AddStatsToStr(s: str, stats: data_types.BuildStats) -> str:
  return '%s %s' % (s, stats.GetStatsAsString())


def OutputAffectedUrls(removed_urls: RemovedUrlsType,
                       orphaned_urls: Optional[RemovedUrlsType] = None,
                       bug_file_handle: Optional[IO] = None,
                       auto_close_bugs: bool = True) -> None:
  """Outputs URLs of affected expectations for easier consumption by the user.

  Outputs the following:

  1. A string suitable for passing to Chrome via the command line to
     open all bugs in the browser.
  2. A string suitable for copying into the CL description to associate the CL
     with all the affected bugs.
  3. A string containing any bugs that should be closable since there are no
     longer any associated expectations.

  Args:
    removed_urls: A set or list of strings containing bug URLs.
    orphaned_urls: A subset of |removed_urls| whose bugs no longer have any
        corresponding expectations.
    bug_file_handle: An optional open file-like object to write CL description
        bug information to. If not specified, will print to the terminal.
    auto_close_bugs: A boolean specifying whether bugs in |orphaned_urls| should
        be auto-closed on CL submission or not. If not closed, a comment will
        be posted instead.
  """
  removed_urls = list(removed_urls)
  removed_urls.sort()
  orphaned_urls = orphaned_urls or []
  orphaned_urls = list(orphaned_urls)
  orphaned_urls.sort()
  _OutputAffectedUrls(removed_urls, orphaned_urls)
  _OutputUrlsForClDescription(removed_urls,
                              orphaned_urls,
                              file_handle=bug_file_handle,
                              auto_close_bugs=auto_close_bugs)


def _OutputAffectedUrls(affected_urls: List[str],
                        orphaned_urls: List[str],
                        file_handle: Optional[IO] = None) -> None:
  """Outputs |urls| for opening in a browser as affected bugs.

  Args:
    affected_urls: A list of strings containing URLs to output.
    orphaned_urls: A list of strings containing URLs to output as closable.
    file_handle: A file handle to write the string to. Defaults to stdout.
  """
  _OutputUrlsForCommandLine(affected_urls, 'Affected bugs', file_handle)
  if orphaned_urls:
    _OutputUrlsForCommandLine(orphaned_urls, 'Closable bugs', file_handle)


def _OutputUrlsForCommandLine(urls: List[str],
                              description: str,
                              file_handle: Optional[IO] = None) -> None:
  """Outputs |urls| for opening in a browser.

  The output string is meant to be passed to a browser via the command line in
  order to open all URLs in that browser, e.g.

  `google-chrome https://crbug.com/1234 https://crbug.com/2345`

  Args:
    urls: A list of strings containing URLs to output.
    description: A description of the URLs to be output.
    file_handle: A file handle to write the string to. Defaults to stdout.
  """
  file_handle = file_handle or sys.stdout

  def _StartsWithHttp(url: str) -> bool:
    return url.startswith('https://') or url.startswith('http://')

  urls = [u if _StartsWithHttp(u) else 'https://%s' % u for u in urls]
  file_handle.write('%s: %s\n' % (description, ' '.join(urls)))


def _OutputUrlsForClDescription(affected_urls: List[str],
                                orphaned_urls: List[str],
                                file_handle: Optional[IO] = None,
                                auto_close_bugs: bool = True) -> None:
  """Outputs |urls| for use in a CL description.

  Output adheres to the line length recommendation and max number of bugs per
  line supported in Gerrit.

  Args:
    affected_urls: A list of strings containing URLs to output.
    orphaned_urls: A list of strings containing URLs to output as closable.
    file_handle: A file handle to write the string to. Defaults to stdout.
    auto_close_bugs: A boolean specifying whether bugs in |orphaned_urls| should
        be auto-closed on CL submission or not. If not closed, a comment will
        be posted instead.
  """

  def AddBugTypeToOutputString(urls, prefix):
    output_str = ''
    current_line = ''
    bugs_on_line = 0

    urls = collections.deque(urls)

    while len(urls):
      current_bug = urls.popleft()
      current_bug = current_bug.split('crbug.com/', 1)[1]
      # Handles cases like crbug.com/angleproject/1234.
      current_bug = current_bug.replace('/', ':')

      # First bug on the line.
      if not current_line:
        current_line = '%s %s' % (prefix, current_bug)
      # Bug or length limit hit for line.
      elif (
          len(current_line) + len(current_bug) + 2 > MAX_CHARACTERS_PER_CL_LINE
          or bugs_on_line >= MAX_BUGS_PER_LINE):
        output_str += current_line + '\n'
        bugs_on_line = 0
        current_line = '%s %s' % (prefix, current_bug)
      # Can add to current line.
      else:
        current_line += ', %s' % current_bug

      bugs_on_line += 1

    output_str += current_line + '\n'
    return output_str

  file_handle = file_handle or sys.stdout
  affected_but_not_closable = set(affected_urls) - set(orphaned_urls)
  affected_but_not_closable = list(affected_but_not_closable)
  affected_but_not_closable.sort()

  output_str = ''
  if affected_but_not_closable:
    output_str += AddBugTypeToOutputString(affected_but_not_closable, 'Bug:')
  if orphaned_urls:
    if auto_close_bugs:
      output_str += AddBugTypeToOutputString(orphaned_urls, 'Fixed:')
    else:
      output_str += AddBugTypeToOutputString(orphaned_urls, 'Bug:')
      _PostCommentsToOrphanedBugs(orphaned_urls)

  file_handle.write('Affected bugs for CL description:\n%s' % output_str)


def _PostCommentsToOrphanedBugs(orphaned_urls: List[str]) -> None:
  """Posts comments to bugs in |orphaned_urls| saying they can likely be closed.

  Does not post again if the comment has been posted before in the past.

  Args:
    orphaned_urls: A list of strings containing URLs to post comments to.
  """

  try:
    buganizer_client = _GetBuganizerClient()
  except buganizer.BuganizerError as e:
    logging.error(
        'Encountered error when authenticating, cannot post comments. %s', e)
    return

  for url in orphaned_urls:
    try:
      comment_list = buganizer_client.GetIssueComments(url)
      # GetIssueComments currently returns a dict if something goes wrong
      # instead of raising an exception.
      if isinstance(comment_list, dict):
        logging.exception('Failed to get comments from %s: %s', url,
                          comment_list.get('error', 'error not provided'))
        continue
      existing_comments = [c['comment'] for c in comment_list]
      if BUGANIZER_COMMENT not in existing_comments:
        buganizer_client.NewComment(url, BUGANIZER_COMMENT)
    except buganizer.BuganizerError:
      logging.exception('Could not fetch or add comments for %s', url)


def _GetBuganizerClient() -> buganizer.BuganizerClient:
  """Helper function to get a usable Buganizer client."""
  return buganizer.BuganizerClient()
