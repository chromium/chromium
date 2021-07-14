# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for working with BigQuery results."""


def AggregateResults(results):
  """Aggregates BigQuery results.

  Args:
    results: Parsed JSON results from a BigQuery query.

  Returns:
    A map in the following format:
    {
      'test_suite': {
        'test_name': {
          'typ_tags_as_string': {
            'typ_tags': [ 'list', 'of', 'tags ],
            'build_url_list': [ 'list', 'of', 'urls' ],
          },
        },
      },
    }
  """
  aggregated_results = {}
  for r in results:
    _, suite, __, test_name = r['name'].split('.', 3)
    build_id = r['id'].split('-')[-1]
    build_url = 'http://ci.chromium.org/b/%s' % build_id
    typ_tags = r['typ_tags']
    typ_tags.sort()
    typ_tags_str = '__'.join(typ_tags)

    unique_test = aggregated_results.setdefault(suite, {}).setdefault(
        test_name, {}).setdefault(typ_tags_str, {})
    build_url_list = unique_test.setdefault('build_url_list', [])
    build_url_list.append(build_url)
    unique_test.setdefault('typ_tags', typ_tags)
  return aggregated_results
