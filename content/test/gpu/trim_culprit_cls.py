#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Script for determining which CLs in a blamelist ran on a certain trybot.

There are cases where CLs can be absolved of a CI failure if they ran on a
similar trybot before being submitted. This CL will go through each CL in a
given blamelist and determine whether they ran on a specified trybot or not.

This script depends on the `bq` tool, which is available as part of the Google
Cloud SDK https://cloud.google.com/sdk/docs/quickstarts.

Example usage:

trim_culprit_cls.py \
  --start-revision <first/oldest revision in the blamelist> \
  --end-revision <last/newest revision in the blamelist> \
  --trybot <optional trybot name> \
  --project <billing project>

Concrete example:

trim_culprit_cls.py \
  --start-revision 1cdf916d194215f1e4139f295e494fc1c1863c3c \
  --end-revision 9aa31419100be8d0f02708a500aaed7c33a53a10 \
  --trybot win_optional_gpu_tests_rel \
  --project chromium-swarm

The --project argument can be any project you are associated with in the
Google Cloud console https://console.cloud.google.com/ (see drop-down menu in
the top left corner).
"""

from __future__ import print_function

import argparse
import json
import re
import subprocess

# pylint: disable=line-too-long
# Schemas:
# - go/buildbucket-bq and go/buildbucket-proto/build.proto
# - go/luci/cq/bq and
#   https://source.chromium.org/chromium/infra/infra/+/main:go/src/go.chromium.org/luci/cv/api/bigquery/v1/attempt.proto
#
# Original author: maruel@
QUERY_TEMPLATE = """\
WITH cq_builds AS (
  SELECT
    build.id,
    build.critical,
    start_time,
    TIMESTAMP_DIFF(end_time, start_time, SECOND) AS duration,
    cl.change,
    cl.patchset
  FROM `commit-queue.chromium.attempts` CROSS JOIN UNNEST(builds) AS build CROSS JOIN UNNEST(gerrit_changes) AS cl
  WHERE
    cl.host = 'chromium-review.googlesource.com'
    AND cl.project = 'chromium/src'
    AND cl.change = {cl_number}
),

builds AS (
  SELECT
    patchset,
    bb.builder.project||'/'||bb.builder.bucket||'/'||bb.builder.builder AS builder,
    'ci.chromium.org/b/'||bb.id AS url,
    cq.critical,
    bb.status,
    cq.start_time,
    duration
  FROM cq_builds AS cq INNER JOIN `cr-buildbucket.chromium.builds` AS bb ON cq.id = bb.id
  WHERE
    # Performance optimization.
    bb.create_time >= TIMESTAMP_SUB(CURRENT_TIMESTAMP(), INTERVAL 30 DAY)
)

