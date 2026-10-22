# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility functions used both when importing and exporting."""

import json
import logging
import re

from requests.structures import CaseInsensitiveDict

WPT_GH_ORG = 'web-platform-tests'
WPT_GH_REPO_NAME = 'wpt'
WPT_GH_URL = 'https://github.com/%s/%s/' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_GH_RANGE_URL_TEMPLATE = '%scompare/{}...{}' % WPT_GH_URL
WPT_MIRROR_URL = 'https://chromium.googlesource.com/external/github.com/web-platform-tests/wpt.git'
WPT_GH_SSH_URL_TEMPLATE = 'https://{}@github.com/%s/%s.git' % (
    WPT_GH_ORG,
    WPT_GH_REPO_NAME,
)
CHANGE_ID_KEY = 'Change-Id'
LINK_KEY = 'Link'
WPT_REVISION_KEY = 'WPT-Export-Revision'
REVIEWED_ON_KEY = 'Reviewed-on'
WPT_REVISION_FOOTER = f'{WPT_REVISION_KEY}: '
_FOOTER_PATTERN = re.compile(r'\s*(?P<key>[\w-]+):\s*(?P<value>.*?)\s*')
EXPORT_PR_LABEL = 'chromium-export'
PROVISIONAL_PR_LABEL = 'do not merge yet'

AUTOROLLER_EMAIL = (
    'wpt-autoroller@chops-service-accounts.iam.gserviceaccount.com'
)
# These are only set in a new WPT checkout, and they should be consistent with
# the bot's GitHub account (chromium-wpt-export-bot).
DEFAULT_WPT_COMMITTER_NAME = 'Chromium WPT Sync'
DEFAULT_WPT_COMMITTER_EMAIL = 'blink-w3c-test-autoroller@chromium.org'

EXPORT_DENYLIST = {
    'third_party/blink/web_tests/external/wpt/config.tmpl.json',
    'third_party/blink/web_tests/external/wpt/config.json',
}

LEGACY_MAIN_BRANCH_NAME = 'retsam'[::-1]

_log = logging.getLogger(__name__)


def parse_footers(message: str) -> CaseInsensitiveDict:
    """Parse footer key-value pairs from a commit message or PR body.

    Skips the subject line if present (to avoid potential Gerrit hashtags in the
    subject). When a footer key appears multiple times, the last value takes
    precedence.

    Unlike `git footers --key`, which terminates backwards scanning at the
    first blank line and therefore misses footers separated by blank lines
    (e.g., when authors leave blank lines before landed footers), this
    function scans across blank lines. It also avoids subprocess overhead.

    Returns:
        A CaseInsensitiveDict mapping normalized footer keys to their values.
    """
    footers = CaseInsensitiveDict()
    lines = message.splitlines()
    if len(lines) > 1 and lines[1] == '':
        lines = lines[2:]
    for line in lines:
        if match := _FOOTER_PATTERN.fullmatch(line):
            key = match.group('key')
            if key.lower() in {'http', 'https'}:
                continue
            footers[key] = match.group('value')
    return footers


def read_credentials(host, credentials_json):
    """Extracts GitHub and Gerrit credentials.

    The data is read from the environment and (optionally) a JSON file. When a
    JSON file is specified, any variables from the environment are discarded
    and the values are all expected to be present in the file.

    Args:
        credentials_json: Path to a JSON file containing an object with the
            keys we want to read, or None.
    """
    env_credentials = {}
    for key in ('GH_USER', 'GH_TOKEN', 'GERRIT_USER', 'GERRIT_TOKEN'):
        if key in host.environ:
            env_credentials[key] = host.environ[key]
    if not credentials_json:
        return env_credentials
    if not host.filesystem.exists(credentials_json):
        _log.warning('Credentials JSON file not found at %s.', credentials_json)
        return {}
    credentials = {}
    contents = json.loads(host.filesystem.read_text_file(credentials_json))
    for key in ('GH_USER', 'GH_TOKEN', 'GERRIT_USER', 'GERRIT_TOKEN'):
        if key in contents:
            credentials[key] = contents[key]
    return credentials


def is_testharness_baseline(filename):
    """Checks whether a given file name appears to be a testharness baseline.

    Args:
        filename: A path (absolute or relative) or a basename.
    """
    return filename.endswith('-expected.txt')


def is_basename_skipped(basename):
    """Checks whether to skip (not sync) a file based on its basename.

    Note: this function is used during both import and export, i.e., files with
    skipped basenames are never imported or exported.
    """
    assert '/' not in basename
    skipped_basenames = [
        'MANIFEST.json',  # MANIFEST.json is automatically regenerated.
        'OWNERS',  # https://crbug.com/584660 https://crbug.com/702283
        'reftest.list',  # https://crbug.com/582838
        'DIR_METADATA',  # https://crbug.com/1103374
        'PRESUBMIT.py',
    ]
    return (
        basename in skipped_basenames
        or is_testharness_baseline(basename)
        or basename.startswith('.')
    )


def is_file_exportable(path, project_config):
    """Checks whether a file in Chromium WPT should be exported to upstream.

    Args:
        path: A relative path from the root of Chromium repository.
    """
    assert path.startswith(project_config.relative_tests_path)
    basename = path[path.rfind('/') + 1 :]
    return path not in EXPORT_DENYLIST and not is_basename_skipped(basename)
