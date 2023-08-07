# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from blinkpy.w3c.chromium_commit import ChromiumCommit

_log = logging.getLogger(__name__)

DEFAULT_COMMIT_HISTORY_WINDOW = 10000


def exportable_commits_over_last_n_commits(
        host,
        local_wpt,
        wpt_github,
        number=DEFAULT_COMMIT_HISTORY_WINDOW,
        require_clean=True,
        verify_merged_pr=False):
    """Lists exportable commits after a certain point.

    Exportable commits contain changes in the wpt directory and have not been
    exported (no corresponding closed PRs on GitHub). Commits made by importer
    are ignored. Exportable commits may or may not apply cleanly against the
    wpt HEAD (see argument require_clean).

    Args:
        host: A Host object.
        local_wpt: A LocalWPT instance, used to see whether a Chromium commit
            can be applied cleanly in the upstream repo.
        wpt_github: A WPTGitHub instance, used to check whether PRs are closed.
        number: The number of commits back to look. The commits to check will
            include all commits starting from the commit before HEAD~n, up
            to and including HEAD.
        require_clean: Whether to only return exportable commits that can be
            applied cleanly and produce non-empty diff when tested individually.
        verify_merged_pr: Whether to verify merged PRs can be found in the local
            WPT repo. If this argument is True, for each Chromium commit with a
            corresponding merged PR, we also check the local WPT repo, and still
            consider the commit exportable if it cannot be found in local WPT.
            (Note: Chromium commits that have closed but not merged PRs are
            always considered exported regardless of this argument.)

    Returns:
        (exportable_commits, errors) where exportable_commits is a list of
        ChromiumCommit objects for exportable commits in the given window, and
        errors is a list of error messages when exportable commits fail to apply
        cleanly, both in chronological order.
    """
    start_commit = 'HEAD~{}'.format(number + 1)
    return _exportable_commits_since(start_commit, host, local_wpt, wpt_github,
                                     require_clean, verify_merged_pr)


def _exportable_commits_since(chromium_commit_hash,
                              host,
                              local_wpt,
                              wpt_github,
                              require_clean=True,
                              verify_merged_pr=False):
    """Lists exportable commits after the given commit.

    Args:
        chromium_commit_hash: The SHA of the Chromium commit from which this
            method will look. This commit is not included in the commits searched.

    Return values and remaining arguments are the same as exportable_commits_over_last_n_commits.
    """
    chromium_repo_root = host.executive.run_command(
        ['git', 'rev-parse', '--show-toplevel'],
        host.project_config.project_root).strip()
    wpt_path = chromium_repo_root + '/' + host.project_config.relative_tests_path
    commit_range = '{}..HEAD'.format(chromium_commit_hash)
    skipped_revs = ['^' + rev for rev in wpt_github.skipped_revisions]
    command = (['git', 'rev-list', commit_range] + skipped_revs +
               ['--reverse', '--', wpt_path])
    commit_hashes = host.executive.run_command(
        command, cwd=host.project_config.project_root).splitlines()
    chromium_commits = [ChromiumCommit(host, sha=sha) for sha in commit_hashes]
    exportable_commits = []
    errors = []
    for commit in chromium_commits:
        state, error = get_commit_export_state(commit, local_wpt, wpt_github,
                                               verify_merged_pr)
        _log.info('Commit %s has export state: "%s"', commit.short_sha, state)

        if require_clean:
            success = state == CommitExportState.EXPORTABLE_CLEAN
        else:
            success = state in (CommitExportState.EXPORTABLE_CLEAN,
                                CommitExportState.EXPORTABLE_DIRTY)
        if success:
            exportable_commits.append(commit)
        elif error != '':
            errors.append(
                'The following commit did not apply cleanly:\nSubject: %s (%s)\n%s' % \
                    (commit.subject(), commit.url(), error))
    return exportable_commits, errors


def get_commit_export_state(chromium_commit,
                            local_wpt,
                            wpt_github,
                            verify_merged_pr=False):
    """Determines the exportability state of a Chromium commit.

    Args:
        verify_merged_pr: Whether to verify merged PRs can be found in the local
            WPT repo. If this argument is True, for each Chromium commit with a
            corresponding merged PR, we also check the local WPT repo, and still
            consider the commit exportable if it cannot be found in local WPT.
            (Note: Chromium commits that have closed but not merged PRs are
            always considered exported regardless of this argument.)

    Returns:
        (state, error): state is one of the members of CommitExportState;
        error is a string of error messages if an exportable patch fails to
        apply (i.e. state=CommitExportState.EXPORTABLE_DIRTY).
    """
    msg_lowercase = chromium_commit.message().lower()
    if 'noexport=true' in msg_lowercase or 'no-export: true' in msg_lowercase:
        return CommitExportState.IGNORED, ''

    patch = chromium_commit.format_patch()
    if not patch:
        return CommitExportState.NO_PATCH, ''

    if _is_commit_exported(chromium_commit, local_wpt, wpt_github,
                           verify_merged_pr):
        return CommitExportState.EXPORTED, ''

    success, error = local_wpt.test_patch(patch)
    return ((CommitExportState.EXPORTABLE_CLEAN, '') if success else
            (CommitExportState.EXPORTABLE_DIRTY, error))


def _is_commit_exported(chromium_commit, local_wpt, wpt_github,
                        verify_merged_pr):
    pull_request = wpt_github.pr_for_chromium_commit(chromium_commit)
    if not pull_request:
        _log.info(
            'Checking if commit is exported: no existing PR found. '
            'Commit: %s', chromium_commit.short_sha)
        return False

    if pull_request.state != 'closed':
        _log.info(
            'Checking if commit is exported: pull request is not closed. '
            'Commit: %s, PR number: %s, PR state: %s',
            chromium_commit.short_sha, pull_request.number, pull_request.state)
        return False

    # A closed PR can either be merged or abandoned:
    # * Merged PR might not be present in local WPT as the checkout may be
    #   stale. If verify_merged_pr=True, we further search the git log of local
    #   WPT the commit to prevent clobbering during import. (crbug.com/756428)
    # * Abandoned PRs are expected to be reverted in Chromium by importer, so
    #   they are always considered "exported".
    if not verify_merged_pr:
        # If no verification is needed, all closed PRs are deemed exported.
        return True

    if not wpt_github.is_pr_merged(pull_request.number):
        # PR is abandoned.
        return True

    # PR is merged, and we need to verify that local WPT contains the commit.
    change_id = chromium_commit.change_id()
    found_in_upstream = bool(
        local_wpt.seek_change_id(change_id) if change_id else local_wpt.
        seek_commit_position(chromium_commit.position))
    if not found_in_upstream:
        needle = change_id if change_id else chromium_commit.position
        _log.info(
            'Checking if commit is exported: failed to find change in local '
            'WPT checkout. Searched for: %s', needle)

    return found_in_upstream


class CommitExportState(object):
    """An enum class for exportability states of a Chromium commit."""
    # pylint: disable=pointless-string-statement
    # String literals are used as attribute docstrings (PEP 257).

    IGNORED = 'ignored'
    """The commit was an import or contains No-Export tags."""

    NO_PATCH = 'no patch'
    """Failed to format patch from the commit."""

    EXPORTED = 'exported'
    """There is a corresponding upstream PR that has been closed (merged or abandoned)."""

    EXPORTABLE_DIRTY = 'exportable dirty'
    """The commit is exportable, but does not apply cleanly or produces empty diff."""

    EXPORTABLE_CLEAN = 'exportable clean'
    """The commit is exportable."""
