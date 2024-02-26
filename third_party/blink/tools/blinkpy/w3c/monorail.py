# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


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

    def get_issue(self, project_id, issue_id):
        return self.api.issues().get(projectId=project_id,
                                     issueId=issue_id).execute()

    def get_comment_list(self, project_id, issue_id):
        return self.api.issues().comments().list(projectId=project_id,
                                                 issueId=issue_id).execute()

    def insert_comment(self, project_id, issue_id, content):
        return self.api.issues().comments().insert(projectId=project_id,
                                                   issueId=issue_id,
                                                   sendEmail=False,
                                                   body={
                                                       'content': content
                                                   }).execute()
