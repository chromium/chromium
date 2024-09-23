# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.w3c.wpt_github import MergeError, WPTGitHub


class MockWPTGitHub(object):

    # Some unused arguments may be included to match the real class's API.
    # pylint: disable=unused-argument

    def __init__(self,
                 pull_requests,
                 unsuccessful_merge_index=-1,
                 create_pr_fail_index=-1,
                 merged_index=-1):
        """Initializes a mock WPTGitHub.

        Args:
            pull_requests: A list of wpt_github.PullRequest.
            unsuccessful_merge_index: The index to the PR in pull_requests that
                cannot be merged. (-1 means all can be merged.)
            create_pr_fail_index: The 0-based index of which PR creation request
                will fail. (-1 means all will succeed.)
            merged_index: The index to the PR in pull_requests that is already
                merged. (-1 means none is merged.)
        """
        self.pull_requests = pull_requests
        self.recent_failing_pull_requests = []
        self.calls = []
        self.pull_requests_created = []
        self.pull_requests_merged = []
        self.unsuccessful_merge_index = unsuccessful_merge_index
        self.create_pr_index = 0
        self.create_pr_fail_index = create_pr_fail_index
        self.merged_index = merged_index
        self.check_runs = []
        self.skipped_revisions = ['77578ccb4082ae20a9326d9e673225f1189ebb63']
        self.url = 'https://github.com/web-platform-tests/wpt/'
        self.provisional_pr_label = 'do not merge yet'
        self.export_pr_label = 'chromium-export'

    def all_provisional_pull_requests(self):
        self.calls.append('all_provisional_pull_requests')
        return self.pull_requests

    def all_pull_requests(self, limit=30):
        self.calls.append('all_pull_requests')
        return self.pull_requests

    def recent_failing_chromium_exports(self):
        self.calls.append("recent_failing_chromium_exports")
        return self.recent_failing_pull_requests

    def is_pr_merged(self, number):
        for index, pr in enumerate(self.pull_requests):
            if pr.number == number:
                return index == self.merged_index
        return False

    def merge_pr(self, number):
        self.calls.append('merge_pr')

        for index, pr in enumerate(self.pull_requests):
            if pr.number == number and index == self.unsuccessful_merge_index:
                raise MergeError(number)

        self.pull_requests_merged.append(number)

    def create_pr(self, remote_branch_name, desc_title, body):
        self.calls.append('create_pr')

        if self.create_pr_fail_index != self.create_pr_index:
            self.pull_requests_created.append((remote_branch_name, desc_title,
                                               body))

        pr_number = 5678 + self.create_pr_index
        self.create_pr_index += 1
        return pr_number

    def update_pr(self, pr_number, desc_title=None, body=None, state=None):
        self.calls.append('update_pr')
        return 5678

    def delete_remote_branch(self, _):
        self.calls.append('delete_remote_branch')

    def add_label(self, _, label):
        self.calls.append('add_label "%s"' % label)

    def remove_label(self, _, label):
        self.calls.append('remove_label "%s"' % label)

    def add_comment(self, _, comment_body):
        self.calls.append('add_comment "%s"' % comment_body)

    def get_pr_branch(self, number):
        self.calls.append('get_pr_branch')
        return 'fake_branch_PR_%d' % number

    def get_branch_check_runs(self, remote_branch_name):
        self.calls.append('get_branch_check_runs')
        return self.check_runs

    def pr_for_chromium_commit(self, commit):
        self.calls.append('pr_for_chromium_commit')
        for pr in self.pull_requests:
            if commit.change_id() in pr.body:
                return pr
        return None

    def pr_with_change_id(self, change_id):
        self.calls.append('pr_with_change_id')
        for pr in self.pull_requests:
            if change_id in pr.body:
                return pr
        return None

    def extract_metadata(self, tag, commit_body, all_matches=False):
        return WPTGitHub.extract_metadata(tag, commit_body, all_matches)
