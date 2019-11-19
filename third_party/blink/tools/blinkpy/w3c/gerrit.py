# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
from urllib2 import HTTPError

from blinkpy.common.net.network_transaction import NetworkTimeout
from blinkpy.w3c.chromium_commit import ChromiumCommit
from blinkpy.w3c.chromium_finder import absolute_chromium_dir
from blinkpy.w3c.common import CHROMIUM_WPT_DIR, is_file_exportable

_log = logging.getLogger(__name__)
URL_BASE = 'https://chromium-review.googlesource.com'
# https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#query-options
QUERY_OPTIONS = 'o=CURRENT_FILES&o=CURRENT_REVISION&o=COMMIT_FOOTERS&o=DETAILED_ACCOUNTS'


class GerritAPI(object):
    """A utility class for the Chromium code review API.

    Wraps the API for Chromium's Gerrit instance at chromium-review.googlesource.com.
    """

    def __init__(self, host, user, token):
        self.host = host
        self.user = user
        self.token = token

    def get(self, path, raw=False):
        url = URL_BASE + path
        raw_data = self.host.web.get_binary(url)
        if raw:
            return raw_data

        if not raw_data:
            return None

        # Gerrit API responses are prefixed by a 5-character JSONP preamble
        return json.loads(raw_data[5:])

    def post(self, path, data):
        """Sends a POST request to path with data as the JSON payload.

        The path has to be prefixed with '/a/':
        https://gerrit-review.googlesource.com/Documentation/rest-api.html#authentication
        """
        assert path.startswith('/a/'), 'POST requests need to use authenticated routes.'
        url = URL_BASE + path
        assert self.user and self.token, 'Gerrit user and token required for authenticated routes.'

        b64auth = base64.b64encode('{}:{}'.format(self.user, self.token))
        headers = {
            'Authorization': 'Basic {}'.format(b64auth),
            'Content-Type': 'application/json',
        }
        return self.host.web.request('POST', url, data=json.dumps(data), headers=headers)

    def query_cl_comments_and_revisions(self, change_id):
        """Queries a CL with comments and revisions information."""
        return self.query_cl(change_id, 'o=MESSAGES&o=ALL_REVISIONS')

    def query_cl(self, change_id, query_options=QUERY_OPTIONS):
        """Queries a commit information from Gerrit."""
        path = '/changes/chromium%2Fsrc~master~{}?{}'.format(change_id, query_options)
        try:
            cl_data = self.get(path)
        except NetworkTimeout:
            raise GerritError('Timed out querying CL using Change-Id')

        if not cl_data:
            raise GerritError('Cannot find Change-Id')

        cl = GerritCL(data=cl_data, api=self)
        return cl

    def query_exportable_open_cls(self, limit=500):
        path = ('/changes/?q=project:\"chromium/src\"+branch:master+is:open+'
                '-is:wip&{}&n={}').format(QUERY_OPTIONS, limit)
        # The underlying host.web.get_binary() automatically retries until it
        # times out, at which point NetworkTimeout is raised.
        try:
            open_cls_data = self.get(path)
        except NetworkTimeout:
            raise GerritError('Timed out querying exportable open CLs.')
        open_cls = [GerritCL(data, self) for data in open_cls_data]

        return [cl for cl in open_cls if cl.is_exportable()]


class GerritCL(object):
    """A data wrapper for a Chromium Gerrit CL."""

    def __init__(self, data, api):
        assert data['change_id']
        self._data = data
        self.api = api

    @property
    def number(self):
        return self._data['_number']

    @property
    def url(self):
        return '{}/{}'.format(URL_BASE, self.number)

    @property
    def subject(self):
        return self._data['subject']

    @property
    def change_id(self):
        return self._data['change_id']

    @property
    def owner_email(self):
        return self._data['owner']['email']

    @property
    def current_revision_sha(self):
        return self._data['current_revision']

    @property
    def current_revision(self):
        return self._data['revisions'][self.current_revision_sha]

    @property
    def has_review_started(self):
        return self._data.get('has_review_started')

    @property
    def current_revision_description(self):
        return self.current_revision['description']

    @property
    def status(self):
        return self._data['status']

    @property
    def messages(self):
        return self._data['messages']

    @property
    def revisions(self):
        return self._data['revisions']

    def post_comment(self, message):
        """Posts a comment to the CL."""
        path = '/a/changes/{change_id}/revisions/current/review'.format(
            change_id=self.change_id,
        )
        try:
            return self.api.post(path, {'message': message})
        except HTTPError as e:
            raise GerritError('Failed to post a comment to issue {} (code {}).'.format(
                self.change_id, e.code))

    def is_exportable(self):
        # TODO(robertma): Consolidate with the related part in chromium_exportable_commits.py.

        try:
            files = self.current_revision['files'].keys()
        except KeyError:
            # Empty (deleted) CL is not exportable.
            return False

        # Guard against accidental CLs that touch thousands of files.
        if len(files) > 1000:
            _log.info('Rejecting CL with over 1000 files: %s (ID: %s) ',
                      self.subject, self.change_id)
            return False

        if 'No-Export: true' in self.current_revision['commit_with_footers']:
            return False

        if 'NOEXPORT=true' in self.current_revision['commit_with_footers']:
            return False

        files_in_wpt = [f for f in files if f.startswith(CHROMIUM_WPT_DIR)]
        if not files_in_wpt:
            return False

        exportable_files = [f for f in files_in_wpt if is_file_exportable(f)]

        if not exportable_files:
            return False

        return True

    def fetch_current_revision_commit(self, host):
        """Fetches the git commit for the latest revision of CL.

        This method fetches the commit corresponding to the latest revision of
        CL to local Chromium repository, but does not checkout the commit to the
        working tree. All changes in the CL are squashed into this one commit,
        regardless of how many revisions have been uploaded.

        Args:
            host: A Host object for git invocation.

        Returns:
            A ChromiumCommit object (the fetched commit).
        """
        git = host.git(absolute_chromium_dir(host))
        url = self.current_revision['fetch']['http']['url']
        ref = self.current_revision['fetch']['http']['ref']
        git.run(['fetch', url, ref])
        sha = git.run(['rev-parse', 'FETCH_HEAD']).strip()
        return ChromiumCommit(host, sha=sha)


class GerritError(Exception):
    """Raised when Gerrit returns a non-OK response or times out."""
    pass
