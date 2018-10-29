# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.w3c.gerrit import GerritCL

# Some unused arguments may be included to match the real class's API.
# pylint: disable=unused-argument


class MockGerritAPI(object):

    def __init__(self):
        self.exportable_open_cls = []
        self.request_posted = []
        self.cl = ''
        self.cls_queried = []

    def query_exportable_open_cls(self):
        return self.exportable_open_cls

    def query_cl(self, change_id):
        self.cls_queried.append(change_id)
        return self.cl

    def get(self, path, raw=False):
        return '' if raw else {}

    def post(self, path, data):
        self.request_posted.append((path, data))
        return {}


class MockGerritCL(GerritCL):

    def __init__(self, data, api=None, chromium_commit=None):
        api = api or MockGerritAPI()
        self.chromium_commit = chromium_commit
        super(MockGerritCL, self).__init__(data, api)

    def fetch_current_revision_commit(self, host):
        return self.chromium_commit