SELECT * FROM builds ORDER BY patchset DESC, critical, builder, start_time
"""
# pylint: enable=line-too-long

GERRIT_URL_REGEX = re.compile(r'^\s*Reviewed-on: (?P<gerrit_url>.*)$',
                              re.MULTILINE)


class ChangeList():
  """Class for storing relevant information for a CL."""

  def __init__(self):
    self.revision = None
    self.gerrit_url = None
    self._cl_number = None
    self.largest_patchset = None
    self.ran_trybot = None

  @property
  def cl_number(self):
    assert self.gerrit_url
    if not self._cl_number:
      self._cl_number = self.gerrit_url.split('/')[-1]
    return self._cl_number

  def __str__(self):
    assert self.revision is not None
    assert self.gerrit_url is not None
    assert self.largest_patchset is not None
    assert self.ran_trybot is not None
    s = '%s (%s)' % (self.revision, self.gerrit_url)
    if not self.ran_trybot:
      s += ' <<<< Did not run trybot'
    return s


def QueryTrybotsForCl(cl_number, project):
  """Queries BigQuery for the tryjobs run for a CL.

  Args:
    cl_number: An int or string containing the CL number to query.
    project: A string containing the billing project to use for queries.

  Returns:
    A list of dicts, each entry containing data for one trybot run.
  """
  query = QUERY_TEMPLATE.format(cl_number=cl_number)

  cmd = [
      'bq',
      'query',
      '--format=json',
      '--project_id=%s' % project,
      '--max_rows=500',
      '--use_legacy_sql=false',
      query,
  ]
  with open('/dev/null', 'w') as devnull:
    stdout = subprocess.check_output(cmd, stderr=devnull)
  return json.loads(stdout)


def FillTrybotRuns(blamelist, trybot, project):
  """Fills the trybot data for the entries in |blamelist|

  Args:
    blamelist: A list of ChangeList objects with their gerrit_url fields filled.
    trybot: A string containing the name of the trybot to check for.
    project: A string containing the billing project to use for queries.
  """
  total_cls = len(blamelist)
  for i, entry in enumerate(blamelist):
    print('Getting data for CL %s/%s' % (i + 1, total_cls))
    largest_patchset = 0
    all_trybots = QueryTrybotsForCl(entry.cl_number, project)
    assert all_trybots
    # Query orders results by patchset, ensuring that we get relevant results
    # even if the number of tryjobs exceeds the row limit, but loading the JSON
    # into a dict doesn't preserve ordering, so find the largest patchset now.
    for tryjob in all_trybots:
      patchset = int(tryjob['patchset'])
      if patchset > largest_patchset:
        largest_patchset = patchset
    entry.largest_patchset = largest_patchset

    for tryjob in all_trybots:
      if largest_patchset != int(tryjob['patchset']):
        continue
      # 'builder' field is in the form project/bucket/builder, e.g.
      # chromium/try/android-marshmallow-arm64-rel
      if trybot == tryjob['builder'].split('/')[-1]:
        entry.ran_trybot = True
        break
    if entry.ran_trybot is None:
      entry.ran_trybot = False


def FillGerritUrls(blamelist):
  """Fills the Gerrit URLs for the entries in |blamelist|

  Args:
    blamelist: A list of ChangeList objects with their revision fields filled.
  """
  cmd_template = [
      'git',
      'show',
      '--name-only',
  ]
  for entry in blamelist:
    assert entry.revision
    stdout = subprocess.check_output(cmd_template + [entry.revision],
                                     stderr=subprocess.STDOUT)
    match = GERRIT_URL_REGEX.search(stdout)
    assert match
    entry.gerrit_url = match.groupdict()['gerrit_url']
    assert entry.gerrit_url


def GetBlamelist(start_revision, end_revision):
  """Gets a revision blamelist between the two given revisions.

  Args:
    start_revision: A string containing the earliest revision in the blamelist.
    end_revision: A string containing the latest revision in the blamelist.

  Returns:
    A list of ChangeList objects with their revision fields filled in, each
    corresponding to a revision in the blamelist. The first entry is the
    latest in the blamelist.
  """
  cmd = [
      'git',
      'log',
      '--pretty=oneline',
      '%s~1..%s' % (start_revision, end_revision),
  ]
  stdout = subprocess.check_output(cmd, stderr=subprocess.STDOUT)

  blamelist = []
  for line in stdout.splitlines():
    cl = ChangeList()
    cl.revision = line.split()[0]
    blamelist.append(cl)
  return blamelist


def ParseArgs():
  parser = argparse.ArgumentParser(
      description='Script to determine which CLs in a blamelist did not run a '
      'particular trybot.')
  parser.add_argument('--start-revision',
                      required=True,
                      help='The earliest revision in the blamelist.')
  parser.add_argument('--end-revision',
                      required=True,
                      help='The latest revision in the blamelist.')
  parser.add_argument('--project',
                      required=True,
                      help='A billing project to use for queries.')
  parser.add_argument('--trybot',
                      required=True,
                      help='The name of the trybot to look for.')
  return parser.parse_args()


def main():
  args = ParseArgs()
  blamelist = GetBlamelist(args.start_revision, args.end_revision)
  FillGerritUrls(blamelist)
  FillTrybotRuns(blamelist, args.trybot, args.project)
  print('\n\nBlamelist (latest first):\n')
  for entry in blamelist:
    print(entry)


if __name__ == '__main__':
  main()
