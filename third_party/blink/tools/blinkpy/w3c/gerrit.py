# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import enum
import json
import logging
from datetime import datetime
from requests.exceptions import HTTPError
from typing import Iterator, List, Mapping, Optional, Tuple
from urllib.parse import urlencode, urlsplit, urlunsplit, quote

from blinkpy.common.host import Host
from blinkpy.common.net.network_transaction import NetworkTimeout
from blinkpy.common.path_finder import RELATIVE_WPT_TESTS
from blinkpy.common.net.git_cl import CLRevisionID
from blinkpy.w3c.chromium_commit import ChromiumCommit
from blinkpy.w3c.common import is_file_exportable

_log = logging.getLogger(__name__)
URL_BASE = urlsplit('https://chromium-review.googlesource.com')


class OutputOption(enum.Flag):
    """A mask denoting what data Gerrit should return in a query.

    See [0] for the full list of options, which should be added here as needed.

    [0]: https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#query-options
    """
    CURRENT_FILES = enum.auto()
    CURRENT_REVISION = enum.auto()
    COMMIT_FOOTERS = enum.auto()
    DETAILED_ACCOUNTS = enum.auto()
    MESSAGES = enum.auto()
    ALL_REVISIONS = enum.auto()
    SKIP_DIFFSTAT = enum.auto()

    def __iter__(self) -> Iterator['OutputOption']:
        # TODO(crbug.com/40631540): Remove this handcrafted `__iter__` after
        # python3.11+ when `enum.Flag` instances become iterable over their
        # members.
        for option in self.__class__:
            if option in self:
                yield option


