# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WPTManifest is responsible for handling MANIFEST.json.

The MANIFEST.json file contains metadata about files in web-platform-tests,
such as what tests exist, and extra information about each test, including
test type, options, URLs to use, and reference file paths if applicable.

Naming conventions:
* A (file) path is a relative file system path from the root of WPT.
* A (test) URL is the path (with an optional query string) to the test on
  wptserve relative to url_base.
Neither has a leading slash.
"""

import json
import logging

from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder

_log = logging.getLogger(__file__)

# The default filename of manifest expected by `wpt`.
MANIFEST_NAME = 'MANIFEST.json'
# The filename used for the base manifest includes the version as a
# workaround for trouble landing huge changes to the base manifest when
# the version changes. See https://crbug.com/876717.
BASE_MANIFEST_NAME = 'WPT_BASE_MANIFEST_6.json'

# TODO(robertma): Use the official wpt.manifest module.


class WPTManifest(object):
    """A simple abstraction of WPT MANIFEST.json.

    The high-level structure of the manifest is as follows:
        {
            "items": {
                "manual": {
                    "file/path": [manifest items],
                    ...
                },
                "reftest": {...},
                "testharness": {...}
            },
            // other info...
        }

    The format of a manifest item depends on:
        https://github.com/web-platform-tests/wpt/blob/master/tools/manifest/item.py
    which can be roughly summarized as follows:
        * testharness test: [url, extras]
        * reftest: [url, references, extras]
    where `extras` is a dict with the following optional items:
        * testharness test: {"timeout": "long", "testdriver": True}
        * reftest: {"timeout": "long", "viewport_size": ..., "dpi": ...}
    and `references` is a list that looks like:
        [[reference_url1, "=="], [reference_url2, "!="], ...]
    """

    def __init__(self, json_content):
        self.raw_dict = json.loads(json_content)
        self.test_types = ('manual', 'reftest', 'testharness')
        self.test_name_to_file = {}

    def _items_for_file_path(self, path_in_wpt):
        """Finds manifest items for the given WPT path.

        Args:
            path_in_wpt: A file path relative to the root of WPT.

        Returns:
            A list of manifest items, or None if not found.
        """
        items = self.raw_dict.get('items', {})
        for test_type in self.test_types:
            if test_type not in items:
                continue
            if path_in_wpt in items[test_type]:
                return items[test_type][path_in_wpt]
        return None

    def _item_for_url(self, url):
        """Finds the manifest item for the given WPT URL.

        Args:
            url: A WPT URL.

        Returns:
            A manifest item, or None if not found.
        """
        return self.all_url_items().get(url)

    @staticmethod
    def _get_url_from_item(item):
        return item[0]

    @staticmethod
    def _get_extras_from_item(item):
        return item[-1]

    @staticmethod
    def _is_not_jsshell(item):
        """Returns True if the manifest item isn't a jsshell test.

        "jsshell" is one of the scopes automatically generated from .any.js
        tests. It is intended to run in a thin JavaScript shell instead of a
        full browser, so we simply ignore it in web tests. (crbug.com/871950)
        """
        extras = WPTManifest._get_extras_from_item(item)
        return not extras.get('jsshell', False)

    @memoized
    def all_url_items(self):
        """Returns a dict mapping every URL in the manifest to its item."""
        url_items = {}
        if 'items' not in self.raw_dict:
            return url_items
        items = self.raw_dict['items']
        for test_type in self.test_types:
            if test_type not in items:
                continue
            for filename, records in items[test_type].iteritems():
                for item in filter(self._is_not_jsshell, records):
                    url_for_item = self._get_url_from_item(item)
                    url_items[url_for_item] = item
                    self.test_name_to_file[url_for_item] = filename
        return url_items

    @memoized
    def all_urls(self):
        """Returns a set of the URLs for all items in the manifest."""
        return frozenset(self.all_url_items().keys())

    def is_test_file(self, path_in_wpt):
        """Checks if path_in_wpt is a test file according to the manifest."""
        assert not path_in_wpt.startswith('/')
        return self._items_for_file_path(path_in_wpt) is not None

    def is_test_url(self, url):
        """Checks if url is a valid test in the manifest."""
        assert not url.startswith('/')
        return url in self.all_urls()

    def is_slow_test(self, url):
        """Checks if a WPT is slow (long timeout) according to the manifest.

        Args:
            url: A WPT URL.

        Returns:
            True if the test is found and is slow, False otherwise.
        """
        if not self.is_test_url(url):
            return False

        item = self._item_for_url(url)
        if not item:
            return False
        extras = self._get_extras_from_item(item)
        return extras.get('timeout') == 'long'

    def extract_reference_list(self, path_in_wpt):
        """Extracts reference information of the specified reference test.

        The return value is a list of (match/not-match, reference path in wpt)
        pairs, like:
           [("==", "/foo/bar/baz-match.html"),
            ("!=", "/foo/bar/baz-mismatch.html")]
        """
        items = self.raw_dict.get('items', {})
        if path_in_wpt not in items.get('reftest', {}):
            return []
        reftest_list = []
        for item in items['reftest'][path_in_wpt]:
            for ref_path_in_wpt, expectation in item[1]:
                # Ref URLs in MANIFEST should be absolute, but we double check
                # just in case.
                if not ref_path_in_wpt.startswith('/'):
                    ref_path_in_wpt = '/' + ref_path_in_wpt
                reftest_list.append((expectation, ref_path_in_wpt))
        return reftest_list

    def file_path_for_test_url(self, url):
        """Finds the file path for the given test URL.

        Args:
            url: a WPT test URL.

        Returns:
            The path to the file containing this test URL, or None if not found.
        """
        # Call all_url_items to ensure the test to file mapping is populated.
        self.all_url_items()
        return self.test_name_to_file.get(url)

    @staticmethod
    def ensure_manifest(port, path=None):
        """Updates the MANIFEST.json file, or generates if it does not exist.

        Args:
            port: A blinkpy.web_tests.port.Port object.
            path: The path to a WPT root (relative to web_tests, optional).
        """
        fs = port.host.filesystem
        if path is None:
            path = fs.join('external', 'wpt')
        wpt_path = fs.join(port.web_tests_dir(), path)
        manifest_path = fs.join(wpt_path, MANIFEST_NAME)

        # Unconditionally delete local MANIFEST.json to avoid regenerating the
        # manifest from scratch (when version is bumped) or invalid/out-of-date
        # local manifest breaking the runner.
        if fs.exists(manifest_path):
            fs.remove(manifest_path)

        # TODO(crbug.com/853815): perhaps also cache the manifest for wpt_internal.
        if 'external' in path:
            base_manifest_path = fs.join(port.web_tests_dir(), 'external', BASE_MANIFEST_NAME)
            if fs.exists(base_manifest_path):
                fs.copyfile(base_manifest_path, manifest_path)
            else:
                _log.error('Manifest base not found at "%s".', base_manifest_path)

        WPTManifest.generate_manifest(port.host, wpt_path)

        if fs.isfile(manifest_path):
            _log.debug('Manifest generation completed.')
        else:
            _log.error('Manifest generation failed; creating an empty MANIFEST.json...')
            fs.write_text_file(manifest_path, '{}')

    @staticmethod
    def generate_manifest(host, dest_path):
        """Generates MANIFEST.json on the specified directory."""
        finder = PathFinder(host.filesystem)
        wpt_exec_path = finder.path_from_blink_tools('blinkpy', 'third_party', 'wpt', 'wpt', 'wpt')
        cmd = ['python', wpt_exec_path, 'manifest', '--no-download', '--tests-root', dest_path]

        # ScriptError will be raised if the command fails.
        host.executive.run_command(
            cmd,
            return_stderr=True  # This will also include stderr in the exception message.
        )
