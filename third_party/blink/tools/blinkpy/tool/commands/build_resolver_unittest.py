# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.tool.commands.build_resolver import Build, BuildResolver


class BuildResolverTest(LoggingTestCase):
    """Basic build resolver tests.

    See high-level `rebaseline-cl` unit tests for coverage of triggering
    try builders and logging builds.
    """

    def setUp(self):
        super().setUp()
        self.host = MockHost()
        # A CL should only be required for try builders without explicit build
        # numbers.
        self.git_cl = MockGitCL(self.host, issue_number='None')
        self.resolver = BuildResolver(self.git_cl)

    def test_resolve_last_failing_ci_build(self):
        self.host.web.append_prpc_response({
            'responses': [{
                'searchBuilds': {
                    'builds': [{
                        'id': '123',
                        'builder': {
                            'builder': 'Fake Test Linux',
                            'bucket': 'ci',
                        },
                        'number': 123,
                        'status': 'FAILURE',
                    }],
                },
            }],
        })
        build_statuses = self.resolver.resolve_builds(
            [Build('Fake Test Linux', bucket='ci')])
        self.assertEqual(
            build_statuses, {
                Build('Fake Test Linux', 123, '123', 'ci'):
                TryJobStatus('COMPLETED', 'FAILURE'),
            })
        (_, body), = self.host.web.requests
        self.assertEqual(
            json.loads(body), {
                'requests': [{
                    'searchBuilds': {
                        'predicate': {
                            'builder': {
                                'project': 'chromium',
                                'bucket': 'ci',
                                'builder': 'Fake Test Linux',
                            },
                            'status': 'FAILURE',
                        },
                        'fields': ('builds.*.id,builds.*.number,'
                                   'builds.*.builder.builder,'
                                   'builds.*.builder.bucket,builds.*.status'),
                    },
                }],
            })

    def test_resolve_builds_with_explicit_numbers(self):
        self.host.web.append_prpc_response({
            'responses': [{
                'getBuild': {
                    'id': '123',
                    'builder': {
                        'builder': 'Fake Test Linux',
                        'bucket': 'ci',
                    },
                    'number': 123,
                    'status': 'FAILURE',
                },
            }, {
                'getBuild': {
                    'id': '456',
                    'builder': {
                        'builder': 'linux-rel',
                        'bucket': 'try',
                    },
                    'number': 456,
                    'status': 'SCHEDULED',
                },
            }],
        })
        build_statuses = self.resolver.resolve_builds([
            Build('Fake Test Linux', 123, bucket='ci'),
            Build('linux-rel', 456),
        ])
        self.assertEqual(
            build_statuses, {
                Build('Fake Test Linux', 123, '123', 'ci'):
                TryJobStatus('COMPLETED', 'FAILURE'),
                Build('linux-rel', 456, '456'):
                TryJobStatus('SCHEDULED', None),
            })
        (_, body), = self.host.web.requests
        self.assertEqual(
            json.loads(body), {
                'requests': [{
                    'getBuild': {
                        'builder': {
                            'project': 'chromium',
                            'bucket': 'ci',
                            'builder': 'Fake Test Linux',
                        },
                        'buildNumber':
                        123,
                        'fields': ('id,number,builder.builder,builder.bucket,'
                                   'status'),
                    },
                }, {
                    'getBuild': {
                        'builder': {
                            'project': 'chromium',
                            'bucket': 'try',
                            'builder': 'linux-rel',
                        },
                        'buildNumber':
                        456,
                        'fields': ('id,number,builder.builder,builder.bucket,'
                                   'status'),
                    },
                }],
            })
