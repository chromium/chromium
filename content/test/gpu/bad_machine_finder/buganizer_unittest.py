# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import typing
from typing import Union
import unittest
from unittest import mock

from bad_machine_finder import buganizer
from bad_machine_finder import detection

from blinkpy.w3c import buganizer as blink_buganizer

# pylint: disable=protected-access


class FakeBuganizerClient:

  def __init__(self):
    # GetIssueComments
    self.issue_comments = []

    # NewComment
    self.comment_content = None

  def GetIssueComments(self, bug_id: int) -> Union[dict, list]:
    del bug_id  # unused.
    return self.issue_comments

  def NewComment(self, bug_id: int, comment: str, use_markdown: bool) -> None:
    del bug_id, use_markdown  # unused.
    self.comment_content = comment


def _GetIsoFormatStringForNDaysAgo(num_days: int) -> str:
  now = datetime.datetime.now()
  n_days_ago = now - datetime.timedelta(days=num_days)
  return n_days_ago.isoformat()


class UpdateBugUnittest(unittest.TestCase):

  def testBasic(self):
    """Tests the basic behavior of posting an update to a bug."""
    client = FakeBuganizerClient()
    client.issue_comments = [
        {
            'timestamp':
            _GetIsoFormatStringForNDaysAgo(1),
            'comment':
            '\n'.join([
                buganizer._AUTOMATED_COMMENT_START,
                'bot-2',
                'bot-3',
            ]),
        },
    ]

    first_machine_list = detection.BadMachineList()
    first_machine_list.AddBadMachine('bot-1', 'reason-1a')
    first_machine_list.AddBadMachine('bot-1', 'reason-1b')
    first_machine_list.AddBadMachine('bot-2', 'reason-2')

    second_machine_list = detection.BadMachineList()
    second_machine_list.AddBadMachine('bot-3', 'reason-3')
    second_machine_list.AddBadMachine('bot-4', 'reason-4')
    second_machine_list.AddBadMachine('bot-5', 'reason-5')

    mgbm = detection.MixinGroupedBadMachines()
    mgbm.AddMixinData('mixin-a', first_machine_list)
    mgbm.AddMixinData('mixin-b', second_machine_list)

    with mock.patch.object(buganizer,
                           '_GetBuganizerClient',
                           return_value=client):
      buganizer.UpdateBug(1234, mgbm, 7)

    expected_markdown = f"""\
{buganizer._AUTOMATED_COMMENT_START}

Bad machines for mixin-a
  * bot-1
    * reason-1a
    * reason-1b

Bad machines for mixin-b
  * bot-4
    * reason-4
  * bot-5
    * reason-5"""
    self.assertEqual(client.comment_content, expected_markdown)

  def testNoNewBadMachines(self):
    """Tests behavior when all bad machines were recently reported."""
    client = FakeBuganizerClient()
    client.issue_comments = [
        {
            'timestamp':
            _GetIsoFormatStringForNDaysAgo(1),
            'comment':
            '\n'.join([
                buganizer._AUTOMATED_COMMENT_START,
                'bot-1'
                'bot-2',
                'bot-3',
                'bot-4',
                'bot-5',
            ]),
        },
    ]

    first_machine_list = detection.BadMachineList()
    first_machine_list.AddBadMachine('bot-1', 'reason-1a')
    first_machine_list.AddBadMachine('bot-1', 'reason-1b')
    first_machine_list.AddBadMachine('bot-2', 'reason-2')

    second_machine_list = detection.BadMachineList()
    second_machine_list.AddBadMachine('bot-3', 'reason-3')
    second_machine_list.AddBadMachine('bot-4', 'reason-4')
    second_machine_list.AddBadMachine('bot-5', 'reason-5')

    mgbm = detection.MixinGroupedBadMachines()
    mgbm.AddMixinData('mixin-a', first_machine_list)
    mgbm.AddMixinData('mixin-b', second_machine_list)

    with mock.patch.object(buganizer,
                           '_GetBuganizerClient',
                           return_value=client):
      buganizer.UpdateBug(1234, mgbm, 7)

    expected_markdown = f"""\
{buganizer._AUTOMATED_COMMENT_START}

No new bad machines detected"""
    self.assertEqual(client.comment_content, expected_markdown)


class GetRecentlyReportedBotsUnittest(unittest.TestCase):

  def testBugNotAccessible(self):
    """Tests behavior when accessing the bug returns an error."""
    client = typing.cast(blink_buganizer.BuganizerClient, FakeBuganizerClient())
    client.issue_comments = {'error': 'error_message'}

    with self.assertRaisesRegex(
        buganizer.BugNotAccessibleException,
        'Failed to get comments from 1234: error_message'):
      buganizer._GetRecentlyReportedBots(1234, client, [], 7)

  def testBasic(self):
    """Tests the basic behavior of finding recently reported bots."""
    client = typing.cast(blink_buganizer.BuganizerClient, FakeBuganizerClient())
    client.issue_comments = [
        # Should be ignored because it's not an automated comment.
        {
            'timestamp': _GetIsoFormatStringForNDaysAgo(1),
            'comment': '\n'.join([
                'bot-1',
                'bot-2',
                'bot-3',
                'bot-4',
            ]),
        },
        # Should be ignored because it's too old.
        {
            'timestamp':
            _GetIsoFormatStringForNDaysAgo(14),
            'comment':
            '\n'.join([
                buganizer._AUTOMATED_COMMENT_START,
                'bot-1',
                'bot-2',
                'bot-3',
                'bot-4',
            ]),
        },
        # Should be parsed.
        {
            'timestamp':
            _GetIsoFormatStringForNDaysAgo(1),
            'comment':
            '\n'.join([
                buganizer._AUTOMATED_COMMENT_START,
                'bot-2',
                'bot-3',
                'bot-5',
            ]),
        },
    ]

    recently_reported_bots = buganizer._GetRecentlyReportedBots(
        1234, client, {'bot-1', 'bot-2', 'bot-3', 'bot-4'}, 7)
    self.assertEqual(recently_reported_bots, {'bot-2', 'bot-3'})

  def testIsoZCompatibility(self):
    """Tests that a trailing Z in an ISO 8601 string does not cause issues."""
    client = typing.cast(blink_buganizer.BuganizerClient, FakeBuganizerClient())
    client.issue_comments = [
        {
            'timestamp':
            _GetIsoFormatStringForNDaysAgo(1) + 'Z',
            'comment':
            '\n'.join([
                buganizer._AUTOMATED_COMMENT_START,
                'bot-2',
                'bot-3',
                'bot-5',
            ]),
        },
    ]

    recently_reported_bots = buganizer._GetRecentlyReportedBots(
        1234, client, {'bot-1', 'bot-2', 'bot-3', 'bot-4'}, 7)
    self.assertEqual(recently_reported_bots, {'bot-2', 'bot-3'})
