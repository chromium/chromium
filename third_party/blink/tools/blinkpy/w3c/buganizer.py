# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import enum
import functools
import logging
import re
import textwrap
from dataclasses import dataclass, field
from http import client as http_client
from typing import List, Optional, Union
from urllib.parse import urlsplit

from blinkpy.common.memoized import memoized
from blinkpy.common.net.web import Web

import google.auth
import google_auth_httplib2
from apiclient import discovery

_DISCOVERY_URI = (
    'https://issuetracker.googleapis.com/$discovery/rest?version=v1&labels=GOOGLE_PUBLIC'
)

BUGANIZER_SCOPES = 'https://www.googleapis.com/auth/buganizer'
EMAIL_SCOPE = 'https://www.googleapis.com/auth/userinfo.email'

MAX_DISCOVERY_RETRIES = 3
MAX_REQUEST_RETRIES = 5
MAX_PAGE_SIZE = 500

class Status(enum.Enum):
    NEW = enum.auto()
    ASSIGNED = enum.auto()
    ACCEPTED = enum.auto()
    FIXED = enum.auto()
    VERIFIED = enum.auto()
    NOT_REPRODUCIBLE = enum.auto()
    INTENDED_BEHAVIOR = enum.auto()
    OBSOLETE = enum.auto()
    INFEASIBLE = enum.auto()
    DUPLICATE = enum.auto()


class Priority(enum.IntEnum):
    P0 = 0
    P1 = 1
    P2 = 2
    P3 = 3
    P4 = 4


class Severity(enum.IntEnum):
    S0 = 0
    S1 = 1
    S2 = 2
    S3 = 3
    S4 = 4


@dataclass(frozen=True)
class BuganizerIssue:
    """A (simplified) representation Buganizer's `Issue` message [0].

    [0]: ///depot/google3/google/devtools/issuetracker/v1/issuetracker.proto
    """
    title: str
    description: str
    component_id: str
    issue_id: Optional[int] = None
    cc: List[str] = field(default_factory=list)
    status: Status = Status.NEW
    # `priority` and `severity` are `IntEnum`s to create orderings.
    priority: Priority = Priority.P3
    severity: Severity = Severity.S4
    # TODO(crbug.com/40283194): There are some fields that aren't needed now
    # but we may want to support in the future:
    #   * `assignee` (i.e., "owner")
    #   * Monorail's old labels (e.g., Test-WebTest) as Buganizer hotlists or
    #     custom fields

    def __str__(self) -> str:
        link = f' {self.link}' if self.link else ''
        formatted_issue = textwrap.dedent(f"""\
            Issue{link}: {self.title}
              Status: {self.status.name}
              Component ID: {self.component_id}
              CC: {", ".join(self.cc) or "(none)"}
              Priority: {self.priority.name}
              Severity: {self.severity.name}
              Description:
            """)
        formatted_issue += textwrap.indent(self.description, ' ' * 4)
        return f'{formatted_issue.rstrip()}\n'

    @functools.cached_property
    def link(self) -> Optional[str]:
        return f'https://crbug.com/{self.issue_id}' if self.issue_id else None

    @classmethod
    def from_payload(cls, payload) -> 'BuganizerIssue':
        # `issueState` and some constituent fields accessed here are required
        # and should always exist.
        state = payload['issueState']
        cc = [user.get('emailAddress', '') for user in state.get('ccs', [])]
        return cls(
            title=state['title'],
            # May or may not exist, depending on the context and endpoint.
            description=payload.get('issueComment', {}).get('comment', ''),
            component_id=str(state['componentId']),
            issue_id=payload.get('issueId'),
            cc=[email for email in cc if email],
            status=Status[state['status']],
            priority=Priority[state['priority']],
            severity=Severity[state['severity']])


