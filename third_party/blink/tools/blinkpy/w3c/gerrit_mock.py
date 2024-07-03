# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import urllib.parse

from blinkpy.common.host_mock import MockHost
from blinkpy.w3c.gerrit import (
    GerritAPI,
    GerritCL,
    GerritError,
    OutputOption,
)
# Some unused arguments may be included to match the real class's API.
# pylint: disable=unused-argument


class MockGerritAPI:

    def __init__(self, host=None, raise_error=False):
        self.host = host or MockHost()
        self.exportable_cls = []
        self.request_posted = []
        self.cl = MockGerritCL(
            {
                'change_id': 'I01234abc',
                'revisions': {
                    'abc01234': {
                        '_number': 1,
                        'kind': 'REWORK',
                    },
                },
            }, self)
        self.cls_queried = []
        self.raise_error = raise_error
        self.project_config = self.host.project_config

    def query_exportable_cls(self):
        return self.exportable_cls

    def query_cl_comments_and_revisions(self, change_id):
        return self.query_cl(
            change_id, OutputOption.MESSAGES | OutputOption.ALL_REVISIONS)

    def query_cl(self, change_id, query_options=GerritAPI.DEFAULT_OUTPUT):
        self.cls_queried.append(change_id)
        if self.raise_error:
            raise GerritError("Error from query_cl")
        return self.cl

    def get(self, path, raw=False):
        return '' if raw else {}

    def post(self, path, data):
        self.request_posted.append((path, data))
        return {}

    @property
    def escaped_repo(self):
        return urllib.parse.quote(self.project_config.gerrit_project, safe='')


class MockGerritCL(GerritCL):
    def __init__(self, data, api=None, chromium_commit=None):
        api = api or MockGerritAPI()
        self.chromium_commit = chromium_commit
        super(MockGerritCL, self).__init__(data, api)

    def fetch_current_revision_commit(self, host):
        return self.chromium_commit
