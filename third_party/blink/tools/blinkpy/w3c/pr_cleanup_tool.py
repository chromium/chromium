"""Cleans up PRs that correspond to abandoned CLs in Gerrit."""

import logging
from datetime import datetime, timedelta

from blinkpy.w3c.gerrit import GerritError

_log = logging.getLogger(__name__)


class PrCleanupTool(object):
    def __init__(self, host):
        self.host = host

    def run(self, wpt_github, gerrit):
        """Closes all PRs that are abandoned two weeks ago in Gerrit."""
        _log.info(
            "Close exported PRs where the corresponding CLs have been abandoned."
        )
        pull_requests = self.retrieve_provisioned_prs(wpt_github)
        past = datetime.utcnow() - timedelta(days=14)
        for pull_request in pull_requests:
            if pull_request.state != 'open':
                continue
            change_id = wpt_github.extract_metadata('Change-Id: ',
                                                    pull_request.body)

            if not change_id:
                continue

            try:
                cl = gerrit.query_cl(change_id)
            except GerritError as e:
                _log.error('Could not query change_id %s: %s', change_id,
                           str(e))
                continue

            cl_status = cl.status
            if cl_status == 'ABANDONED' and cl.updated < past:
                comment = 'Close this PR because the Chromium CL has been abandoned.'
                self.log_affected_pr_details(wpt_github, pull_request, comment)
                self.close_pr_and_delete_branch(wpt_github,
                                                pull_request.number, comment)
            elif cl_status == 'MERGED' and (not cl.is_exportable()):
                comment = 'Close this PR because the Chromium CL does not have exportable changes.'
                self.log_affected_pr_details(wpt_github, pull_request, comment)
                self.close_pr_and_delete_branch(wpt_github,
                                                pull_request.number, comment)

        return True

    def retrieve_provisioned_prs(self, wpt_github):
        """Retrieves last 1000 PRs with 'do not merge' label."""
        return wpt_github.all_provisional_pull_requests()

    def close_pr_and_delete_branch(self, wpt_github, pull_request_number,
                                   comment):
        """Closes a PR with a comment and delete the corresponding branch."""
        wpt_github.add_comment(pull_request_number, comment)
        wpt_github.update_pr(pull_request_number, state='closed')
        branch = wpt_github.get_pr_branch(pull_request_number)
        wpt_github.delete_remote_branch(branch)

    def log_affected_pr_details(self, wpt_github, pull_request, comment):
        """Logs details of an affected PR."""
        _log.info(comment)
        _log.info('https://github.com/web-platform-tests/wpt/pull/%s',
                  pull_request.number)
        _log.info(
            wpt_github.extract_metadata('Reviewed-on: ', pull_request.body))
