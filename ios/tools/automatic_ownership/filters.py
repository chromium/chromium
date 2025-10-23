#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def avoid_file(file: str) -> bool:
    """Determines if a file should be skipped in the analysis.

    This function is used to filter for primary source code files based on
    their name. It allows files with 'test' in their name, as well as language
    extensions used in iOS Chrome.

    Args:
        file: The name of the file to check. Assumes a valid string filename.

    Returns:
        True if the file should be avoided (skipped), False otherwise.
    """
    if 'test' in file:
        return False
    return not (file.endswith('.h') or file.endswith('.mm')
                or file.endswith('.cc') or file.endswith('.swift')
                or file.endswith('.ts'))


def avoid_directory(directory: str) -> bool:
    """Determines if a directory should be skipped in the analysis.

    This is used to exclude directories that typically do not contain source
    code relevant for ownership analysis, such as test directories, resources,
    or third-party code.

    Args:
        directory: The path of the directory to check. Assumes a valid string.

    Returns:
        True if the directory should be avoided (skipped), False otherwise.
    """
    # French version
    if 'ressources' in directory:
        return True
    # English version :D
    if 'resources' in directory:
        return True
    if 'xcassets' in directory:
        return True
    if 'strings' in directory:
        return True
    if 'test' in directory:
        return True
    if 'tests' in directory:
        return True
    if 'third_party' in directory:
        return True
    if 'ios/chrome/browser/flags/' in directory:
        return True

    return False


def avoid_commit(commit: str) -> bool:
    """Determines if a commit should be skipped in the analysis.

    This function filters out commits that are unlikely to be useful for
    determining code ownership. This includes automated commits (e.g., reverts,
    large-scale refactors), commits with a very large number of changed files,
    or malformed commit messages.

    Args:
        commit: The full text of a commit, including its message and stats.
            Assumes a valid, non-empty string.

    Returns:
        True if the commit should be avoided (skipped), False otherwise.
    """
    if not 'Author' in commit:
        return True

    keyword_to_ignore = [
        'Revert',
        'Migrate TODOs referencing old crbug IDs to the new issue tracker IDs',
        'Use NOTREACHED_IN_MIGRATION() in ios/',
        'Migrate to NOTREACHED() in ios/',
        '[iOS] Clean //i/c/b/s/',
        '[iOS] Remove useless imports of chrome browser state',
        '[iOS] Migrate ChromeBrowserState forward declaration to header.',
        '[iOS] Use forwarding header of ',
        '[iOS] Updates files under ios/',
        '[iOS] Update files under ios/',
        '[iOS] Remove usage of profile_ios_forward.h',
        '[iOS] Remove chrome_browser_state.h',
        '[Code Health]',
        '[gardener]',
        '[iOS] Force format in all BUILD files',
        'Move foundation_util to base/apple, leave a forwarding header',
        'Remove ARC boilerplate in /ios',
        'Remove redundant ARC configuration in ',
        '[iOS] Remove GetForBrowserState',
        '[iOS] Prepare ios_internal migration',
        '#cleanup',
        'Disable test',
        'Fix typo',
        'Reland',
        '[ios] Migrate to ProfileIOS',
        'Cleanup feature',
        'LARGE_SCALE_REFACTOR',
        'bling-autoroll-builder',
        'chromium-autoroll',
        'ios/third_party/earl_grey2',
        'git cl split',
    ]
    for keyword in keyword_to_ignore:
        if keyword in commit:
            return True

    stats_regex = re.search('[0-9]+ file.? changed', commit)
    if stats_regex:
        modified_files_count = int(
            commit[stats_regex.start():stats_regex.end()].split()[0])
        if modified_files_count >= 20:
            return True
    return False


def avoid_username(username: str) -> bool:
    """Determines if a username should be skipped in the analysis.

    This is used to filter out bot accounts and autorollers that are not
    indicative of human code ownership.

    Args:
        username: The username to check. Assumes a valid, non-empty string.

    Returns:
        True if the username should be avoided (skipped), False otherwise.
    """
    username_to_ignore = [
        'mdb.chrome-pki-metadata-release-jobs',
        'wpt-autoroller',
        'chromium-internal-autoroll',
        'chromium-autoroll',
        'chrome-official-brancher',
        'chromium-automated-expectation',
        'v8-ci-autoroll-builder',
        'skylab-test-cros-roller',
    ]
    return username in username_to_ignore


def avoid_owner_line(line: str) -> bool:
    """Checks if a line from an OWNERS file should be filtered out.

    This function is used to filter out lines that are not relevant for
    determining code ownership, such as comments, 'set noparent' directives,
    and 'per-file' rules.

    Args:
        line: The line from the OWNERS file to check.

    Returns:
        True if the line should be filtered out, False otherwise.
    """
    line = line.strip()
    return not line or line.startswith(('#', 'set noparent', 'per-file'))
