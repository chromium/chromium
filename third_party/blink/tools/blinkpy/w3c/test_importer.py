# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fetches a copy of the latest state of a W3C test repository and commits.

If this script is given the argument --auto-update, it will also:
 1. Upload a CL.
 2. Trigger try jobs and wait for them to complete.
 3. Make any changes that are required for new failing tests.
 4. Attempt to land the CL.
"""

import argparse
import collections
import contextlib
import itertools
import json
import logging
import textwrap
from functools import cached_property
from typing import List, Mapping, Optional, Set

from blinkpy.common.checkout.git import CommitRange
from blinkpy.common.net.git_cl import (
    BuildStatus,
    CLRevisionID,
    CLStatus,
    GitCL,
)
from blinkpy.common.net.network_transaction import NetworkTimeout
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.buganizer import BuganizerClient, BuganizerIssue
from blinkpy.w3c.chromium_commit import ChromiumCommit
from blinkpy.w3c.chromium_exportable_commits import exportable_commits_over_last_n_commits
from blinkpy.w3c.common import (
    read_credentials,
    is_file_exportable,
    WPT_GH_URL,
    WPT_GH_RANGE_URL_TEMPLATE,
)
from blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from blinkpy.w3c.gerrit import GerritAPI
from blinkpy.w3c.import_notifier import ImportNotifier
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.test_copier import TestCopier
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.w3c.wpt_github import WPTGitHub
from blinkpy.w3c.wpt_manifest import WPTManifest, BASE_MANIFEST_NAME
from blinkpy.web_tests.models import typ_types
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.port.base import Port

# Settings for how often to check try job results and how long to wait.
POLL_DELAY_SECONDS = 2 * 60
TIMEOUT_SECONDS = 210 * 60

# Sheriff calendar URL, used for getting the ecosystem infra sheriff to cc.
ROTATIONS_URL = 'https://chrome-ops-rotation-proxy.appspot.com/current/grotation:chromium-wpt-two-way-sync'
SHERIFF_EMAIL_FALLBACK = 'weizhong@google.com'
RUBBER_STAMPER_BOT = 'rubber-stamper@appspot.gserviceaccount.com'

_log = logging.getLogger(__file__)


class TestImporter:

    def __init__(self,
                 host,
                 github=None,
                 wpt_manifests=None,
                 buganizer_client: Optional[BuganizerClient] = None):
        self.host = host
        self.github = github

        self.executive = host.executive
        self.fs = host.filesystem
        self.finder = PathFinder(self.fs)
        self.project_git = self.host.git(self.host.project_config.project_root)
        self.dest_path = self.finder.path_from_web_tests('external', 'wpt')

        # A common.net.git_cl.GitCL instance.
        self.git_cl = None
        # Another Git instance with local WPT as CWD, which can only be
        # instantiated after the working directory is created.
        self.wpt_git = None
        self.verbose = False
        self.wpt_manifests = wpt_manifests
        self._buganizer_client = buganizer_client or BuganizerClient()
        self._cleanup = contextlib.ExitStack()

    def __enter__(self):
        return self._cleanup.__enter__()

    def __exit__(self, exc_type, exc, tb):
        return self._cleanup.__exit__(exc_type, exc, tb)

    @cached_property
    def expectations_updater(self):
        args = ['--clean-up-affected-tests-only',
                '--clean-up-test-expectations']
        return WPTExpectationsUpdater(
            self.host, args, self.wpt_manifests)

    def main(self, argv=None):
        # TODO(robertma): Test this method! Split it to make it easier to test
        # if necessary.

        options = self.parse_args(argv)

        self.verbose = options.verbose
        log_level = logging.DEBUG if self.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        # Having the full output when executive.run_command fails is useful when
        # investigating a failed import, as all we have are logs.
        self.executive.error_output_limit = None

        if options.auto_update and options.auto_upload:
            _log.error(
                '--auto-upload and --auto-update cannot be used together.')
            return 1

        if not self.checkout_is_okay():
            return 1

        credentials = read_credentials(self.host, options.credentials_json)
        gh_user = credentials.get('GH_USER')
        gh_token = credentials.get('GH_TOKEN')
        if not gh_user or not gh_token:
            _log.warning('You have not set your GitHub credentials. This '
                         'script may fail with a network error when making '
                         'an API request to GitHub.')
            _log.warning('See https://chromium.googlesource.com/chromium/src'
                         '/+/main/docs/testing/web_platform_tests.md'
                         '#GitHub-credentials for instructions on how to set '
                         'your credentials up.')
        self.github = self.github or WPTGitHub(self.host, gh_user, gh_token)
        self.git_cl = GitCL(self.host)

        _log.debug('Noting the current Chromium revision.')
        chromium_revision = self.project_git.latest_git_commit()

        # Instantiate Git after local_wpt.fetch() to make sure the path exists.
        local_wpt = LocalWPT(self.host, gh_token=gh_token)
        local_wpt.fetch()
        self.wpt_git = self.host.git(local_wpt.path)

        # File bugs for the previous imported CL. This is done at the start so
        # that manually revived CLs still receive bugs.
        gerrit_api = GerritAPI.from_credentials(self.host, credentials)
        notifier = ImportNotifier(self.host, self.project_git, local_wpt,
                                  gerrit_api, self._buganizer_client)
        self.file_and_record_bugs(notifier,
                                  auto_file_bugs=options.auto_file_bugs)

        if options.revision is not None:
            _log.info('Checking out %s', options.revision)
            self.wpt_git.run(['checkout', options.revision])

        new_wpt_revision = self.wpt_git.latest_git_commit()
        _log.info('Importing wpt@%s to Chromium %s', new_wpt_revision,
                  chromium_revision)

        if options.ignore_exportable_commits:
            commits = []
        else:
            commits = self.apply_exportable_commits_locally(local_wpt)
            if commits is None:
                _log.error('Could not apply some exportable commits cleanly.')
                _log.error('Aborting import to prevent clobbering commits.')
                return 1
        last_wpt_revision, _ = notifier.latest_wpt_import()
        wpt_range = CommitRange(last_wpt_revision, new_wpt_revision)
        commit_message = self.commit_message(chromium_revision,
                                             wpt_range,
                                             locally_applied_commits=commits)

        self._clear_out_dest_path()

        _log.info('Copying the tests from the temp repo to the destination.')
        test_copier = TestCopier(self.host, local_wpt.path)
        test_copier.do_import()

        # TODO(robertma): Implement `add --all` in Git (it is different from `commit --all`).
        self.project_git.run(['add', '--all', self.dest_path])

        # Remove expectations for tests that were deleted and rename tests in
        # expectations for renamed tests. This requires the old WPT manifest, so
        # must happen before we regenerate it.
        self.expectations_updater.cleanup_test_expectations_files()
        self._generate_manifest()
        self.delete_orphaned_baselines()

        if not self.project_git.has_working_directory_changes():
            _log.info('Done: no changes to import.')
            return 0

        if not self._has_wpt_changes():
            _log.info('Only manifest or expectations was updated; skipping the import.')
            return 0
        testlist_path = self.finder.path_from_web_tests(
            "TestLists", "android.filter")
        _log.info('Updating testlist based on file changes.')
        self.update_testlist_with_idlharness_changes(testlist_path)

        self._commit_changes(commit_message)
        _log.info('Changes imported and committed.')

        if not options.auto_upload and not options.auto_update:
            return 0

        directory_owners = self.get_directory_owners()
        description = self.cl_description(directory_owners)
        import_issue_num = self._upload_cl(description)
        self._cleanup.callback(self._ensure_cl_closed, import_issue_num)

        if not self.update_expectations_for_cl():
            return 1
        if not options.auto_update:
            return 0
        if not self.run_commit_queue_for_cl():
            return 1
        return 0

    def _ensure_cl_closed(self, issue: Optional[int] = None):
        if self.git_cl.get_cl_status(issue) is not CLStatus.CLOSED:
            self.git_cl.close(issue)

    def log_try_job_results(self, try_job_results) -> None:
        if try_job_results:
            _log.info('Failing builder results:')
            for builder, try_job_status in try_job_results.items():
                if try_job_status is not BuildStatus.SUCCESS:
                    _log.info(f'{builder}: {try_job_status}')

    def update_expectations_for_cl(self) -> bool:
        """Performs the expectation-updating part of an auto-import job.

        This includes triggering try jobs and waiting; then, if applicable,
        writing new baselines, metadata, and TestExpectation lines, committing,
        and uploading a new patchset.

        This assumes that there is CL associated with the current branch.

        Returns True if everything is OK to continue, or False on failure.
        """
        self._trigger_try_jobs()
        cl_status = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS,
            timeout_seconds=TIMEOUT_SECONDS)

        if not cl_status:
            _log.error('No initial try job results, aborting.')
            issue_number = self.git_cl.get_issue_number()
            try_job_results = self.git_cl.latest_try_jobs(issue_number,
                                                          cq_only=False)
            self.log_try_job_results(try_job_results)
            return False

        if cl_status.status is CLStatus.CLOSED:
            _log.error('The CL was closed, aborting.')
            return False

        _log.info('All jobs finished.')
        try_results = cl_status.try_job_results

        if try_results and self.git_cl.some_failed(try_results):
            self.fetch_new_expectations_and_baselines()
            # Skip slow and timeout tests so that presubmit check passes
            port = self.host.port_factory.get()
            if self.expectations_updater.skip_slow_timeout_tests(port):
                path = port.path_to_generic_test_expectations_file()
                self.project_git.add_list([path])

            self._generate_manifest()
            message = 'Update test expectations and baselines.'
            if self.project_git.has_working_directory_changes():
                self._commit_changes(message)
            # Even if we didn't commit anything here, we may still upload
            # `TestExpectations`, which are committed earlier (before
            # rebaselining).
            self._upload_patchset(message)
        return True

    def _trigger_try_jobs(self):
        builders = self.host.builders.builders_for_rebaselining()
        _log.info('Triggering try jobs for updating expectations:')
        for builder in sorted(builders):
            _log.info(f'  {builder}')
        self.git_cl.trigger_try_jobs(builders)

    def run_commit_queue_for_cl(self):
        """Triggers CQ and either commits or aborts; returns True on success."""
        _log.info('Triggering CQ try jobs.')
        self.git_cl.run(['try'])
        cl_status = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS,
            timeout_seconds=TIMEOUT_SECONDS,
            cq_only=True)

        if not cl_status:
            _log.error('Timed out waiting for CQ; aborting.')
            return False

        if cl_status.status is CLStatus.CLOSED:
            _log.error('The CL was closed; aborting.')
            return False

        _log.info('All jobs finished.')
        cq_try_results = cl_status.try_job_results

        if not cq_try_results:
            _log.error('No CQ try results found in try results')
            return False

        if not self.git_cl.all_success(cq_try_results):
            _log.error('CQ appears to have failed; aborting.')
            return False

        # `--send-mail` is required to take the CL out of WIP mode.
        if self._need_sheriff_attention():
            _log.info(
                'CQ appears to have passed; sending to the sheriff for '
                'CR+1 and commit. The sheriff has one hour to respond.')
            self.git_cl.run([
                'upload', '-f', '--send-mail', '--enable-auto-submit',
                '--cc', self.sheriff_email()
            ])
            timeout = 3600
        else:
            _log.info(
                'CQ appears to have passed; sending to the rubber-stamper bot for '
                'CR+1 and commit.')
            _log.info(
                'If the rubber-stamper bot rejects the CL, you either need to '
                'modify the benign file patterns, or manually CR+1 and land the '
                'import yourself if it touches code files. See https://chromium.'
                'googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/'
                'appengine/rubber-stamper/README.md')
            self.git_cl.run([
                'upload', '-f', '--send-mail', '--enable-auto-submit',
                '--reviewers', RUBBER_STAMPER_BOT
            ])
            # Some internal builders (e.g., `win-branded-compile-rel`) only run
            # on CQ+2 without reusing a CQ+1 build. Use a 1h timeout as well to
            # accommodate them.
            timeout = 3600

        if self.git_cl.wait_for_closed_status(timeout_seconds=timeout):
            _log.info('Update completed.')
            return True

        _log.error('Cannot submit CL; aborting.')
        return False

    def parse_args(self, argv):
        parser = argparse.ArgumentParser()
        parser.description = __doc__
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='log extra details that may be helpful when debugging')
        parser.add_argument(
            '--ignore-exportable-commits',
            action='store_true',
            help='do not check for exportable commits that would be clobbered')
        parser.add_argument('-r', '--revision', help='target wpt revision')
        parser.add_argument(
            '--auto-upload',
            action='store_true',
            help='upload a CL, update expectations, but do NOT trigger CQ')
        parser.add_argument(
            '--auto-update',
            action='store_true',
            help='upload a CL, update expectations, and trigger CQ')
        parser.add_argument(
            '--auto-file-bugs',
            action='store_true',
            help='file new failures automatically to crbug.com')
        parser.add_argument(
            '--credentials-json',
            help='A JSON file with GitHub credentials, '
            'generally not necessary on developer machines')

        return parser.parse_args(argv)

    def checkout_is_okay(self):
        if self.project_git.has_working_directory_changes():
            _log.warning('Checkout is dirty; aborting.')
            return False
        # TODO(robertma): Add a method in Git to query a range of commits.
        local_commits = self.project_git.run(
            ['log', '--oneline', 'origin/main..HEAD'])
        if local_commits:
            _log.warning('Checkout has local commits before import.')
        return True

    def apply_exportable_commits_locally(self, local_wpt):
        """Applies exportable Chromium changes to the local WPT repo.

        The purpose of this is to avoid clobbering changes that were made in
        Chromium but not yet merged upstream. By applying these changes to the
        local copy of web-platform-tests before copying files over, we make
        it so that the resulting change in Chromium doesn't undo the
        previous Chromium change.

        Args:
            A LocalWPT instance for our local copy of WPT.

        Returns:
            A list of commits applied (could be empty), or None if any
            of the patches could not be applied cleanly.
        """
        commits = self.exportable_but_not_exported_commits(local_wpt)
        for commit in commits:
            _log.info('Applying exportable commit locally:')
            _log.info(commit.url())
            _log.info('Subject: %s', commit.subject().strip())
            # Log a note about the corresponding PR.
            # This might not be necessary, and could potentially be removed.
            pull_request = self.github.pr_for_chromium_commit(commit)
            if pull_request:
                _log.info('PR: %spull/%d', WPT_GH_URL, pull_request.number)
            else:
                _log.warning('No pull request found.')
            error = local_wpt.apply_patch(commit.format_patch())
            if error:
                _log.error('Commit cannot be applied cleanly:')
                _log.error(error)
                return None
            self.wpt_git.commit_locally_with_message(
                'Applying patch %s' % commit.sha)
        return commits

    def exportable_but_not_exported_commits(self, local_wpt):
        """Returns a list of commits that would be clobbered by importer.

        The list contains all exportable but not exported commits, not filtered
        by whether they can apply cleanly.
        """
        # The errors returned by exportable_commits_over_last_n_commits are
        # irrelevant and ignored here, because it tests patches *individually*
        # while the importer tries to reapply these patches *cumulatively*.
        commits, _ = exportable_commits_over_last_n_commits(
            self.host,
            local_wpt,
            self.github,
            require_clean=False,
            verify_merged_pr=True)
        return commits

    def _generate_manifest(self):
        """Generates MANIFEST.json for imported tests.

        Runs the (newly-updated) manifest command if it's found, and then
        stages the generated MANIFEST.json in the git index, ready to commit.
        """
        _log.info('Generating MANIFEST.json')
        WPTManifest.generate_manifest(self.host.port_factory.get(),
                                      self.dest_path)
        manifest_path = self.fs.join(self.dest_path, 'MANIFEST.json')
        assert self.fs.exists(manifest_path)
        manifest_base_path = self.fs.normpath(
            self.fs.join(self.dest_path, '..', BASE_MANIFEST_NAME))
        self.copyfile(manifest_path, manifest_base_path)
        self.project_git.add_list([manifest_base_path])

    def _clear_out_dest_path(self):
        """Removes all files that are synced with upstream from Chromium WPT.

        Instead of relying on TestCopier to overwrite these files, cleaning up
        first ensures if upstream deletes some files, we also delete them.
        """
        _log.info('Cleaning out tests from %s.', self.dest_path)
        should_remove = lambda fs, dirname, basename: (is_file_exportable(
            fs.relpath(fs.join(dirname, basename), self.finder.chromium_base()
                       ), self.host.project_config))
        files_to_delete = self.fs.files_under(
            self.dest_path, file_filter=should_remove)
        for subpath in files_to_delete:
            self.remove(self.finder.path_from_web_tests('external', subpath))

    def _commit_changes(self, commit_message):
        _log.info('Committing changes.')
        self.project_git.commit_locally_with_message(commit_message)

    def _has_wpt_changes(self):
        changed_files = self.project_git.changed_files()
        test_roots = [
            self.fs.relpath(self.finder.path_from_web_tests(subdir),
                            self.finder.chromium_base())
            for subdir in Port.WPT_DIRS
        ]
        for changed_file in changed_files:
            if any(changed_file.startswith(root) for root in test_roots):
                return True
        return False

    def _need_sheriff_attention(self):
        # Per the rules defined for the rubber-stamper, it can not auto approve
        # a CL that has .bat, .sh or .py files. Request the sheriff on rotation
        # to approve the CL.
        changed_files = self.project_git.changed_files()
        for cf in changed_files:
            extension = self.fs.splitext(cf)[1]
            if extension in ['.bat', '.sh', '.py']:
                return True
        return False

    def commit_message(
        self,
        chromium_commit_sha: str,
        wpt_range: CommitRange,
        locally_applied_commits: Optional[List[ChromiumCommit]] = None,
    ) -> str:
        wpt_short_range = CommitRange(wpt_range.start[:9], wpt_range.end[:9])
        message = textwrap.dedent(f"""\
            {ImportNotifier.IMPORT_SUBJECT_PREFIX}{wpt_range.end}

            {WPT_GH_RANGE_URL_TEMPLATE.format(*wpt_short_range)}

            Using wpt-import in Chromium {chromium_commit_sha}.
            """)
        if locally_applied_commits:
            message += 'With Chromium commits locally applied on WPT:\n'
            message += '\n'.join(f'  {textwrap.shorten(str(commit), 70)}'
                                 for commit in locally_applied_commits) + '\n'
        return message

    def delete_orphaned_baselines(self):
        """Delete baselines that don't correspond to any external WPTs.

        Notes:
          * This method should be called after importing the new tests and
            regenerating the manifest.
          * There's no need to handle renames explicitly because any failures
            in the renamed tests will be rebaselined later.
        """
        port = self.host.port_factory.get()

        # Find which baselines should be deleted for each deleted test. Because
        # baseline paths are lossily sanitized, it's not easy to determine what
        # test corresponds to a baseline path. Therefore, this map is keyed on
        # the generic baseline as a proxy for the test URL instead.
        baselines_by_generic_path = collections.defaultdict(set)
        baseline_glob = self.fs.join(port.web_tests_dir(), '**', 'external',
                                     'wpt', '**', '*-expected.txt')
        for baseline in self.fs.glob(baseline_glob):
            _, generic_baseline = port.parse_output_filename(baseline)
            baselines_by_generic_path[generic_baseline].add(baseline)

        # Note about possible refactoring:
        #  - the manifest path could be factored out to a common location, and
        #  - the logic for reading the manifest could be factored out from here
        # and the Port class.
        manifest_path = self.finder.path_from_wpt_tests('MANIFEST.json')
        # Exclude test types `(print-)reftest` and `crashtest`, which can't
        # have `*-expected.txt`.
        manifest = WPTManifest.from_file(port, manifest_path,
                                         ['testharness', 'wdspec', 'manual'])
        for url_from_test_root in manifest.all_urls():
            test = self.finder.wpt_prefix() + url_from_test_root
            generic_baseline = port.output_filename(test, Port.BASELINE_SUFFIX,
                                                    '.txt')
            baselines_by_generic_path.pop(generic_baseline, None)

        orphan_count = 0
        for baseline_to_delete in itertools.chain.from_iterable(
                baselines_by_generic_path.values()):
            self.remove(baseline_to_delete)
            orphan_count += 1
        _log.info(f'Deleted {orphan_count} orphaned baseline(s).')

    def copyfile(self, source, destination):
        _log.debug('cp %s %s', source, destination)
        self.fs.copyfile(source, destination)

    def remove(self, dest):
        _log.debug('rm %s', dest)
        self.fs.remove(dest)

    def _upload_patchset(self, message):
        self.git_cl.run(['upload', '--bypass-hooks', '-f', '-t', message])

    def _upload_cl(self,
                   description: str,
                   extra_args: Optional[List[str]] = None) -> int:
        """Upload a CL on the current branch and return its issue number."""
        _log.info('Uploading change list.')
        self.git_cl.run([
            'upload',
            '--bypass-hooks',
            '-f',
            f'--message={description}',
            *(extra_args or []),
        ])
        issue_num = int(self.git_cl.get_issue_number())
        _log.info(f'Issue: {CLRevisionID(issue_num)}')
        return issue_num

    def get_directory_owners(self):
        """Returns a mapping of email addresses to owners of changed tests."""
        _log.info('Gathering directory owners emails to CC.')
        changed_files = self.project_git.changed_files()
        extractor = DirectoryOwnersExtractor(self.host)
        return extractor.list_owners(changed_files)

    def cl_description(self, directory_owners):
        """Returns a CL description string.

        Args:
            directory_owners: A dict of tuples of owner names to lists of directories.
        """
        # TODO(robertma): Add a method in Git for getting the commit body.
        description = self.project_git.run(['log', '-1', '--format=%B'])
        gardener_instructions = textwrap.dedent("""\
            Note to gardeners: This CL imports external tests and adds
            expectations for those tests; if this CL is large and causes a few
            new failures, please fix the failures by adding new lines to
            TestExpectations rather than reverting. See:
            """)
        description += '\n'.join([
            *textwrap.wrap(gardener_instructions, width=72),
            'https://chromium.googlesource.com/chromium/src/+/'
            'main/docs/testing/web_platform_tests.md',
        ]) + '\n\n'

        if directory_owners:
            description += self._format_directory_owners(
                directory_owners) + '\n\n'

        # Prevent FindIt from auto-reverting import CLs.
        description += 'NOAUTOREVERT=true\n'
        description += 'No-Export: true\n'

        # If this starts blocking the importer unnecessarily, revert
        # https://chromium-review.googlesource.com/c/chromium/src/+/2451504
        # Try linux-blink-rel to make sure no breakage in webdriver tests
        for builder in ['linux-blink-rel']:
            description += f'Cq-Include-Trybots: luci.chromium.try:{builder}\n'

        return description

    @staticmethod
    def _format_directory_owners(directory_owners):
        message_lines = ['Directory owners for changes in this CL:']
        for owner_tuple, directories in sorted(directory_owners.items()):
            message_lines.append(', '.join(owner_tuple) + ':')
            message_lines.extend('  ' + d for d in directories)
        return '\n'.join(message_lines)

    def sheriff_email(self):
        """Returns the sheriff email address to cc.

        This tries to fetch the current ecosystem infra sheriff, but falls back
        in case of error.
        """
        email = ''
        try:
            email = self._fetch_ecosystem_infra_sheriff_email()
        except (IOError, KeyError, ValueError) as error:
            _log.error('Exception while fetching current sheriff: %s', error)
        return email or SHERIFF_EMAIL_FALLBACK

    def _fetch_ecosystem_infra_sheriff_email(self):
        try:
            content = self.host.web.get_binary(ROTATIONS_URL)
        except NetworkTimeout:
            _log.error('Cannot fetch %s', ROTATIONS_URL)
            return ''
        data = json.loads(content)
        if not data.get('emails'):
            _log.error(
                'No email found for current sheriff. Retrieved content: %s',
                content)
            return ''
        return data['emails'][0]

    def fetch_new_expectations_and_baselines(self):
        """Modifies expectation lines and baselines based on try job results.

        Assuming that there are some try job results available, this
        adds new expectation lines to TestExpectations and downloads new
        baselines based on the try job results.
        """
        tests_to_rebaseline, _ = (
            self.expectations_updater.update_expectations())
        # commit local changes so that rebaseline tool will be happy
        if self.project_git.has_working_directory_changes():
            message = 'Update test expectations'
            self._commit_changes(message)
        self.expectations_updater.download_text_baselines(
            list(tests_to_rebaseline))

    def file_and_record_bugs(
        self,
        notifier: ImportNotifier,
        auto_file_bugs: bool = True,
    ):
        """File bugs for the last imported CL and add them to TestExpectations.

        Notes:
            The starting commit log should look like:
              (HEAD -> update_wpt, origin/main) <tip-of-tree>

            where `update_wpt` was created by the `wpt_import.py` recipe. If
            this method creates a fixup CL, the commit log afterwards should
            look like:
              (HEAD -> wpt-import, update_wpt) [wpt-import] Update `TestExpectations` ...
              (origin/main) <tip-of-tree>
        """
        _log.info('Filing bugs for the last WPT import.')
        bugs_by_dir, cl = notifier.main(dry_run=(not auto_file_bugs))
        referenced_bugs = self._update_bugs_in_expectations(
            notifier, bugs_by_dir)

        if self.project_git.has_working_directory_changes():
            self.project_git.add_list([self.finder.path_from_web_tests()])
            description = textwrap.dedent(f"""\
                Update `TestExpectations` with bugs filed for crrev.com/c/{cl.number}

                Bug: {', '.join(map(str, sorted(referenced_bugs)))}
                """)
            self._commit_changes(description)
            assert not self.project_git.has_working_directory_changes()
            # Immediately send the fixup CL to CQ+2. The chained CQ votes
            # feature of CV should automatically rebase the `Import wpt@...`
            # CL on tip-of-tree after the fixup CL lands.
            fixup_cl_issue = self._upload_cl(description, [
                '--send-mail',
                '--enable-auto-submit',
                f'--reviewers={RUBBER_STAMPER_BOT}',
            ])
            # Get back on an issue-less branch for the `Import wpt@...` CL.
            self.project_git.new_branch('import-wpt')
            self._cleanup.callback(self._notify_if_cl_blocked, referenced_bugs,
                                   fixup_cl_issue, self.host.time())

        diff_from_tracking = self.project_git.changed_files(
            CommitRange('@{u}', 'HEAD'))
        assert not diff_from_tracking, diff_from_tracking

    def _notify_if_cl_blocked(self, referenced_bugs: Set[int], issue: int,
                              start: float):
        assert referenced_bugs
        # If both CL types were created, it's likely this timeout has already
        # elapsed (i.e., this will only block if only the bug fixup CL was
        # created).
        if self.git_cl.wait_for_closed_status(POLL_DELAY_SECONDS,
                                              TIMEOUT_SECONDS, issue, start):
            return

        bug_links = {
            f'https://crbug.com/{issue_id}'
            for issue_id in sorted(referenced_bugs)
        }
        _log.warning(f'Failed to automatically submit {CLRevisionID(issue)}. '
                     f'Pinging {", ".join(bug_links)} for help.')
        comment = (
            f'{CLRevisionID(issue)} backfills TestExpectations to reference '
            'this bug, but that CL failed to land automatically. Please try '
            'resubmitting. You may need to rebase that CL on tip-of-tree and '
            'resolve any resulting merge conflicts.')
        for issue_id in referenced_bugs:
            self._buganizer_client.NewComment(issue_id, comment)
        self._ensure_cl_closed(issue)

    def _update_bugs_in_expectations(
        self,
        notifier: ImportNotifier,
        bugs_by_dir: Mapping[str, BuganizerIssue],
    ) -> Set[int]:
        expectations = TestExpectations(
            notifier.default_port,
            notifier.default_port.all_expectations_dict())
        referenced_bugs = set()
        for directory, bug in bugs_by_dir.items():
            assert bug.issue_id, bug
            failures = notifier.new_failures_by_directory[directory]
            for path_from_root, target_lines in failures.exp_by_file.items():
                for target_line in target_lines:
                    path = self.finder.path_from_chromium_base(path_from_root)
                    # It's possible that the expectation added previously no
                    # longer exists in its current form (e.g., might be
                    # consolidated with similar lines).
                    if self._update_bug_in_exp_line(expectations, bug, path,
                                                    target_line):
                        referenced_bugs.add(bug.issue_id)

        # Only serialize to the filesystem if a line has been updated.
        # Otherwise, `commit_changes()` may reformat the file and dirty the
        # checkout, even without meaningful changes to upload. Examples of
        # reformatting include whitespace modifications and sorting tag or
        # result lists.
        if referenced_bugs:
            expectations.commit_changes()
        return referenced_bugs

    def _update_bug_in_exp_line(
        self,
        expectations: TestExpectations,
        bug: BuganizerIssue,
        path: str,
        target_line: typ_types.ExpectationType,
    ) -> Optional[typ_types.ExpectationType]:
        """Add a bug for a matching line, if any, in a given file.

        Returns:
            The new line if the update was done. The filed bug will not replace
            an existing bug.
        """
        for current_line in expectations.get_expectations_from_file(
                path, target_line.test):
            if (current_line.tags != target_line.tags
                    or current_line.results != target_line.results):
                continue
            if current_line.reason.strip():
                continue
            # Avoid `bug.link`, which includes `https://...` prefix.
            new_line = typ_types.Expectation(f'crbug.com/{bug.issue_id}',
                                             current_line.test,
                                             current_line.tags,
                                             current_line.results,
                                             current_line.lineno)
            expectations.remove_expectations(path, [current_line])
            expectations.add_expectations(path, [new_line], new_line.lineno)
            # There can't be another expectation with the same tags, results,
            # and test name for this file.
            return new_line
        return None

    def update_testlist_with_idlharness_changes(self, testlist_path):
        """Update testlist file to include idlharness test changes
        """
        added_files = self.project_git.added_files()
        deleted_files = self.project_git.deleted_files()

        # extract test name and filter for idlharness tests from file list
        added_tests = list(
            filter(Port.is_wpt_idlharness_test,
                   map(self.finder.strip_web_tests_path, added_files)))
        deleted_tests = list(
            filter(Port.is_wpt_idlharness_test,
                   map(self.finder.strip_web_tests_path, deleted_files)))

        if added_files or deleted_files:
            _log.info('Idlharness test changes:')
            _log.info("Added tests:\n" + "\n".join(added_tests))
            _log.info("Deleted tests:\n" + "\n".join(deleted_tests))
        else:
            _log.info(f'No idlharness changes. Skipping testlist update.')

        with self.fs.open_text_file_for_reading(testlist_path) as f:
            testlist_lines = f.read().split("\n")

        new_testlist_lines = self.update_testlist_lines(
            testlist_lines, added_tests, deleted_tests)

        with self.fs.open_text_file_for_writing(testlist_path) as f:
            f.write("\n".join(new_testlist_lines))
        self.project_git.run(['add', testlist_path])

    def update_testlist_lines(self, testlist_lines, added_tests,
                              deleted_tests):
        """Updates the lines from testlist to remove deleted tests,
        and include the new tests"""
        new_testlist_lines = []
        for line in testlist_lines:
            current_test = line.strip()
            if current_test in deleted_tests:
                continue
            new_testlist_lines.append(line)
        last_insertion_index = 0
        # Pre-sort tests to be inserted
        for new_test in sorted(added_tests):
            insertion_index = self.find_insert_index_ignore_comments(
                new_testlist_lines, new_test, start_index=last_insertion_index)
            if (insertion_index < len(new_testlist_lines)
                    and new_testlist_lines[insertion_index] == new_test):
                _log.info(f'Skip duplicate test "{new_test}"')
                continue
            new_testlist_lines.insert(insertion_index, new_test)
            last_insertion_index = insertion_index
        return new_testlist_lines

    def find_insert_index_ignore_comments(self,
                                          targets_list,
                                          insert_key,
                                          start_index=0):
        """Finds index where the insert key should be added.
        The insert index points to the first item that is greater than
        the insert key and is not comment (start with #) or empty line"""
        last_insert_index = start_index
        for index, target in enumerate(targets_list[start_index:],
                                       start_index):
            if not target.strip() or target.startswith("#"):
                continue
            elif insert_key <= target:
                return index
            else:
                last_insert_index = index + 1
        return last_insert_index
