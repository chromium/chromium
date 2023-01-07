# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.w3c.monorail import MonorailAPI, MonorailIssue


class MonorailIssueTest(unittest.TestCase):
    def test_init_succeeds(self):
        # Minimum example.
        MonorailIssue('chromium', summary='test', status='Untriaged')
        # All fields.
        MonorailIssue(
            'chromium',
            summary='test',
            status='Untriaged',
            description='body',
            cc=['foo@chromium.org'],
            labels=['Flaky'],
            components=['Infra'])

    def test_init_fills_project_id(self):
        issue = MonorailIssue('chromium', summary='test', status='Untriaged')
        self.assertEqual(issue.body['projectId'], 'chromium')

    def test_unicode(self):
        issue = MonorailIssue(
            'chromium',
            summary=u'test',
            status='Untriaged',
            description='ABC~‾¥≈¤･・•∙·☼★星🌟星★☼·∙•・･¤≈¥‾~XYZ',
            cc=['foo@chromium.org', 'bar@chromium.org'],
            labels=['Flaky'],
            components=['Infra'])
        self.assertEqual(type(str(issue)), str)
        self.assertEqual(
            str(issue),
            ('Monorail issue in project chromium\n'
             'Summary: test\n'
             'Status: Untriaged\n'
             'CC: foo@chromium.org, bar@chromium.org\n'
             'Components: Infra\n'
             'Labels: Flaky\n'
             'Description:\nABC~‾¥≈¤･・•∙·☼★星🌟星★☼·∙•・･¤≈¥‾~XYZ\n'))

    def test_init_unknown_fields(self):
        with self.assertRaises(AssertionError):
            MonorailIssue('chromium', component='foo')

    def test_init_missing_required_fields(self):
        with self.assertRaises(AssertionError):
            MonorailIssue('', summary='test', status='Untriaged')
        with self.assertRaises(AssertionError):
            MonorailIssue('chromium', summary='', status='Untriaged')
        with self.assertRaises(AssertionError):
            MonorailIssue('chromium', summary='test', status='')

    def test_init_unknown_status(self):
        with self.assertRaises(AssertionError):
            MonorailIssue('chromium', summary='test', status='unknown')

    def test_init_string_passed_for_list_fields(self):
        with self.assertRaises(AssertionError):
            MonorailIssue(
                'chromium',
                summary='test',
                status='Untriaged',
                cc='foo@chromium.org')
        with self.assertRaises(AssertionError):
            MonorailIssue(
                'chromium',
                summary='test',
                status='Untriaged',
                components='Infra')
        with self.assertRaises(AssertionError):
            MonorailIssue(
                'chromium', summary='test', status='Untriaged', labels='Flaky')

    def test_new_chromium_issue(self):
        issue = MonorailIssue.new_chromium_issue('test',
                                                 description='body',
                                                 cc=['foo@chromium.org'],
                                                 components=['Infra'],
                                                 labels=['Test-WebTest'])
        self.assertEqual(issue.project_id, 'chromium')
        self.assertEqual(issue.body['summary'], 'test')
        self.assertEqual(issue.body['description'], 'body')
        self.assertEqual(issue.body['cc'], ['foo@chromium.org'])
        self.assertEqual(issue.body['components'], ['Infra'])
        self.assertEqual(issue.body['labels'],
                         ['Pri-3', 'Type-Bug', 'Test-WebTest'])

    def test_crbug_link(self):
        self.assertEqual(
            MonorailIssue.crbug_link(12345), 'https://crbug.com/12345')


class MonorailAPITest(unittest.TestCase):
    def test_fix_cc_field_in_body(self):
        original_body = {
            'summary': 'test bug',
            'cc': ['foo@chromium.org', 'bar@chromium.org']
        }
        # pylint: disable=protected-access
        self.assertEqual(
            MonorailAPI._fix_cc_in_body(original_body), {
                'summary': 'test bug',
                'cc': [{
                    'name': 'foo@chromium.org'
                }, {
                    'name': 'bar@chromium.org'
                }]
            })
