# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Contains all project-specific configuration for Chromium/WPT host."""

from blinkpy.common.path_finder import RELATIVE_WPT_TESTS
from blinkpy.w3c.chromium_finder import absolute_chromium_dir, absolute_chromium_wpt_dir
from blinkpy.w3c.common import WPT_REVISION_FOOTER
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.wpt_github import WPTGitHub


class ProjectConfig:
    def __init__(self, filesystem):
        self.filesystem = filesystem
        self.relative_tests_path = None
        self.revision_footer = None
        self.gerrit_project = None
        self.gerrit_branch = None
        self.github_factory = None
        self.local_repo_factory = None

    @property
    def project_root(self):
        pass

    @property
    def test_root(self):
        pass

    @property
    def pr_updated_comment_template(self):
        pass

    @property
    def inflight_cl_comment_template(self):
        pass


class ChromiumWPTConfig(ProjectConfig):
    def __init__(self, filesystem):
        super().__init__(filesystem)
        self.relative_tests_path = RELATIVE_WPT_TESTS
        self.revision_footer = WPT_REVISION_FOOTER
        self.gerrit_project = 'chromium/src'
        self.gerrit_branch = 'main'
        self.github_factory = WPTGitHub
        self.local_repo_factory = LocalWPT
        # TODO(crbug.com/1474702): Flip this to True when CQ/CI has switched to
        # using wptrunner.
        self.switched_to_wptrunner = False

    @property
    def project_root(self):
        return absolute_chromium_dir(self.filesystem)

    @property
    def test_root(self):
        return absolute_chromium_wpt_dir(self.filesystem)

    @property
    def pr_updated_comment_template(self):
        return ('Successfully updated WPT GitHub pull request with '
                'new revision "{subject}": {pr_url}')

    @property
    def inflight_cl_comment_template(self):
        return (
            'Exportable changes to web-platform-tests were detected in this CL '
            'and a pull request in the upstream repo has been made: {pr_url}.\n\n'
            'When this CL lands, the bot will automatically merge the PR '
            'on GitHub if the required GitHub checks pass; otherwise, '
            'ecosystem-infra@ team will triage the failures and may contact you.\n\n'
            'WPT Export docs:\n'
            'https://chromium.googlesource.com/chromium/src/+/main'
            '/docs/testing/web_platform_tests.md#Automatic-export-process')
