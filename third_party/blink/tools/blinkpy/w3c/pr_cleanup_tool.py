"""Cleans up PRs that correspond to abandoned CLs in Gerrit."""

import argparse
import logging

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.wpt_github import WPTGitHub
from blinkpy.w3c.gerrit import GerritAPI, GerritError
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

        self.wpt_github = self.wpt_github or WPTGitHub(
            self.host, gh_user, gh_token)
        self.gerrit = self.gerrit or GerritAPI(self.host, gr_user, gr_token)
        pull_requests = self.retrieve_all_prs()
        for pull_request in pull_requests:
            if pull_request.state != 'open':
                continue
            change_id = self.wpt_github.extract_metadata(
                'Change-Id: ', pull_request.body)

            if not change_id:
                continue

            try:
                cl = self.gerrit.query_cl(change_id)
            except GerritError as e:
                _log.error(
                    'Could not query change_id %s: %s', change_id, str(e))
                continue

            cl_status = cl.status
            if cl_status == 'ABANDONED':
                comment = 'Close this PR because the Chromium CL has been abandoned.'
                self.log_affected_pr_details(pull_request, comment)
                self.close_pr_and_delete_branch(pull_request.number, comment)
            elif cl_status == 'MERGED' and (not cl.is_exportable()):
                comment = 'Close this PR because the Chromium CL does not have exportable changes.'
                self.log_affected_pr_details(pull_request, comment)
                self.close_pr_and_delete_branch(pull_request.number, comment)

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
        """Retrieves last 1000 PRs."""
        return self.wpt_github.all_pull_requests()

    def close_pr_and_delete_branch(self, pull_request_number, comment):
        """Closes a PR with a comment and delete the corresponding branch."""
        self.wpt_github.add_comment(pull_request_number, comment)
        self.wpt_github.update_pr(pull_request_number, state='closed')
        branch = self.wpt_github.get_pr_branch(pull_request_number)
        self.wpt_github.delete_remote_branch(branch)

    def log_affected_pr_details(self, pull_request, comment):
        """Logs details of an affected PR."""
        _log.info(comment)
        _log.info('https://github.com/web-platform-tests/wpt/pull/%s',
                  pull_request.number)
        _log.info(self.wpt_github.extract_metadata(
            'Reviewed-on: ', pull_request.body))