class GerritAPI:
    """A utility class for the Chromium code review API.

    Wraps the API for Chromium's Gerrit instance at chromium-review.googlesource.com.
    """

    DEFAULT_OUTPUT = (OutputOption.CURRENT_FILES
                      | OutputOption.CURRENT_REVISION
                      | OutputOption.COMMIT_FOOTERS
                      | OutputOption.DETAILED_ACCOUNTS)

    def __init__(self,
                 host,
                 user: Optional[str] = None,
                 token: Optional[str] = None):
        self.host = host
        self.project_config = host.project_config
        # Authentication is only needed for mutating CLs (e.g., commenting).
        self.user = user
        self.token = token

    @classmethod
    def from_credentials(cls, host: Host,
                         credentials: Mapping[str, str]) -> 'GerritAPI':
        return cls(host, credentials.get('GERRIT_USER'),
                   credentials.get('GERRIT_TOKEN'))

    def get(self,
            path: str,
            query_params: List[Tuple[str, str]],
            raw: bool = False,
            return_none_on_404: bool = False):
        query_str = urlencode(query_params, safe='":')
        url = urlunsplit(
            (URL_BASE.scheme, URL_BASE.netloc, path, query_str, ''))
        raw_data = self.host.web.get_binary(
            url, return_none_on_404=return_none_on_404, trace='b346392205')
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
        assert path.startswith('/a/'), \
            'POST requests need to use authenticated routes.'
        url = urlunsplit((URL_BASE.scheme, URL_BASE.netloc, path, '', ''))
        assert self.user and self.token, 'Gerrit user and token required for authenticated routes.'

        b64auth = base64.b64encode('{}:{}'.format(self.user,
                                                  self.token).encode('utf-8'))
        headers = {
            'Authorization': 'Basic {}'.format(b64auth.decode('utf-8')),
            'Content-Type': 'application/json',
        }
        return self.host.web.request('POST',
                                     url,
                                     data=json.dumps(data).encode('utf-8'),
                                     headers=headers)

    def query_cl_comments_and_revisions(self, change_id: str) -> 'GerritCL':
        """Queries a CL with comments and revisions information."""
        return self.query_cl(
            change_id, OutputOption.MESSAGES | OutputOption.ALL_REVISIONS)

    def query_cl(
        self,
        change_id: str,
        output_options: OutputOption = DEFAULT_OUTPUT,
    ) -> 'GerritCL':
        """Queries a commit information from Gerrit."""
        # Gerrit can uniquely identify CLs by number (i.e., crrev.com/c/<n>) or
        # by `Change-Id` commit footer (i.e., a hex string prepended by "I"):
        # https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#change-id
        change_id_parts = [self.escaped_repo, change_id]
        if not change_id.isdigit():
            change_id_parts.insert(1, self.project_config.gerrit_branch)
        path = '/changes/' + '~'.join(change_id_parts)
        query_params = [('o', option.name) for option in output_options]
        try:
            cl_data = self.get(path, query_params, return_none_on_404=True)
        except NetworkTimeout:
            raise GerritError('Timed out querying CL using Change-Id')

        if not cl_data:
            raise GerritNotFoundError('Cannot find Change-Id')
        cl = GerritCL(data=cl_data, api=self)
        return cl

    def query_cls(
        self,
        query: str,
        limit: int = 500,
        output_options: OutputOption = DEFAULT_OUTPUT,
    ) -> List['GerritCL']:
        """Query Gerrit for CLs that match the given criteria.

        Arguments:
            query: The search criteria written in the syntax given by [0].
            limit: The maximum number of CLs to fetch.
            output_options: Fields to return (see `OutputOption`).

        [0]: https://gerrit-review.googlesource.com/Documentation/user-search.html
        """
        assert limit > 0
        query_params = [('q', query), ('n', str(limit))]
        query_params.extend(('o', option.name) for option in output_options)
        # The underlying host.web.get_binary() automatically retries until it
        # times out, at which point NetworkTimeout is raised.
        try:
            raw_cls = self.get('/changes/', query_params)
        except NetworkTimeout:
            raise GerritError('Timed out querying exportable open CLs.')
        return [GerritCL(data, self) for data in raw_cls]

    def query_exportable_cls(
        self,
        limit: int = 200,
        output_options: OutputOption = DEFAULT_OUTPUT,
    ) -> List['GerritCL']:
        query = ' '.join([
            f'project:"{self.project_config.gerrit_project}"',
            f'branch:{self.project_config.gerrit_branch}',
            'is:submittable',
            '-is:wip',
        ])
        open_cls = self.query_cls(query, limit, output_options)
        return [cl for cl in open_cls if cl.is_exportable()]

    @property
    def escaped_repo(self):
        return quote(self.project_config.gerrit_project, safe='')


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
        return '{}/{}'.format(urlunsplit(URL_BASE), self.number)

    @property
    def subject(self):
        return self._data['subject']

    @property
    def change_id(self):
        return self._data['change_id']

    @property
    def id(self):
        branch = self.api.project_config.gerrit_branch
        return f"{self.api.escaped_repo}~{branch}~{self.change_id}"

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
    def current_revision_id(self) -> CLRevisionID:
        patchset = int(self.current_revision['_number'])
        return CLRevisionID(self.number, patchset)

    @property
    def has_review_started(self):
        return self._data.get('has_review_started')

    @property
    def current_revision_description(self):
        # A patchset may have no description.
        return self.current_revision.get('description', '')

    @property
    def status(self):
        return self._data['status']

    @property
    def updated(self):
        # Timestamps are given in UTC and have the format "'yyyy-mm-dd hh:mm:ss.fffffffff'"
        # where "'ffffffffff'" represents nanoseconds.
        return datetime.strptime(self._data["updated"][:-3] + " +0000",
                                 "%Y-%m-%d %H:%M:%S.%f %z")

    @property
    def messages(self):
        return self._data['messages']

    @property
    def revisions(self):
        return self._data['revisions']

    def post_comment(self, message):
        """Posts a comment to the CL."""
        path = '/a/changes/{id}/revisions/current/review'.format(id=self.id)
        try:
            return self.api.post(path, {'message': message})
        except HTTPError as e:
            message = 'Failed to post a comment to issue {}'.format(
                self.change_id)
            if hasattr(e, 'response'):
                message += ' (code {})'.format(e.response.status_code)
            else:
                message += ' (error {})'.format(e.response.status_code)
            raise GerritError(message)

    def is_exportable(self):
        # TODO(robertma): Consolidate with the related part in chromium_exportable_commits.py.

        try:
            files = list(self.current_revision['files'].keys())
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

        files_in_wpt = [f for f in files if f.startswith(RELATIVE_WPT_TESTS)]
        if not files_in_wpt:
            return False

        exportable_files = [
            f for f in files_in_wpt
            if is_file_exportable(f, self.api.project_config)
        ]

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
        git = host.git(host.project_config.project_root)
        url = self.current_revision['fetch']['http']['url']
        ref = self.current_revision['fetch']['http']['ref']
        git.run(['fetch', url, ref])
        sha = git.run(['rev-parse', 'FETCH_HEAD']).strip()
        return ChromiumCommit(host, sha=sha)


class GerritError(Exception):
    """Raised when Gerrit returns a non-OK response or times out."""
    pass


class GerritNotFoundError(GerritError):
    """Raised when Gerrit returns a resource not found response."""
    pass
