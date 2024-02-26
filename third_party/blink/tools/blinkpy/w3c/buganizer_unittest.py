# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import textwrap
import unittest

from blinkpy.w3c.buganizer import BuganizerIssue, Status, Priority, Severity


class BuganizerIssueTest(unittest.TestCase):

    def test_display(self):
        issue = BuganizerIssue(title='test',
                               description='ABC~‾¥≈¤･・•∙·☼★星🌟星★☼·∙•・･¤≈¥‾~XYZ',
                               component_id='999',
                               issue_id=12345,
                               cc=['foo@chromium.org', 'bar@chromium.org'],
                               status=Status.ASSIGNED,
                               priority=Priority.P0,
                               severity=Severity.S0)
        self.assertEqual(
            str(issue),
            textwrap.dedent("""\
                Issue https://crbug.com/12345: test
                  Status: ASSIGNED
                  Component ID: 999
                  CC: foo@chromium.org, bar@chromium.org
                  Priority: P0
                  Severity: S0
                  Description:
                    ABC~‾¥≈¤･・•∙·☼★星🌟星★☼·∙•・･¤≈¥‾~XYZ
                  """))

    def test_display_minimal(self):
        issue = BuganizerIssue(title='test',
                               description=textwrap.dedent("""\
                                  line 1
                                    line 2
                                  line 3
                                  """),
                               component_id='999')
        self.assertEqual(
            str(issue),
            textwrap.dedent("""\
                Issue: test
                  Status: NEW
                  Component ID: 999
                  CC: (none)
                  Priority: P3
                  Severity: S4
                  Description:
                    line 1
                      line 2
                    line 3
                """))

    def test_crbug_link(self):
        issue = BuganizerIssue(title='test',
                               description='test',
                               component_id='999',
                               issue_id=12345)
        self.assertEqual(issue.link, 'https://crbug.com/12345')
        issue = BuganizerIssue(title='test',
                               description='test',
                               component_id='999')
        self.assertIsNone(issue.link)
