# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.rpc import RESPONSE_PREFIX
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS, RELATIVE_WPT_TESTS
from blinkpy.common.system.executive_mock import mock_git_commands
from blinkpy.w3c.gerrit import GerritAPI, GerritCL
from blinkpy.w3c.gerrit_mock import MockGerritAPI


class GerritAPITest(unittest.TestCase):
    def test_post_wrong_url(self):
        host = MockHost()
        gerrit = GerritAPI(host, 'user', 'token')
        with self.assertRaises(AssertionError):
            gerrit.post('/changes/test~master~I100/abandon', None)

    def test_post_missing_auth(self):
        host = MockHost()
        gerrit = GerritAPI(host, '', '')
        with self.assertRaises(AssertionError):
            gerrit.post('/a/changes/test~master~I100/abandon', None)

    def test_query_cl(self):
        host = MockHost()
        url = ('https://chromium-review.googlesource.com/changes/chromium%2F'
               'src~main~I012345?o=CURRENT_FILES&o=CURRENT_REVISION'
               '&o=COMMIT_FOOTERS&o=DETAILED_ACCOUNTS')
        payload = {'change_id': 'I012345'}
        host.web.urls = {
            url: RESPONSE_PREFIX + b'\n' + json.dumps(payload).encode(),
        }
        gerrit = GerritAPI(host, 'user', 'token')
        cl = gerrit.query_cl('I012345')
        self.assertEqual(cl.change_id, 'I012345')

    def test_query_cl_comments_and_revisions(self):
        host = MockHost()
        url = ('https://chromium-review.googlesource.com/changes/chromium%2F'
               'src~main~I012345?o=MESSAGES&o=ALL_REVISIONS')
        payload = {'change_id': 'I012345'}
        host.web.urls = {
            url: RESPONSE_PREFIX + b'\n' + json.dumps(payload).encode(),
        }
        gerrit = GerritAPI(host, 'user', 'token')
        cl = gerrit.query_cl_comments_and_revisions('I012345')
        self.assertEqual(cl.change_id, 'I012345')

    def test_query_exportable_cls(self):
        host = MockHost()
        url = ('https://chromium-review.googlesource.com/changes/'
               '?q=project:"chromium%2Fsrc"+branch:main+is:submittable+-is:wip'
               '&n=200&o=CURRENT_FILES&o=CURRENT_REVISION&o=COMMIT_FOOTERS'
               '&o=DETAILED_ACCOUNTS')
        payload = []
        host.web.urls = {
            url: RESPONSE_PREFIX + b'\n' + json.dumps(payload).encode(),
        }
        gerrit = GerritAPI(host, 'user', 'token')
        cls = gerrit.query_exportable_cls()
        self.assertEqual(cls, [])


class GerritCLTest(unittest.TestCase):
    def test_url(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            '_number': 638250,
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertEqual(gerrit_cl.url,
                         'https://chromium-review.googlesource.com/638250')

    def test_current_revision_description(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'fake subject',
            '_number': 638250,
            'current_revision': '1',
            'revisions': {
                '1': {}
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertEqual(gerrit_cl.current_revision_description, '')

        data['revisions']['1']['description'] = 'patchset 1'
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertEqual(gerrit_cl.current_revision_description, 'patchset 1')

    def test_fetch_current_revision_commit(self):
        host = MockHost()
        host.executive = mock_git_commands(
            {
                'fetch': '',
                'rev-parse': '4de71d0ce799af441c1f106c5432c7fa7256be45',
                'footers': 'no-commit-position-yet'
            },
            strict=True)
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'fake subject',
            '_number': 638250,
            'current_revision': '1',
            'revisions': {
                '1': {
                    'fetch': {
                        'http': {
                            'url':
                            'https://chromium.googlesource.com/chromium/src',
                            'ref':
                            'refs/changes/50/638250/1'
                        }
                    }
                }
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        commit = gerrit_cl.fetch_current_revision_commit(host)

        self.assertEqual(commit.sha,
                         '4de71d0ce799af441c1f106c5432c7fa7256be45')
        self.assertEqual(host.executive.calls,
                         [[
                             'git', 'fetch',
                             'https://chromium.googlesource.com/chromium/src',
                             'refs/changes/50/638250/1'
                         ], ['git', 'rev-parse', 'FETCH_HEAD'],
                          [
                              'git', 'footers', '--position',
                              '4de71d0ce799af441c1f106c5432c7fa7256be45'
                          ]])

    def test_empty_cl_is_not_exportable(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'fake subject',
            '_number': 638250,
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        # It's important that this does not throw!
        self.assertFalse(gerrit_cl.is_exportable())

    def test_wpt_cl_is_exportable(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'fake subject',
            '_number': 638250,
            'current_revision': '1',
            'revisions': {
                '1': {
                    'commit_with_footers': 'fake subject',
                    'files': {
                        RELATIVE_WPT_TESTS + 'foo/bar.html': '',
                    }
                }
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertTrue(gerrit_cl.is_exportable())

    def test_no_wpt_cl_is_not_exportable(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'fake subject',
            '_number': 638250,
            'current_revision': '1',
            'revisions': {
                '1': {
                    'commit_with_footers': 'fake subject',
                    'files': {
                        RELATIVE_WEB_TESTS + 'foo/bar.html': '',
                    }
                }
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertFalse(gerrit_cl.is_exportable())

    def test_no_export_is_not_exportable(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'fake subject',
            '_number': 638250,
            'current_revision': '1',
            'revisions': {
                '1': {
                    'commit_with_footers': 'fake subject\nNo-Export: true',
                    'files': {
                        RELATIVE_WPT_TESTS + 'foo/bar.html': '',
                    }
                }
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertFalse(gerrit_cl.is_exportable())

    def test_legacy_noexport_is_not_exportable(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'fake subject',
            '_number': 638250,
            'current_revision': '1',
            'revisions': {
                '1': {
                    'commit_with_footers': 'fake subject\nNOEXPORT=true',
                    'files': {
                        RELATIVE_WPT_TESTS + 'foo/bar.html': '',
                    }
                }
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertFalse(gerrit_cl.is_exportable())

    def test_import_in_subject_is_exportable(self):
        data = {
            'change_id': 'Ib58c7125d85d2fd71af711ea8bbd2dc927ed02cb',
            'subject': 'Import something',
            '_number': 638250,
            'current_revision': '1',
            'revisions': {
                '1': {
                    'commit_with_footers': 'fake subject',
                    'files': {
                        RELATIVE_WPT_TESTS + 'foo/bar.html': '',
                    }
                }
            },
            'owner': {
                'email': 'test@chromium.org'
            },
        }
        gerrit_cl = GerritCL(data, MockGerritAPI())
        self.assertTrue(gerrit_cl.is_exportable())
