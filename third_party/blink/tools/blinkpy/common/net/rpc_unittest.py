# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.rpc import Build, BuildbucketClient, RPCError


class BuildbucketTest(unittest.TestCase):
    """Basic test of BB pagination and 'Batch' functionality.

    There are more extensive tests at higher abstraction layers (e.g., GitCL).
    """

    def setUp(self):
        self.host = MockHost()
        self.client = BuildbucketClient.from_host(
            self.host,
            hostname='cr-buildbucket.appspot.com',
            service='buildbucket.v2.Builds')

    def test_search_builds_one_page(self):
        predicate = {
            'builder': {
                'project': 'chromium',
                'bucket': 'try',
                'builder': 'linux-rel',
            },
        }
        self.host.web.append_prpc_response({
            'builds': [{
                'id': '123',
                'number': 123
            }, {
                'id': '456',
                'number': 456
            }],
            'nextPageToken':
            'id>789',
        })
        builds = self.client.search_builds(predicate, ['id', 'number'],
                                           count=1)
        (url, request_body), = self.host.web.requests
        self.assertEqual(
            url, 'https://cr-buildbucket.appspot.com'
            '/prpc/buildbucket.v2.Builds/SearchBuilds')
        self.assertEqual(
            json.loads(request_body), {
                'predicate': predicate,
                'pageSize': 1,
                'fields': 'builds.*.id,builds.*.number',
            })
        self.assertEqual(builds, [{'id': '123', 'number': 123}])

    def test_search_builds_follow_pages(self):
        predicate = {
            'builder': {
                'project': 'chromium',
                'bucket': 'try',
                'builder': 'linux-rel',
            },
        }
        self.host.web.append_prpc_response({
            'builds': [{
                'id': '123',
                'number': 123
            }, {
                'id': '456',
                'number': 456
            }],
            'nextPageToken':
            'id>789',
        })
        self.host.web.append_prpc_response({
            'builds': [{
                'id': '789',
                'number': 789
            }, {
                'id': '012',
                'number': 102
            }],
            'nextPageToken':
            'id>345',
        })
        build1, build2, build3 = self.client.search_builds(predicate,
                                                           ['id', 'number'],
                                                           count=3)
        (url1, request1), (url2, request2) = self.host.web.requests
        self.assertEqual(
            url1, 'https://cr-buildbucket.appspot.com'
            '/prpc/buildbucket.v2.Builds/SearchBuilds')
        self.assertEqual(url1, url2)
        self.assertEqual(
            json.loads(request1), {
                'predicate': predicate,
                'pageSize': 3,
                'fields': 'builds.*.id,builds.*.number',
            })
        self.assertEqual(
            json.loads(request2), {
                'predicate': predicate,
                'pageSize': 3,
                'fields': 'builds.*.id,builds.*.number',
                'pageToken': 'id>789',
            })
        self.assertEqual(build1, {'id': '123', 'number': 123})
        self.assertEqual(build2, {'id': '456', 'number': 456})
        self.assertEqual(build3, {'id': '789', 'number': 789})

    def test_search_builds_no_next_token(self):
        self.host.web.append_prpc_response(
            {'builds': [{
                'id': '123',
                'number': 123
            }]})
        builds = self.client.search_builds({}, ['id', 'number'], count=3)
        self.assertEqual(len(self.host.web.requests), 1)
        self.assertEqual(builds, [{'id': '123', 'number': 123}])

    def test_execute_batch(self):
        self.host.web.append_prpc_response({
            'responses': [{
                'getBuild': {
                    'id': '123'
                },
            }, {
                'searchBuilds': {
                    'builds': [{
                        'id': '456'
                    }, {
                        'id': '789'
                    }],
                }
            }],
        })
        self.client.add_get_build_req(Build('linux-rel', 123),
                                      build_fields=['id'])
        self.client.add_search_builds_req({}, ['id'])
        build1, build2, build3 = self.client.execute_batch()
        (url, request), = self.host.web.requests
        self.assertEqual(
            url, 'https://cr-buildbucket.appspot.com'
            '/prpc/buildbucket.v2.Builds/Batch')
        self.assertEqual(
            json.loads(request), {
                'requests': [
                    {
                        'getBuild': {
                            'builder': {
                                'project': 'chromium',
                                'bucket': 'try',
                                'builder': 'linux-rel',
                            },
                            'buildNumber': 123,
                            'fields': 'id',
                        },
                    },
                    {
                        'searchBuilds': {
                            'predicate': {},
                            'fields': 'builds.*.id',
                        },
                    },
                ],
            })
        self.assertEqual(build1, {'id': '123'})
        self.assertEqual(build2, {'id': '456'})
        self.assertEqual(build3, {'id': '789'})

    def test_execute_batch_with_error(self):
        self.host.web.append_prpc_response({
            'responses': [{
                'error': {
                    'code': 5,
                    'message': 'resource not found',
                },
            }],
        })
        self.client.add_search_builds_req({})
        with self.assertRaises(RPCError):
            list(self.client.execute_batch())
        self.assertEqual(list(self.client.execute_batch()), [])

    def test_clear_batch(self):
        self.client.add_search_builds_req({})
        self.client.clear_batch()
        self.assertEqual(list(self.client.execute_batch()), [])
