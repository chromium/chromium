# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for interacting with Buganizer."""

import datetime
from typing import Iterable, Set

from bad_machine_finder import detection

from blinkpy.w3c import buganizer

_AUTOMATED_COMMENT_START = 'Automated Report Of Bad Machines'


class BuganizerException(Exception):
  """A general exception for Buganizer-related errors."""


class ClientNotAvailableException(BuganizerException):
  """Indicates that a Buganzier client could not be created."""


class BugNotAccessibleException(BuganizerException):
  """Indicates that the specified bug could not be accessed."""


def UpdateBug(bug_id: int,
              mixin_grouped_bad_machines: detection.MixinGroupedBadMachines,
              grace_period: int) -> None:
  """Updates the given |bug_id| with bad machine results.

  Will automatically omit bad machines that have previously been reported in the
  past |grace_period| days.

  Args:
    bug_id: The Buganizer bug ID to update.
    mixin_groupd_bad_machines: A MixinGroupedBadMachines object containing all
        of the bad machine data to use when updating the bug.
    grace_period: The minimum number of days between when a bad machine was
        reported and when it can be reported again.
  """
  client = _GetBuganizerClient()

  bad_machine_names = mixin_grouped_bad_machines.GetAllBadMachineNames()
  recently_reported_bots = _GetRecentlyReportedBots(bug_id, client,
                                                    bad_machine_names,
                                                    grace_period)

  markdown_components = [
      _AUTOMATED_COMMENT_START,
  ]
  mixin_report_markdown = mixin_grouped_bad_machines.GenerateMarkdown(
      bots_to_skip=recently_reported_bots)
  if mixin_report_markdown:
    markdown_components.append(mixin_report_markdown)
  else:
    markdown_components.append('No new bad machines detected')
  markdown_comment = '\n\n'.join(markdown_components)

  client.NewComment(bug_id, markdown_comment, use_markdown=True)


def _GetRecentlyReportedBots(bug_id: int, client: buganizer.BuganizerClient,
                             bad_machine_names: Iterable[str],
                             grace_period: int) -> Set[str]:
  """Retrieves the subset of |bad_machine_names| which were reported recently.

  Args:
    bug_id: The Buganizer bug ID to check.
    client: The BuganizerClient to use for interacting with Buganizer.
    bad_machine_names: All machine names that should be looked for within
        recent comments on the bug.
    grace_period: The minimum number of days since the last mention of a machine
        on the bug for it not to be considered recently reported.

  Returns:
    A set of machine names which were reported on |bug_id| within the past
    |grace_period| days. Guaranteed to be a subset of |bad_machine_names|.
  """
  comment_list = client.GetIssueComments(bug_id)
  # GetIssueComments currently returns a dict if something goes wrong instead of
  # raising an exception.
  # TODO(crbug.com/361602059): Switch to catching exceptions once those are
  # raised.
  if isinstance(comment_list, dict):
    raise BugNotAccessibleException(
        f'Failed to get comments from {bug_id}: '
        f'{comment_list.get("error", "error not provided")}')

  recent_comment_bodies = []
  for c in comment_list:
    comment_iso_timestamp = c['timestamp']
    # Z indicates the UTC timezone, but is not supported until Python 3.11.
    if comment_iso_timestamp.endswith(('z', 'Z')):
      comment_iso_timestamp = comment_iso_timestamp[:-1]
    comment_date = datetime.datetime.fromisoformat(comment_iso_timestamp)
    n_days_ago = (datetime.datetime.now(comment_date.tzinfo) -
                  datetime.timedelta(days=grace_period))
    if comment_date < n_days_ago:
      continue
    if _AUTOMATED_COMMENT_START not in c['comment']:
      continue
    recent_comment_bodies.append(c['comment'])

  recently_reported_bots = set()
  for bot_id in bad_machine_names:
    for cb in recent_comment_bodies:
      if bot_id in cb:
        recently_reported_bots.add(bot_id)
        break

  return recently_reported_bots


def _GetBuganizerClient() -> buganizer.BuganizerClient:
  try:
    return buganizer.BuganizerClient()
  except Exception as e:  # pylint: disable=broad-except
    raise ClientNotAvailableException(
        'Failed to create Buganizer client') from e