# An issue ID may be one of:
# * A valid Buganizer ID (at least 8 digits).
# * A historic bug ID (7 digits or fewer) in the `chromium` Monorail project.
# * A URL that will be resolved into one of the above. May also reference
#   non-`chromium` projects (e.g., `skia`).
IssueID = Union[str, int]


class BuganizerClient:
    MIN_ID: int = 10_000_000
    _URL_PATTERN: re.Pattern = re.compile('https?://[^/]+/(?P<id>\d{8,})')

    def __init__(self, service=None, web: Optional[Web] = None):
        self._web = web or Web()
        self._service = service
        if self._service is not None:
            return

        http = ServiceAccountHttp(BUGANIZER_SCOPES)
        http.timeout = 30
        http_exception = None
        for attempt in range(MAX_DISCOVERY_RETRIES):
            try:
                self._service = discovery.build(
                    'issuetracker',
                    'v1',
                    discoveryServiceUrl=_DISCOVERY_URI,
                    http=http)
                break
            except http_client.HTTPException as e:
                logging.error('Attempt #%d: %s', attempt + 1, e)
                http_exception = e

        if self._service is None:
            raise BuganizerError(
                'failed to connect to service') from http_exception

    def GetIssue(self, issue_id: IssueID):
        """Makes a request to the issue tracker to get an issue."""
        request = self._service.issues().get(issueId=self._ResolveID(issue_id))
        try:
            return self._ExecuteRequest(request)
        except Exception as e:
            logging.error('[BuganizerClient] Failed to GetIssue '
                          'error: %s', str(e))
            return {'error': str(e)}

    def GetIssueList(self,
                     query_string,
                     limit: int = MAX_PAGE_SIZE) -> List[BuganizerIssue]:
        """Makes a request to the issue tracker to get list of issues by query"""
        # TODO(crbug.com/333112144) : Use nextPageToken in response to support
        # more than 500 issues
        request = self._service.issues().list(query=query_string,
                                              pageSize=min(
                                                  MAX_PAGE_SIZE, limit),
                                              view='FULL')
        try:
            response = self._ExecuteRequest(request)
            logging.debug('[BuganizerClient] GetIssueList response: %s',
                          response)
            if not response:
                return []
            issues = [
                BuganizerIssue.from_payload(issue_payload)
                for issue_payload in response.get('issues', [])
            ]
            return issues
        except Exception as e:
            raise BuganizerError(f'failed to get issue list: {e}') from e

    def GetIssueComments(self, issue_id: IssueID):
        """Makes a request to the issue tracker to get all the comments."""
        request = self._service.issues().issueUpdates().list(
            issueId=str(self._ResolveID(issue_id)))

        try:
            response = self._ExecuteRequest(request)
            logging.debug(
                '[BuganizerClient] Post GetIssueComments response:'
                ' %s', response)
            comments = []
            if not response:
                return comments

            issue_updates = response.get('issueUpdates', [])
            for index, update in enumerate(issue_updates):
                comment = {
                    'index': index,
                    'timestamp': update.get('timestamp'),
                    'author': update.get('author', {}).get('emailAddress', ''),
                    'comment': update.get('issueComment',
                                          {}).get('comment', ''),
                }
                comments.append(comment)
            return comments
        except Exception as e:
            logging.error(
                '[BuganizerClient] Failed to GetIssueComments '
                'error: %s', str(e))
            return {'error': str(e)}

    def NewComment(self, issue_id: IssueID, comment: str):
        """Makes a request to the issue tracker to add a comment."""
        new_comment_request = {'issueComment': {'comment': comment}}
        request = self._service.issues().modify(issueId=str(
            self._ResolveID(issue_id)), body=new_comment_request)
        try:
            return self._ExecuteRequest(request)
        except Exception as e:
            logging.error(
                '[BuganizerClient] Failed to NewComment '
                'error: %s', str(e))
            return {'error': str(e)}

    @memoized
    def _ResolveID(self, issue_id: IssueID) -> int:
        """Resolve any Buganizer or Monorail ID/URL into a valid Buganizer ID.

        Notes on how different projects allocate IDs:
        * Monorail is organized into different "projects" with separate ID
          spaces (e.g., `crbug.com/v8/1` is a different issue from
          `crbug.com/skia/1`).
        * Buganizer has separate "trackers" for each project (e.g.,
          `issues.chromium.org` versus `issues.skia.org`), but all IDs are
          allocated from one space.
        """
        if isinstance(issue_id, str):
            url = issue_id
            if not urlsplit(url).scheme:
                url = f'https://{url}'
            # This URL might already contain a valid Buganizer ID.
            maybe_match = self._URL_PATTERN.fullmatch(url)
        elif issue_id >= self.MIN_ID:
            return issue_id
        else:
            url, maybe_match = f'https://crbug.com/{issue_id}', None

        # Unfortunately, there's no machine-readable way to translate historic
        # Monorail bug IDs (seven digits or fewer) to the migrated Buganizer
        # ones. Therefore, we must resort to scraping the JavaScript that
        # performs the redirect:
        # https://source.chromium.org/chromium/infra/infra/+/main:appengine/monorail/redirect/templates/redirect.html
        #
        # See b/331419421 for updates.
        if not maybe_match:
            text = self._web.get_binary(url).decode(errors='replace')
            maybe_match = self._URL_PATTERN.search(text)
        if not maybe_match:
            raise BuganizerError(
                f'{issue_id!r} did not resolve into a valid Buganizer ID')
        return int(maybe_match.group('id'))

    def _ExecuteRequest(self, request):
        """Makes a request to the issue tracker.

            Args:
            request: The request object, which has a execute method.

            Returns:
            The response if there was one, or else None.
        """
        response = request.execute(num_retries=MAX_REQUEST_RETRIES,
                                   http=ServiceAccountHttp(BUGANIZER_SCOPES))
        return response

    def NewIssue(self, issue: BuganizerIssue) -> BuganizerIssue:
        """File a new bug with the `CreateIssue` RPC [0].

        [0]: ///depot/google3/google/devtools/issuetracker/v1/issuetracker_service.proto

        Raises:
            BuganizerError: If the client could not create the issue.
        """
        new_issue = {
            'issueState': {
                'title': issue.title,
                'componentId': issue.component_id,
                'status': issue.status.name,
                'type': 'BUG',
                'severity': issue.severity.name,
                'priority': issue.priority.name,
                'ccs': [{
                    'emailAddress': email,
                } for email in set(issue.cc)],
            },
            'issueComment': {
                'comment': issue.description,
            },
        }

        logging.warning('[BuganizerClient] PostIssue request: %s', new_issue)
        request = self._service.issues().create(body=new_issue)

        try:
            response = self._ExecuteRequest(request)
            logging.debug('[BuganizerClient] PostIssue response: %s', response)
            return BuganizerIssue.from_payload(response)
        except Exception as e:
            raise BuganizerError(f'failed to create issue: {e}') from e


class BuganizerError(Exception):
    """Base exception representing a failed Buganizer operation."""


def ServiceAccountHttp(scope=EMAIL_SCOPE, timeout=None):
    """Returns the Credentials of the service account if available."""
    assert scope, "ServiceAccountHttp scope must not be None."
    credentials = _GetAppDefaultCredentials(scope)
    http = google_auth_httplib2.AuthorizedHttp(credentials)
    if timeout:
        http.timeout = timeout
    return http


def _GetAppDefaultCredentials(scope=None):
    try:
        credentials, _ = google.auth.default()
        if scope and credentials.requires_scopes:
            credentials = credentials.with_scopes([scope])
        return credentials
    except google.auth.exceptions.DefaultCredentialsError as e:
        logging.error(
            '[BuganizerClient]  Error when getting the application default'
            ' credentials: %s', str(e))
        return None
