# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from http import client as http_client
import logging

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


class BuganizerClient:
    """Testing call buganizer api"""
    def __init__(self):
        """Initializes an object for communicate to the Buganizer."""
        http = ServiceAccountHttp(BUGANIZER_SCOPES)
        http.timeout = 30

        # Retry connecting at least 3 times.
        self._service = None
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
            raise http_exception

    def GetIssue(self, id):
        """Makes a request to the issue tracker to get an issue."""
        request = self._service.issues().get(issueId=id)
        response = self._ExecuteRequest(request)
        return response

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

    def NewIssue(self,
                 title,
                 description,
                 project='chromium',
                 priority='P3',
                 severity='S3',
                 components=None,
                 owner=None,
                 cc=None,
                 status=None,
                 componentId=None):
        """ Create an issue on Buganizer

            While the API looks the same as in monorail_client, similar to what we do
            in reconciling buganizer data, we need to reconstruct the data in the
            reversed way: from the monorail fashion to the buganizer fashion.
            The issueState property should always exist for an Issue, and these
            properties are required for an issueState:
            title, componentId, status, type, severity, priority.

            Args:
            title: a string as the issue title.
            description: a string as the initial description of the issue.
            project: this is no longer needed in Buganizer. When creating an issue,
                we will NOT use it to look for the corresponding components.
            labels: a list of Monorail labels, each of which will be mapped to a
                Buganizer hotlist id.
            components: a list of component names in Monorail. The size of the list
                should always be 1 as required by Buganizer.
            owner: the email address of the issue owner/assignee.
            cc: a list of email address to which the issue update is cc'ed.
            status: the initial status of the issue

            Returns:
            {'issue_id': id, 'project_id': project_name} if succeeded; otherwise
            {'error': error_msg}
        """

        new_issue_state = {
            'title': title,
            'componentId': componentId,
            'status': status,
            'type': 'BUG',
            'severity': severity,
            'priority': priority,
        }

        new_description = {'comment': description}

        if owner:
            new_issue_state['assignee'] = {'emailAddress': owner}
        if cc:
            emails = set(email.strip() for email in cc if email.strip())
            new_issue_state['ccs'] = [{
                'emailAddress': email
            } for email in emails if email]

        new_issue = {
            'issueState': new_issue_state,
            'issueComment': new_description
        }

        logging.warning('[PerfIssueService] PostIssue request: %s', new_issue)
        request = self._service.issues().create(body=new_issue)

        try:
            response = self._ExecuteRequest(request)
            logging.debug('[PerfIssueService] PostIssue response: %s',
                          response)
            if response and 'issueId' in response:
                return {'issue_id': response['issueId'], 'project_id': project}
            logging.error('Failed to create new issue; response %s', response)
        except Exception as e:
            return {'error': str(e)}
        return {'error': 'Unknown failure creating issue.'}


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
            'Error when getting the application default credentials: %s',
            str(e))
        return None
