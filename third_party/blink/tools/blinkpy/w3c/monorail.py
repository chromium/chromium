# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CRBUG_BASE_URL = 'https://crbug.com/'


class MonorailIssue(object):
    """A lean abstraction of a Monorail issue.

    The list of fields and verification are not exhaustive.
    """

    # Monorail API docs: https://goo.gl/pycDK2#monorail_issues_insert
    # Fields that are strings.
    _STRING_FIELDS = frozenset(['status', 'summary', 'description'])
    # Fields that are lists of strings.
    _STRING_LIST_FIELDS = frozenset(['cc', 'labels', 'components'])
    _ALLOWED_FIELDS = _STRING_LIST_FIELDS | _STRING_FIELDS

    # Again, the list is non-exhaustive.
    _VALID_STATUSES = frozenset(
        ['Unconfirmed', 'Untriaged', 'Available', 'Assigned', 'Started'])

    def __init__(self, project_id, **kwargs):
        self.project_id = project_id
        for key in kwargs:
            assert key in self._ALLOWED_FIELDS, 'Unknown field: ' + key
        self._body = kwargs
        self._normalize()

    def _normalize(self):
        # These requirements are based on trial and error. No docs were found.
        assert self.project_id, 'project_id cannot be empty.'
        self._body['projectId'] = self.project_id
        for field in self._STRING_LIST_FIELDS:
            if field in self._body:
                # Not a str or unicode.
                assert not isinstance(self._body[field], str)
                # Is iterable (TypeError would be raised otherwise).
                self._body[field] = list(self._body[field])
        # We expect a KeyError to be raised if 'status' is missing.
        self._body['status'] = self._body['status'].capitalize()
        assert self._body['status'] in self._VALID_STATUSES, \
            'Unknown status %s.' % self._body['status']
        assert self._body['summary'], 'summary cannot be empty.'

    def __str__(self):
        result = ('Monorail issue in project {}\n'
                  'Summary: {}\n'
                  'Status: {}\n').format(self.project_id, self.body['summary'],
                                         self.body['status'])
        if 'cc' in self.body:
            result += 'CC: {}\n'.format(', '.join(self.body['cc']))
        if 'components' in self.body:
            result += 'Components: {}\n'.format(', '.join(
                self.body['components']))
        if 'labels' in self.body:
            result += 'Labels: {}\n'.format(', '.join(self.body['labels']))
        if 'description' in self.body:
            result += 'Description:\n{}\n'.format(self.body['description'])
        return result

    @property
    def body(self):
        return self._body

    @staticmethod
    def new_chromium_issue(summary,
                           description='',
                           cc=None,
                           components=None,
                           priority='3',
                           type='Bug',
                           labels=None):
        """Creates a minimal new Chromium issue.

        Chromium requires at least summary, priority and type: you must provide
        the summary, whereas priority defaults to 3 and type defaults to Bug.

        Args:
            summary: The summary line.
            description: The issue description.
            cc: A list of email addresses to CC.
            components: A list of components.
            priority: A string, defaults to '3'.
            type: A string, defaults to 'Bug'.
            labels: A list of labels (strings).
        """
        return MonorailIssue('chromium',
                             summary=summary,
                             description=description,
                             cc=cc or [],
                             components=components or [],
                             status='Untriaged',
                             labels=['Pri-' + priority, 'Type-' + type] +
                             (labels or []))

    @staticmethod
    def crbug_link(issue_id):
        return CRBUG_BASE_URL + str(issue_id)


class MonorailAPI(object):
    """A wrapper of Monorail API.

    Unlike other code in blinkpy, this class uses os, sys and network directly
    (via oauth2client and googleapiclient).
    """

    # TODO(robertma): Mock googleapiclient and oauth2client to test this class.

    _DISCOVERY_URL = (
        'https://monorail-prod.appspot.com/_ah/api/discovery/v1/apis/'
        '{api}/{apiVersion}/rest')

    def __init__(self, service_account_key_json=None, access_token=None):
        """Initializes a MonorailAPI instance.

        Args:
            service_account_key_json: The path to a JSON private key of a
                service account for accessing Monorail. If None, use access_token.
            access_token: An OAuth access token. If None, fall back to Google
                application default credentials.
        """
        # Make it easier to mock out the two libraries in the future.
        # Dependencies managed by wpt-import.vpython - pylint: disable=import-error,no-member
        import googleapiclient.discovery
        self._api_discovery = googleapiclient.discovery
        import oauth2client.client
        self._oauth2_client = oauth2client.client

        # TODO(robertma): Deprecate the JSON key support once BuildBot is gone.
        if service_account_key_json:
            credentials = self._oauth2_client.GoogleCredentials.from_stream(
                service_account_key_json)
        elif access_token:
            credentials = self._oauth2_client.AccessTokenCredentials(
                access_token=access_token, user_agent='blinkpy/1.0')
        else:
            credentials = self._oauth2_client.GoogleCredentials.get_application_default(
            )

        # cache_discovery needs to be disabled because of https://github.com/google/google-api-python-client/issues/299
        self.api = self._api_discovery.build(
            'monorail',
            'v1',
            discoveryServiceUrl=self._DISCOVERY_URL,
            credentials=credentials,
            cache_discovery=False)

    @staticmethod
    def _fix_cc_in_body(body):
        # TODO(crbug.com/monorail/3300): Despite the docs, 'cc' is in fact a
        # list of dictionaries with only one string field 'name'. Hide the bug
        # and expose the cleaner, documented API for now.
        if 'cc' in body:
            body['cc'] = [{'name': email} for email in body['cc']]
        return body

    def insert_issue(self, issue):
        body = self._fix_cc_in_body(issue.body)
        return self.api.issues().insert(
            projectId=issue.project_id, body=body).execute()
