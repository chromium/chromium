"""Cleans up PRs that correspond to abandoned CLs in Gerrit."""

import argparse
import logging

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.wpt_github import WPTGitHub
from blinkpy.w3c.gerrit import GerritAPI
from blinkpy.w3c.common import (
    read_credentials
)

_log = logging.getLogger(__name__)


class PrCleanupTool(object):
    def __init__(self, host):
        self.host = host
        self.wpt_github = None
        self.gerrit = None

    def main(self, argv=None):
        """Closes all PRs that are abandoned in Gerrit."""
        options = self.parse_args(argv)
        log_level = logging.DEBUG if options.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)
        credentials = read_credentials(self.host, options.credentials_json)
        gh_user = credentials.get('GH_USER')
        gh_token = credentials.get('GH_TOKEN')
        if not gh_user or not gh_token:
            _log.error('You have not set your GitHub credentials. This '
                       'script may fail with a network error when making '
                       'an API request to GitHub.')
            _log.error('See https://chromium.googlesource.com/chromium/src'
                       '/+/master/docs/testing/web_platform_tests.md'
                       '#GitHub-credentials for instructions on how to set '
                       'your credentials up.')
            return False

        gr_user = credentials['GERRIT_USER']
        gr_token = credentials['GERRIT_TOKEN']
        if not gr_user or not gr_token:
            _log.warning('You have not set your Gerrit credentials. This '
                         'script may fail with a network error when making '
                         'an API request to Gerrit.')

        self.wpt_github = self.wpt_github or WPTGitHub(self.host, gh_user, gh_token)
        self.gerrit = self.gerrit or GerritAPI(self.host, gr_user, gr_token)
        pull_requests = self.retrieve_all_prs()
        for pull_request in pull_requests:
            if pull_request.state != 'open':
                continue
            change_id = self.wpt_github.extract_metadata('Change-Id: ', pull_request.body)
            if not change_id:
                continue
            cl_status = self.gerrit.query_cl(change_id).status
            if cl_status == 'ABANDONED':
                _log.info('https://github.com/web-platform-tests/wpt/pull/%s', pull_request.number)
                _log.info(self.wpt_github.extract_metadata('Reviewed-on: ', pull_request.body))
                self.close_abandoned_pr(pull_request)
        return True

    def parse_args(self, argv):
        parser = argparse.ArgumentParser()
        parser.description = __doc__
        parser.add_argument(
            '-v', '--verbose', action='store_true',
            help='log extra details that may be helpful when debugging')
        parser.add_argument(
            '--credentials-json',
            help='A JSON file with GitHub credentials, '
                 'generally not necessary on developer machines')
        return parser.parse_args(argv)

    def retrieve_all_prs(self):
        """Retrieve last 1000 PRs."""
        return self.wpt_github.all_pull_requests()

    def close_abandoned_pr(self, pull_request):
        """Closes a PR if the original CL is abandoned."""
        comment = 'Close this PR because the Chromium CL has been abandoned.'
        self.wpt_github.add_comment(pull_request.number, comment)
        self.wpt_github.update_pr(pull_request.number, state='closed')
