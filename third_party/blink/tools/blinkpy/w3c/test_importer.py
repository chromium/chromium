# Copyright 2014 The Chromium Authors. All rights reserved.
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
import datetime
import json
import logging
import re

from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.net.network_transaction import NetworkTimeout
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.w3c.chromium_exportable_commits import exportable_commits_over_last_n_commits
from blinkpy.w3c.common import read_credentials, is_testharness_baseline, is_file_exportable, WPT_GH_URL
from blinkpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor
from blinkpy.w3c.import_notifier import ImportNotifier
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.test_copier import TestCopier
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.w3c.wpt_github import WPTGitHub
from blinkpy.w3c.wpt_manifest import WPTManifest, BASE_MANIFEST_NAME
from blinkpy.web_tests.models.test_expectations import TestExpectations, TestExpectationParser
from blinkpy.web_tests.port.base import Port

# Settings for how often to check try job results and how long to wait.
POLL_DELAY_SECONDS = 2 * 60
TIMEOUT_SECONDS = 210 * 60

# Sheriff calendar URL, used for getting the ecosystem infra sheriff to TBR.
ROTATIONS_URL = 'https://rota-ng.appspot.com/legacy/all_rotations.js'
TBR_FALLBACK = 'robertma'

_log = logging.getLogger(__file__)


