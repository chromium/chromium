# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for outputting results in a human-readable format."""

import tempfile
from typing import Dict, IO, List, Optional, Union

from flake_suppressor_common import common_typing as ct

UrlListType = List[str]
StringTagsToUrlsType = Dict[str, UrlListType]
TestToStringTagsType = Dict[str, StringTagsToUrlsType]
StringMapType = Dict[str, TestToStringTagsType]

TestToUrlListType = Dict[str, UrlListType]
SuiteToTestsType = Dict[str, TestToUrlListType]
ConfigGroupedStringMapType = Dict[str, SuiteToTestsType]

NodeType = Union[UrlListType, StringTagsToUrlsType, TestToStringTagsType,
                 StringMapType, TestToUrlListType, SuiteToTestsType,
                 ConfigGroupedStringMapType]


def GenerateHtmlOutputFile(aggregated_results: ct.AggregatedResultsType,
                           outfile: Optional[IO] = None) -> None:
  """Generates an HTML results file.

  Args:
    aggregated_results: A map containing the aggregated test results.
    outfile: A file-like object to output to. Will create one if not provided.
  """
  outfile = outfile or tempfile.NamedTemporaryFile(
      mode='w', delete=False, suffix='.html')
  try:
    outfile.write('<html>\n<body>\n')
    string_map = _ConvertAggregatedResultsToStringMap(aggregated_results)
    _OutputMapToHtmlFile(string_map, 'Grouped By Test', outfile)
    config_map = _ConvertFromTestGroupingToConfigGrouping(string_map)
    _OutputMapToHtmlFile(config_map, 'Grouped By Config', outfile)
    outfile.write('</body>\n</html>\n')
  finally:
    outfile.close()
  print('HTML results: %s' % outfile.name)


def _OutputMapToHtmlFile(string_map: StringMapType, result_header: str,
                         output_file: IO) -> None:
  """Outputs a map to a file as a nested list.

  Args:
    string_map: The string map to output.
    result_header: A string containing the header contents placed before the
        nested list.
    output_file: A file-like object to output the map to.
  """
  output_file.write('<h1>%s</h1>\n' % result_header)
  output_file.write('<ul>\n')
  _RecursiveHtmlToFile(string_map, output_file)
  output_file.write('</ul>\n')


def _RecursiveHtmlToFile(node: NodeType, output_file: IO) -> None:
  """Recursively outputs a string map to an output file as HTML.

  Specifically, contents are output as an unordered list (<ul>).

  Args:
    node: The current node to output. Must be either a dict or list.
    output_file: A file-like object to output the HTML to.
  """
  if isinstance(node, dict):
    for key, value in node.items():
      output_file.write('<li>%s</li>\n' % key)
      output_file.write('<ul>\n')
      _RecursiveHtmlToFile(value, output_file)
      output_file.write('</ul>\n')
  elif isinstance(node, list):
    for element in node:
      output_file.write('<li><a href="%s">%s</a></li>\n' % (element, element))
  else:
    raise RuntimeError('Unsupported type %s' % type(node).__name__)


def _ConvertAggregatedResultsToStringMap(
    aggregated_results: ct.AggregatedResultsType) -> StringMapType:
  """Converts aggregated results to a format usable by _RecursiveHtmlToFile.

  Specifically, updates the string representation of the typ tags and replaces
  the lowest level dict with the build URL list.

  Args:
    aggregated_results: A map containing the aggregated test results.

  Returns:
    A map in the format:
    {
      'suite': {
        'test': {
          'space separated typ tags': ['build', 'url', 'list']
        }
      }
    }
  """
  string_map = {}
  for suite, test_map in aggregated_results.items():
    for test, tag_map in test_map.items():
      for typ_tags, build_url_list in tag_map.items():
        str_typ_tags = ' '.join(typ_tags)
        string_map.setdefault(suite,
                              {}).setdefault(test,
                                             {})[str_typ_tags] = build_url_list
  return string_map


def _ConvertFromTestGroupingToConfigGrouping(string_map: StringMapType
                                             ) -> ConfigGroupedStringMapType:
  """Converts |string| map to be grouped by typ tags/configuration.

  Args:
    string_map: The output of _ConvertAggregatedResultsToStringMap.

  Returns:
    A map in the format:
    {
      'space separated typ tags': {
        'suite': {
          'test': ['build', 'url', 'list']
        }
      }
    }
  """
  converted_map = {}
  for suite, test_map in string_map.items():
    for test, tag_map in test_map.items():
      for typ_tags, build_urls in tag_map.items():
        converted_map.setdefault(typ_tags, {}).setdefault(suite,
                                                          {})[test] = build_urls
  return converted_map
