# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to outputting script results in a human-readable format.

Also probably a good example of how to *not* write HTML.
"""

import collections
import logging
import sys
import tempfile

from unexpected_passes import data_types

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
  }
  if (elem.classList.contains("collapsible_group")) {
    elem.classList.add("highlighted_collapsible_group");
    elem.classList.remove("collapsible_group");
  }
  highlighted_list.push(elem.parentElement);
}

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
                     'Ran, But No Matching One Currently Exists)')
SECTION_UNUSED = ('Unused Expectations (Indicative Of The Configuration No '
                  'Longer Being Tested Or Tags Changing)')

MAX_BUGS_PER_LINE = 5
MAX_CHARACTERS_PER_CL_LINE = 72


def OutputResults(stale_dict,
                  semi_stale_dict,
                  active_dict,
                  unmatched_results,
                  unused_expectations,
                  output_format,
                  file_handle=None):
  """Outputs script results to |file_handle|.

  Args:
    stale_dict: A map in the format returned by
        expectations.CreateTestExpectationMap() containing all the stale
        expectations.
    semi_stale_dict: A map in the format returned by
        expectations.CreateTestExpectationMap() containing all the semi-stale
        expectations.
    active_dict: A map in the format returned by
        expectations.CreateTestExpectationMap() containing all the active
        expectations.
    ummatched_results: Any unmatched results found while filling
        |test_expectation_map|, as returned by
        queries.FillExpectationMapFor[Ci|Try]Builders().
    unused_expectations: A list of any unmatched Expectations that were pulled
        out of |test_expectation_map|.
    output_format: A string denoting the format to output to. Valid values are
        "print" and "html".
    file_handle: An optional open file-like object to output to. If not
        specified, a suitable default will be used.
  """
  logging.info('Outputting results in format %s', output_format)
  stale_str_dict = _ConvertTestExpectationMapToStringDict(stale_dict)
  semi_stale_str_dict = _ConvertTestExpectationMapToStringDict(semi_stale_dict)
  active_str_dict = _ConvertTestExpectationMapToStringDict(active_dict)
  unmatched_results_str_dict = _ConvertUnmatchedResultsToStringDict(
      unmatched_results)
  unused_expectations_str_list = _ConvertUnusedExpectationsToStringList(
      unused_expectations)

  if output_format == 'print':
    file_handle = file_handle or sys.stdout
    if stale_dict:
      file_handle.write(SECTION_STALE + '\n')
      _RecursivePrintToFile(stale_str_dict, 0, file_handle)
    if semi_stale_dict:
      file_handle.write(SECTION_SEMI_STALE + '\n')
      _RecursivePrintToFile(semi_stale_str_dict, 0, file_handle)
    if active_dict:
      file_handle.write(SECTION_ACTIVE + '\n')
      _RecursivePrintToFile(active_str_dict, 0, file_handle)

    if unused_expectations_str_list:
      file_handle.write('\n' + SECTION_UNUSED + '\n')
      _RecursivePrintToFile(unused_expectations_str_list, 0, file_handle)
    if unmatched_results_str_dict:
      file_handle.write('\n' + SECTION_UNMATCHED + '\n')
      _RecursivePrintToFile(unmatched_results_str_dict, 0, file_handle)

  elif output_format == 'html':
    should_close_file = False
    if not file_handle:
      should_close_file = True
      file_handle = tempfile.NamedTemporaryFile(delete=False, suffix='.html')

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
      file_handle.write('\n<h1>' + SECTION_UNUSED + "</h1>\n")
      _RecursiveHtmlToFile(unused_expectations_str_list, file_handle)
    if unmatched_results_str_dict:
      file_handle.write('\n<h1>' + SECTION_UNMATCHED + '</h1>\n')
      _RecursiveHtmlToFile(unmatched_results_str_dict, file_handle)

    file_handle.write(HTML_FOOTER)
    if should_close_file:
      file_handle.close()
    print 'Results available at file://%s' % file_handle.name
  else:
    raise RuntimeError('Unsupported output format %s' % output_format)


def _RecursivePrintToFile(element, depth, file_handle):
  """Recursively prints |element| as text to |file_handle|.

  Args:
    element: A dict, list, or str/unicode to output.
    depth: The current depth of the recursion as an int.
    file_handle: An open file-like object to output to.
  """
  if element is None:
    element = str(element)
  if isinstance(element, str) or isinstance(element, unicode):
    file_handle.write(('  ' * depth) + element + '\n')
  elif isinstance(element, dict):
    for k, v in element.iteritems():
      _RecursivePrintToFile(k, depth, file_handle)
      _RecursivePrintToFile(v, depth + 1, file_handle)
  elif isinstance(element, list):
    for i in element:
      _RecursivePrintToFile(i, depth, file_handle)
  else:
    raise RuntimeError('Given unhandled type %s' % type(element))


def _RecursiveHtmlToFile(element, file_handle):
  """Recursively outputs |element| as HTMl to |file_handle|.

  Iterables will be output as a collapsible section containing any of the
  iterable's contents.

  Any link-like text will be turned into anchor tags.

  Args:
    element: A dict, list, or str/unicode to output.
    file_handle: An open file-like object to output to.
  """
  if isinstance(element, str) or isinstance(element, unicode):
    file_handle.write('<p>%s</p>\n' % _LinkifyString(element))
  elif isinstance(element, dict):
    for k, v in element.iteritems():
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


def _LinkifyString(s):
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


def _ConvertTestExpectationMapToStringDict(test_expectation_map):
  """Converts |test_expectation_map| to a dict of strings for reporting.

  Args:
    test_expectation_map: A dict in the format output by
        expectations.CreateTestExpectationMap()

  Returns:
    A string dictionary representation of |test_expectation_map| in the
    following format:
    {
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
  """
  output_dict = {}
  for test_name, expectation_map in test_expectation_map.iteritems():
    output_dict[test_name] = {}

    for expectation, builder_map in expectation_map.iteritems():
      expectation_str = _FormatExpectation(expectation)
      output_dict[test_name][expectation_str] = {}

      for builder_name, step_map in builder_map.iteritems():
        output_dict[test_name][expectation_str][builder_name] = {}
        fully_passed = []
        partially_passed = {}
        never_passed = []

        for step_name, stats in step_map.iteritems():
          if stats.passed_builds == stats.total_builds:
            fully_passed.append(_AddStatsToStr(step_name, stats))
          elif stats.failed_builds == stats.total_builds:
            never_passed.append(_AddStatsToStr(step_name, stats))
          else:
            assert step_name not in partially_passed
            partially_passed[step_name] = stats

        output_builder_map =\
            output_dict[test_name][expectation_str][builder_name]
        if fully_passed:
          output_builder_map[FULL_PASS] = fully_passed
        if partially_passed:
          output_builder_map[PARTIAL_PASS] = {}
          for step_name, stats in partially_passed.iteritems():
            s = _AddStatsToStr(step_name, stats)
            output_builder_map[PARTIAL_PASS][s] = list(stats.failure_links)
        if never_passed:
          output_builder_map[NEVER_PASS] = never_passed
  return output_dict


def _ConvertUnmatchedResultsToStringDict(unmatched_results):
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
  for builder, results in unmatched_results.iteritems():
    for r in results:
      builder_map = output_dict.setdefault(r.test, {})
      step_map = builder_map.setdefault(builder, {})
      result_str = 'Got "%s" on %s with tags [%s]' % (
          r.actual_result, data_types.BuildLinkFromBuildId(
              r.build_id), ' '.join(r.tags))
      step_map.setdefault(r.step, []).append(result_str)
  return output_dict


def _ConvertUnusedExpectationsToStringList(unused_expectations):
  """Converts |unused_expectations| to a list of strings for reporting.

  Args:
    unused_expectations: A list of data_types.Expectation that didn't have any
        matching results.

  Returns:
    A list of strings, each one corresponding to an element in
    |unused_expectations|. Strings are in a format similar to what would be
    present as a line in an expectation file.
  """
  output_list = []
  for e in unused_expectations:
    output_list.append('[ %s ] %s [ %s ]' %
                       (' '.join(e.tags), e.test, ' '.join(e.expected_results)))
  return output_list


def _FormatExpectation(expectation):
  return '"%s" expectation on "%s"' % (' '.join(
      expectation.expected_results), ' '.join(expectation.tags))


def _AddStatsToStr(s, stats):
  return '%s (%d/%d)' % (s, stats.passed_builds, stats.total_builds)


def OutputRemovedUrls(removed_urls):
  """Outputs URLs of removed expectations for easier consumption by the user.

  Outputs both a string suitable for passing to Chrome via the command line to
  open all bugs in the browser and a string suitable for copying into the CL
  description to associate the CL with all the affected bugs.

  Args:
    removed_urls: A set or list of strings containing bug URLs.
  """
  removed_urls = list(removed_urls)
  removed_urls.sort()
  _OutputUrlsForCommandLine(removed_urls)
  _OutputUrlsForClDescription(removed_urls)


def _OutputUrlsForCommandLine(urls, file_handle=None):
  """Outputs |urls| for opening in a browser.

  The output string is meant to be passed to a browser via the command line in
  order to open all URLs in that browser, e.g.

  `google-chrome https://crbug.com/1234 https://crbug.com/2345`

  Args:
    urls: A list of strings containing URLs to output.
    file_handle: A file handle to write the string to. Defaults to stdout.
  """
  file_handle = file_handle or sys.stdout

  def _StartsWithHttp(url):
    return url.startswith('https://') or url.startswith('http://')

  urls = [u if _StartsWithHttp(u) else 'https://%s' % u for u in urls]
  file_handle.write('Affected bugs: %s\n' % ' '.join(urls))


def _OutputUrlsForClDescription(urls, file_handle=None):
  """Outputs |urls| for use in a CL description.

  Output adheres to the line length recommendation and max number of bugs per
  line supported in Gerrit.

  Args:
    urls: A list of strings containing URLs to output.
    file_handle: A file handle to write the string to. Defaults to stdout.
  """
  file_handle = file_handle or sys.stdout
  urls = collections.deque(urls)

  output_str = ''
  current_line = ''
  bugs_on_line = 0
  while len(urls):
    current_bug = urls.popleft()
    current_bug = current_bug.split('crbug.com/', 1)[1]
    # Handles cases like crbug.com/angleproject/1234.
    current_bug = current_bug.replace('/', ':')

    # First bug on the line.
    if not current_line:
      current_line = 'Bug: %s' % current_bug
    # Bug or length limit hit for line.
    elif (len(current_line) + len(current_bug) + 2 > MAX_CHARACTERS_PER_CL_LINE
          or bugs_on_line >= MAX_BUGS_PER_LINE):
      output_str += current_line + '\n'
      bugs_on_line = 0
      current_line = 'Bug: %s' % current_bug
    # Can add to current line.
    else:
      current_line += ', %s' % current_bug

    bugs_on_line += 1

  output_str += current_line + '\n'
  file_handle.write('Affected bugs:\n%s' % output_str)