class TestImporter(object):

    def __init__(self, host, wpt_github=None):
        self.host = host
        self.wpt_github = wpt_github

        self.executive = host.executive
        self.fs = host.filesystem
        self.finder = PathFinder(self.fs)
        self.chromium_git = self.host.git(self.finder.chromium_base())
        self.dest_path = self.finder.path_from_web_tests('external', 'wpt')

        # A common.net.git_cl.GitCL instance.
        self.git_cl = None
        # Another Git instance with local WPT as CWD, which can only be
        # instantiated after the working directory is created.
        self.wpt_git = None
        # The WPT revision we are importing and the one imported last time.
        self.wpt_revision = None
        self.last_wpt_revision = None
        # A set of rebaselined tests and a dictionary of new test expectations
        # mapping failing tests to platforms to
        # wpt_expectations_updater.SimpleTestResult.
        self.rebaselined_tests = set()
        self.new_test_expectations = {}
        self.verbose = False

    def main(self, argv=None):
        # TODO(robertma): Test this method! Split it to make it easier to test
        # if necessary.

        options = self.parse_args(argv)

        self.verbose = options.verbose
        log_level = logging.DEBUG if self.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)
        if options.verbose:
            # Print out the full output when executive.run_command fails.
            self.host.executive.error_output_limit = None

        if options.auto_update and options.auto_upload:
            _log.error('--auto-upload and --auto-update cannot be used together.')
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
                         '/+/master/docs/testing/web_platform_tests.md'
                         '#GitHub-credentials for instructions on how to set '
                         'your credentials up.')
        self.wpt_github = self.wpt_github or WPTGitHub(self.host, gh_user, gh_token)
        self.git_cl = GitCL(self.host, auth_refresh_token_json=options.auth_refresh_token_json)

        _log.debug('Noting the current Chromium revision.')
        chromium_revision = self.chromium_git.latest_git_commit()

        # Instantiate Git after local_wpt.fetch() to make sure the path exists.
        local_wpt = LocalWPT(self.host, gh_token=gh_token)
        local_wpt.fetch()
        self.wpt_git = self.host.git(local_wpt.path)

        if options.revision is not None:
            _log.info('Checking out %s', options.revision)
            self.wpt_git.run(['checkout', options.revision])

        _log.debug('Noting the revision we are importing.')
        self.wpt_revision = self.wpt_git.latest_git_commit()
        self.last_wpt_revision = self._get_last_imported_wpt_revision()
        import_commit = 'wpt@%s' % self.wpt_revision

        _log.info('Importing %s to Chromium %s', import_commit, chromium_revision)

        if options.ignore_exportable_commits:
            commit_message = self._commit_message(chromium_revision, import_commit)
        else:
            commits = self.apply_exportable_commits_locally(local_wpt)
            if commits is None:
                _log.error('Could not apply some exportable commits cleanly.')
                _log.error('Aborting import to prevent clobbering commits.')
                return 1
            commit_message = self._commit_message(
                chromium_revision, import_commit, locally_applied_commits=commits)

        self._clear_out_dest_path()

        _log.info('Copying the tests from the temp repo to the destination.')
        test_copier = TestCopier(self.host, local_wpt.path)
        test_copier.do_import()

        # TODO(robertma): Implement `add --all` in Git (it is different from `commit --all`).
        self.chromium_git.run(['add', '--all', self.dest_path])

        self._generate_manifest()

        # TODO(crbug.com/800570 robertma): Re-enable it once we fix the bug.
        # self._delete_orphaned_baselines()

        _log.info('Updating TestExpectations for any removed or renamed tests.')
        self.update_all_test_expectations_files(self._list_deleted_tests(), self._list_renamed_tests())

        if not self.chromium_git.has_working_directory_changes():
            _log.info('Done: no changes to import.')
            return 0

        if self._only_wpt_manifest_changed():
            _log.info('Only manifest was updated; skipping the import.')
            return 0

        self._commit_changes(commit_message)
        _log.info('Changes imported and committed.')

        if not options.auto_upload and not options.auto_update:
            return 0

        self._upload_cl()
        _log.info('Issue: %s', self.git_cl.run(['issue']).strip())

        if not self.update_expectations_for_cl():
            return 1

        if not options.auto_update:
            return 0

        if not self.run_commit_queue_for_cl():
            return 1

        if not self.send_notifications(local_wpt, options.auto_file_bugs, options.monorail_auth_json):
            return 1

        return 0

    def update_expectations_for_cl(self):
        """Performs the expectation-updating part of an auto-import job.

        This includes triggering try jobs and waiting; then, if applicable,
        writing new baselines and TestExpectation lines, committing, and
        uploading a new patchset.

        This assumes that there is CL associated with the current branch.

        Returns True if everything is OK to continue, or False on failure.
        """
        _log.info('Triggering try jobs for updating expectations.')
        self.git_cl.trigger_try_jobs(self.blink_try_bots())
        cl_status = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS,
            timeout_seconds=TIMEOUT_SECONDS)

        if not cl_status:
            _log.error('No initial try job results, aborting.')
            self.git_cl.run(['set-close'])
            return False

        if cl_status.status == 'closed':
            _log.error('The CL was closed, aborting.')
            return False

        _log.info('All jobs finished.')
        try_results = cl_status.try_job_results

        if try_results and self.git_cl.some_failed(try_results):
            self.fetch_new_expectations_and_baselines()
            if self.chromium_git.has_working_directory_changes():
                self._generate_manifest()
                message = 'Update test expectations and baselines.'
                self._commit_changes(message)
                self._upload_patchset(message)
        return True

    def run_commit_queue_for_cl(self):
        """Triggers CQ and either commits or aborts; returns True on success."""
        _log.info('Triggering CQ try jobs.')
        self.git_cl.run(['try'])
        cl_status = self.git_cl.wait_for_try_jobs(
            poll_delay_seconds=POLL_DELAY_SECONDS,
            timeout_seconds=TIMEOUT_SECONDS,
            cq_only=True)

        if not cl_status:
            self.git_cl.run(['set-close'])
            _log.error('Timed out waiting for CQ; aborting.')
            return False

        if cl_status.status == 'closed':
            _log.error('The CL was closed; aborting.')
            return False

        _log.info('All jobs finished.')
        cq_try_results = cl_status.try_job_results

        if not cq_try_results:
            _log.error('No CQ try results found in try results')
            self.git_cl.run(['set-close'])
            return False

        if not self.git_cl.all_success(cq_try_results):
            _log.error('CQ appears to have failed; aborting.')
            self.git_cl.run(['set-close'])
            return False

        _log.info('CQ appears to have passed; trying to commit.')
        self.git_cl.run(['upload', '-f', '--send-mail'])  # Turn off WIP mode.
        self.git_cl.run(['set-commit'])

        if self.git_cl.wait_for_closed_status():
            _log.info('Update completed.')
            return True

        _log.error('Cannot submit CL; aborting.')
        try:
            self.git_cl.run(['set-close'])
        except ScriptError as e:
            if e.output and 'Conflict: change is merged' in e.output:
                _log.error('CL is already merged; treating as success.')
                return True
            else:
                raise e
        return False

    def blink_try_bots(self):
        """Returns the collection of builders used for updating expectations."""
        return self.host.builders.all_try_builder_names()

    def parse_args(self, argv):
        parser = argparse.ArgumentParser()
        parser.description = __doc__
        parser.add_argument(
            '-v', '--verbose', action='store_true',
            help='log extra details that may be helpful when debugging')
        parser.add_argument(
            '--ignore-exportable-commits', action='store_true',
            help='do not check for exportable commits that would be clobbered')
        parser.add_argument('-r', '--revision', help='target wpt revision')
        parser.add_argument(
            '--auto-upload', action='store_true',
            help='upload a CL, update expectations, but do NOT trigger CQ')
        parser.add_argument(
            '--auto-update', action='store_true',
            help='upload a CL, update expectations, and trigger CQ')
        parser.add_argument(
            '--auto-file-bugs', action='store_true',
            help='file new failures automatically to crbug.com')
        parser.add_argument(
            '--auth-refresh-token-json',
            help='authentication refresh token JSON file used for try jobs, '
                 'generally not necessary on developer machines')
        parser.add_argument(
            '--credentials-json',
            help='A JSON file with GitHub credentials, '
                 'generally not necessary on developer machines')
        parser.add_argument(
            '--monorail-auth-json',
            help='A JSON file containing the private key of a service account '
                 'to access Monorail (crbug.com), only needed when '
                 '--auto-file-bugs is used')

        return parser.parse_args(argv)

    def checkout_is_okay(self):
        if self.chromium_git.has_working_directory_changes():
            _log.warning('Checkout is dirty; aborting.')
            return False
        # TODO(robertma): Add a method in Git to query a range of commits.
        local_commits = self.chromium_git.run(['log', '--oneline', 'origin/master..HEAD'])
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
            pull_request = self.wpt_github.pr_for_chromium_commit(commit)
            if pull_request:
                _log.info('PR: %spull/%d', WPT_GH_URL, pull_request.number)
            else:
                _log.warning('No pull request found.')
            error = local_wpt.apply_patch(commit.format_patch())
            if error:
                _log.error('Commit cannot be applied cleanly:')
                _log.error(error)
                return None
            self.wpt_git.commit_locally_with_message('Applying patch %s' % commit.sha)
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
            self.host, local_wpt, self.wpt_github, require_clean=False, verify_merged_pr=True)
        return commits

    def _generate_manifest(self):
        """Generates MANIFEST.json for imported tests.

        Runs the (newly-updated) manifest command if it's found, and then
        stages the generated MANIFEST.json in the git index, ready to commit.
        """
        _log.info('Generating MANIFEST.json')
        WPTManifest.generate_manifest(self.host, self.dest_path)
        manifest_path = self.fs.join(self.dest_path, 'MANIFEST.json')
        assert self.fs.exists(manifest_path)
        manifest_base_path = self.fs.normpath(
            self.fs.join(self.dest_path, '..', BASE_MANIFEST_NAME))
        self.copyfile(manifest_path, manifest_base_path)
        self.chromium_git.add_list([manifest_base_path])

    def _clear_out_dest_path(self):
        """Removes all files that are synced with upstream from Chromium WPT.

        Instead of relying on TestCopier to overwrite these files, cleaning up
        first ensures if upstream deletes some files, we also delete them.
        """
        _log.info('Cleaning out tests from %s.', self.dest_path)
        should_remove = lambda fs, dirname, basename: (
            is_file_exportable(fs.relpath(fs.join(dirname, basename), self.finder.chromium_base())))
        files_to_delete = self.fs.files_under(self.dest_path, file_filter=should_remove)
        for subpath in files_to_delete:
            self.remove(self.finder.path_from_web_tests('external', subpath))

    def _commit_changes(self, commit_message):
        _log.info('Committing changes.')
        self.chromium_git.commit_locally_with_message(commit_message)

    def _only_wpt_manifest_changed(self):
        changed_files = self.chromium_git.changed_files()
        wpt_base_manifest = self.fs.relpath(
            self.fs.join(self.dest_path, '..', BASE_MANIFEST_NAME),
            self.finder.chromium_base())
        return changed_files == [wpt_base_manifest]

    def _commit_message(self, chromium_commit_sha, import_commit_sha,
                        locally_applied_commits=None):
        message = 'Import {}\n\nUsing wpt-import in Chromium {}.\n'.format(
            import_commit_sha, chromium_commit_sha)
        if locally_applied_commits:
            message += 'With Chromium commits locally applied on WPT:\n'
            message += '\n'.join(str(commit) for commit in locally_applied_commits)
        message += '\nNo-Export: true'
        return message

    def _delete_orphaned_baselines(self):
        _log.info('Deleting any orphaned baselines.')

        is_baseline_filter = lambda fs, dirname, basename: is_testharness_baseline(basename)

        baselines = self.fs.files_under(self.dest_path, file_filter=is_baseline_filter)

        # Note about possible refactoring:
        #  - the manifest path could be factored out to a common location, and
        #  - the logic for reading the manifest could be factored out from here
        # and the Port class.
        manifest_path = self.finder.path_from_web_tests('external', 'wpt', 'MANIFEST.json')
        manifest = WPTManifest(self.fs.read_text_file(manifest_path))
        wpt_urls = manifest.all_urls()

        # Currently baselines for tests with query strings are merged,
        # so that the tests foo.html?r=1 and foo.html?r=2 both have the same
        # baseline, foo-expected.txt.
        # TODO(qyearsley): Remove this when this behavior is fixed.
        wpt_urls = [url.split('?')[0] for url in wpt_urls]

        wpt_dir = self.finder.path_from_web_tests('external', 'wpt')
        for full_path in baselines:
            rel_path = self.fs.relpath(full_path, wpt_dir)
            if not self._has_corresponding_test(rel_path, wpt_urls):
                self.fs.remove(full_path)

    def _has_corresponding_test(self, rel_path, wpt_urls):
        # TODO(qyearsley): Ensure that this works with platform baselines and
        # virtual baselines, and add unit tests.
        base = '/' + rel_path.replace('-expected.txt', '')
        return any((base + ext) in wpt_urls for ext in Port.supported_file_extensions)

    def copyfile(self, source, destination):
        _log.debug('cp %s %s', source, destination)
        self.fs.copyfile(source, destination)

    def remove(self, dest):
        _log.debug('rm %s', dest)
        self.fs.remove(dest)

    def _upload_patchset(self, message):
        self.git_cl.run(['upload', '-f', '-t', message, '--gerrit'])

    def _upload_cl(self):
        _log.info('Uploading change list.')
        directory_owners = self.get_directory_owners()
        description = self._cl_description(directory_owners)
        sheriff_email = self.tbr_reviewer()

        temp_file, temp_path = self.fs.open_text_tempfile()
        temp_file.write(description)
        temp_file.close()

        self.git_cl.run([
            'upload',
            '-f',
            '--gerrit',
            '--message-file', temp_path,
            '--tbrs', sheriff_email,
            # Note: we used to CC all the directory owners, but have stopped
            # in search of a better notification mechanism. (crbug.com/765334)
            '--cc', 'robertma@chromium.org',
        ])

        self.fs.remove(temp_path)

    def get_directory_owners(self):
        """Returns a mapping of email addresses to owners of changed tests."""
        _log.info('Gathering directory owners emails to CC.')
        changed_files = self.chromium_git.changed_files()
        extractor = DirectoryOwnersExtractor(self.fs)
        return extractor.list_owners(changed_files)

    def _cl_description(self, directory_owners):
        """Returns a CL description string.

        Args:
            directory_owners: A dict of tuples of owner names to lists of directories.
        """
        # TODO(robertma): Add a method in Git for getting the commit body.
        description = self.chromium_git.run(['log', '-1', '--format=%B'])
        description += (
            'Note to sheriffs: This CL imports external tests and adds\n'
            'expectations for those tests; if this CL is large and causes\n'
            'a few new failures, please fix the failures by adding new\n'
            'lines to TestExpectations rather than reverting. See:\n'
            'https://chromium.googlesource.com'
            '/chromium/src/+/master/docs/testing/web_platform_tests.md\n\n')

        if directory_owners:
            description += self._format_directory_owners(directory_owners) + '\n\n'

        # Prevent FindIt from auto-reverting import CLs.
        description += 'NOAUTOREVERT=true\n'

        # Move any No-Export tag to the end of the description.
        description = description.replace('No-Export: true', '')
        description = description.replace('\n\n\n\n', '\n\n')
        description += 'No-Export: true'
        return description

    @staticmethod
    def _format_directory_owners(directory_owners):
        message_lines = ['Directory owners for changes in this CL:']
        for owner_tuple, directories in sorted(directory_owners.items()):
            message_lines.append(', '.join(owner_tuple) + ':')
            message_lines.extend('  ' + d for d in directories)
        return '\n'.join(message_lines)

    def tbr_reviewer(self):
        """Returns the user name or email address to use as the reviewer.

        This tries to fetch the current ecosystem infra sheriff, but falls back
        in case of error.

        Either a user name (which is assumed to have a chromium.org email
        address) or a full email address (for other cases) is returned.
        """
        username = ''
        try:
            username = self._fetch_ecosystem_infra_sheriff_username()
        except (IOError, KeyError, ValueError) as error:
            _log.error('Exception while fetching current sheriff: %s', error)
        if username in ['kyleju']:
            _log.warning('Cannot TBR by %s: not a committer', username)
            username = ''
        return username or TBR_FALLBACK

    def _fetch_ecosystem_infra_sheriff_username(self):
        try:
            content = self.host.web.get_binary(ROTATIONS_URL)
        except NetworkTimeout:
            _log.error('Cannot fetch %s', ROTATIONS_URL)
            return ''
        data = json.loads(content)
        today = datetime.date.fromtimestamp(self.host.time()).isoformat()
        index = data['rotations'].index('ecosystem_infra')
        calendar = data['calendar']
        for entry in calendar:
            if entry['date'] == today:
                if not entry['participants'][index]:
                    _log.info('No sheriff today.')
                    return ''
                return entry['participants'][index][0]
        _log.error('No entry found for date %s in rotations table.', today)
        return ''

    def fetch_new_expectations_and_baselines(self):
        """Modifies expectation lines and baselines based on try job results.

        Assuming that there are some try job results available, this
        adds new expectation lines to TestExpectations and downloads new
        baselines based on the try job results.

        This is the same as invoking the `wpt-update-expectations` script.
        """
        _log.info('Adding test expectations lines to TestExpectations.')
        expectation_updater = WPTExpectationsUpdater(self.host)
        self.rebaselined_tests, self.new_test_expectations = expectation_updater.update_expectations()

    def update_all_test_expectations_files(self, deleted_tests, renamed_tests):
        """Updates all test expectations files for tests that have been deleted or renamed.

        This is only for deleted or renamed tests in the initial import,
        not for tests that have failures in try jobs.
        """
        port = self.host.port_factory.get()
        for path, file_contents in port.all_expectations_dict().iteritems():
            parser = TestExpectationParser(port, all_tests=None, is_lint_mode=False)
            expectation_lines = parser.parse(path, file_contents)
            self._update_single_test_expectations_file(port, path, expectation_lines, deleted_tests, renamed_tests)

    def _update_single_test_expectations_file(self, port, path, expectation_lines, deleted_tests, renamed_tests):
        """Updates a single test expectations file."""
        # FIXME: This won't work for removed or renamed directories with test
        # expectations that are directories rather than individual tests.
        new_lines = []
        changed_lines = []
        for expectation_line in expectation_lines:
            expectation_test_name = expectation_line.name
            if expectation_test_name and self.finder.is_webdriver_test_path(expectation_test_name):
                expectation_test_name, subtest_suffix = port.split_webdriver_test_name(expectation_test_name)
            if expectation_test_name in deleted_tests:
                continue
            if expectation_test_name in renamed_tests:
                if self.finder.is_webdriver_test_path(expectation_line.name):
                    renamed_test = renamed_tests[expectation_test_name]
                    expectation_line.name = port.add_webdriver_subtest_suffix(renamed_test, subtest_suffix)
                else:
                    expectation_line.name = renamed_tests[expectation_test_name]
                # Upon parsing the file, a "path does not exist" warning is expected
                # to be there for tests that have been renamed, and if there are warnings,
                # then the original string is used. If the warnings are reset, then the
                # expectation line is re-serialized when output.
                expectation_line.warnings = []
                changed_lines.append(expectation_line)
            new_lines.append(expectation_line)
        new_file_contents = TestExpectations.list_to_string(new_lines, reconstitute_only_these=changed_lines)
        self.host.filesystem.write_text_file(path, new_file_contents)

    def _list_deleted_tests(self):
        """List of web tests that have been deleted."""
        # TODO(robertma): Improve Git.changed_files so that we can use it here.
        out = self.chromium_git.run(['diff', 'origin/master', '-M100%', '--diff-filter=D', '--name-only'])
        deleted_tests = []
        for path in out.splitlines():
            test = self._relative_to_web_test_dir(path)
            if test:
                deleted_tests.append(test)
        return deleted_tests

    def _list_renamed_tests(self):
        """Lists tests that have been renamed.

        Returns a dict mapping source name to destination name.
        """
        out = self.chromium_git.run(['diff', 'origin/master', '-M100%', '--diff-filter=R', '--name-status'])
        renamed_tests = {}
        for line in out.splitlines():
            _, source_path, dest_path = line.split()
            source_test = self._relative_to_web_test_dir(source_path)
            dest_test = self._relative_to_web_test_dir(dest_path)
            if source_test and dest_test:
                renamed_tests[source_test] = dest_test
        return renamed_tests

    def _relative_to_web_test_dir(self, path_relative_to_repo_root):
        """Returns a path that's relative to the web tests directory."""
        abs_path = self.finder.path_from_chromium_base(path_relative_to_repo_root)
        if not abs_path.startswith(self.finder.web_tests_dir()):
            return None
        return self.fs.relpath(abs_path, self.finder.web_tests_dir())

    def _get_last_imported_wpt_revision(self):
        """Finds the last imported WPT revision."""
        # TODO(robertma): Only match commit subjects.
        output = self.chromium_git.most_recent_log_matching('^Import wpt@', self.finder.chromium_base())
        # No line-start anchor (^) below because of the formatting of output.
        result = re.search(r'Import wpt@(\w+)', output)
        if result:
            return result.group(1)
        else:
            _log.error('Cannot find last WPT import.')
            return None

    def send_notifications(self, local_wpt, auto_file_bugs, monorail_auth_json):
        issue = self.git_cl.run(['status', '--field=id']).strip()
        patchset = self.git_cl.run(['status', '--field=patch']).strip()
        # Construct the notifier here so that any errors won't affect the import.
        notifier = ImportNotifier(self.host, self.chromium_git, local_wpt)
        notifier.main(self.last_wpt_revision, self.wpt_revision,
                      self.rebaselined_tests, self.new_test_expectations,
                      issue, patchset,
                      dry_run=not auto_file_bugs, service_account_key_json=monorail_auth_json)
        return True
