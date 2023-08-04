# Copyright 2017 The Chromium Authors
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
from typing import Optional, Sequence

from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder

_log = logging.getLogger(__file__)

# The default filename of manifest expected by `wpt`.
MANIFEST_NAME = 'MANIFEST.json'

# Generating the WPT manifest entirely from scratch is a slow process; it takes
# >10 seconds real-time on a powerful Linux desktop. To avoid paying this cost,
# we keep a cached version of the manifest in the source tree, the 'base
# manifest', and update it automatically whenever we import WPT. We utilize a
# separate file for this and then copy it to MANIFEST_NAME so that modifications
# or corruptions (which often happen if the test runner is killed by the user
# mid-run) do not cause trouble.
#
# The filename used for the base manifest includes the version as a
# workaround for trouble landing huge changes to the base manifest when
# the version changes. See https://crbug.com/876717.
#
# NOTE: If this is changed, be sure to update other instances of
# "WPT_BASE_MANIFEST_8" in the code.
BASE_MANIFEST_NAME = 'WPT_BASE_MANIFEST_8.json'

# TODO(robertma): Use the official wpt.manifest module.


class WPTManifest(object):
    """A simple abstraction of WPT MANIFEST.json.

    The high-level structure of the manifest is as follows:
        {
            "items": {
                "crashtest": {
                    "dir1": {
                        "dir2": {
                            "filename1": [
                                "git object ID",
                                [manifest item],
                                [manifest item],
                                ...
                            ],
                        },
                    },
                },
                "manual": {...},
                "reftest": {...},
                "print-reftest": {...},
                "testharness": {...},
            },
            // other info...
        }

    The 'git object ID' is the ID the git repository has assigned to the file
    blob, i.e. via git hash-object.

    The format of a manifest item depends on:
        https://github.com/web-platform-tests/wpt/blob/master/tools/manifest/item.py
    which can be roughly summarized as follows:
        * testharness test: [url, extras]
        * reftest: [url, references, extras]
        * print-reftest: [url, references, extras]
    where `extras` is a dict with the following optional items:
        * testharness test: {"timeout": "long", "testdriver": True}
        * reftest: {"timeout": "long", "viewport_size": ..., "dpi": ...}
        * print-reftest: {"timeout": "long", "viewport_size": ..., "dpi": ..., "page_ranges": ...}
    and `references` is a list that looks like:
        [[reference_url1, "=="], [reference_url2, "!="], ...]
    """

    def __init__(self,
                 host,
                 manifest_path,
                 test_types: Optional[Sequence[str]] = None,
                 exclude_jsshell: bool = True):
        self.host = host
        self.port = self.host.port_factory.get()
        self.raw_dict = json.loads(
            self.host.filesystem.read_text_file(manifest_path))
        # As a workaround to handle the change from a flat-list to a trie
        # structure in the v8 manifest, flatten the items back to the v7 format.
        #
        # TODO(crbug.com/912496): Properly support the trie structure.
        self.raw_dict['items'] = self._flatten_items(
            self.raw_dict.get('items', {}))

        self.wpt_manifest_path = manifest_path
        self.test_types = test_types or (
            'manual',
            'reftest',
            'print-reftest',
            'testharness',
            'crashtest',
        )
        self.test_name_to_file = {}
        self._exclude_jsshell = exclude_jsshell

    @property
    def wpt_dir(self):
        return self.host.filesystem.dirname(
            self.host.filesystem.relpath(
                self.wpt_manifest_path, self.port.web_tests_dir()))

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
            for filename, records in items[test_type].items():
                if self._exclude_jsshell:
                    records = filter(self._is_not_jsshell, records)
                for item in records:
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

    def get_test_type(self, test_path: str) -> Optional[str]:
        """Returns the test type of the given test url."""
        assert not test_path.startswith('/')
        items = self.raw_dict.get('items', {})
        for test_type in self.test_types:
            type_items = items.get(test_type, {})
            if test_path in type_items:
                return test_type
        return None

    def is_test_url(self, url):
        """Checks if url is a valid test in the manifest."""
        assert not url.startswith('/')
        return url in self.all_urls()

    def is_crash_test(self, url):
        """Checks if a WPT is a crashtest according to the manifest."""
        return self.get_test_type(url) == 'crashtest'

    def is_manual_test(self, url):
        """Checks if a WPT is a manual according to the manifest."""
        return self.get_test_type(url) == 'manual'

    def is_print_reftest(self, url):
        """Checks if a WPT is a print reftest according to the manifest."""
        return self.get_test_type(url) == 'print-reftest'

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

    def extract_test_pac(self, url):
        """Get the proxy configuration (PAC) for the test

        Args:
            url: A WPT URL.

        Returns:
            A relative PAC url if noted by the test, None otherwise.
        """
        if not self.is_test_url(url):
            return None

        item = self._item_for_url(url)
        if not item:
            return None

        extras = self._get_extras_from_item(item)
        return extras.get('pac')

    def extract_reference_list(self, path_in_wpt):
        """Extracts reference information of the specified (print) reference test.

        The return value is a list of (match/not-match, reference path in wpt)
        pairs, like:
           [("==", "/foo/bar/baz-match.html"),
            ("!=", "/foo/bar/baz-mismatch.html")]
        """
        items = self.raw_dict.get('items', {})
        test_type = self.get_test_type(path_in_wpt)

        if test_type not in ['reftest', 'print-reftest']:
            return []

        reftest_list = []
        for item in items[test_type][path_in_wpt]:
            for ref_path_in_wpt, expectation in item[1]:
                # Ref URLs in MANIFEST should be absolute, but we double check
                # just in case.
                if not ref_path_in_wpt.startswith('/'):
                    ref_path_in_wpt = '/' + ref_path_in_wpt
                reftest_list.append((expectation, ref_path_in_wpt))
        return reftest_list

    def extract_fuzzy_metadata(self, url):
        """Extracts the fuzzy reftest metadata for the specified (print) reference test.

        Although WPT supports multiple fuzzy references for a given test (one
        for each reference file), blinkpy only supports a single reference per
        test. As such, we just return the first fuzzy reference that we find.

        FIXME: It is possible for the references and the fuzzy metadata to be
        listed in different orders, which would then make our 'choose first'
        logic incorrect. Instead we should return a dictionary and let our
        caller select the reference being used.

        See https://web-platform-tests.org/writing-tests/reftests.html#fuzzy-matching

        Args:
            url: A WPT URL.

        Returns:
            A pair of lists representing the maxDifference and totalPixel ranges
            for the test. If the test isn't a reference test or doesn't have
            fuzzy information, a pair of Nones are returned.
        """

        items = self.raw_dict.get('items', {})
        test_type = self.get_test_type(url)
        if test_type not in ['reftest', 'print-reftest']:
            return None, None

        for item in items[test_type][url]:
            # Each item is a list of [url, refs, properties], and the fuzzy
            # metadata is stored in the properties dict.
            if 'fuzzy' not in item[2]:
                return None, None
            fuzzy_metadata_list = item[2]['fuzzy']
            for fuzzy_metadata in fuzzy_metadata_list:
                # The fuzzy metadata is a nested list of [url, [maxDifference,
                # maxPixels]].
                assert len(
                    fuzzy_metadata[1]
                ) == 2, 'Malformed fuzzy ref data for {}'.format(url)
                return fuzzy_metadata[1]
        return None, None

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
        """Regenerates the WPT MANIFEST.json file.

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
            _log.debug('Removing existing manifest file "%s".', manifest_path)
            fs.remove(manifest_path)

        # TODO(crbug.com/853815): perhaps also cache the manifest for wpt_internal.
        #
        # `url_base` should match those of `web_tests/wptrunner.blink.ini` (or
        # the implicit root `/` URL base).
        if path.startswith('external'):
            base_manifest_path = fs.join(port.web_tests_dir(), 'external',
                                         BASE_MANIFEST_NAME)
            if fs.exists(base_manifest_path):
                _log.debug('Copying base manifest from "%s" to "%s".',
                           base_manifest_path, manifest_path)
                fs.copyfile(base_manifest_path, manifest_path)
            else:
                _log.error('Manifest base not found at "%s".',
                           base_manifest_path)
            url_base = '/'
        elif path.startswith('wpt_internal'):
            url_base = '/wpt_internal/'

        WPTManifest.generate_manifest(port, wpt_path, url_base)

        if fs.isfile(manifest_path):
            _log.info(
                f'Manifest generation completed for {url_base!r} ({path})')
        else:
            _log.error(
                f'Manifest generation failed for {url_base!r} ({path}); '
                'creating an empty MANIFEST.json...')
            fs.write_text_file(manifest_path, '{}')

    @staticmethod
    def generate_manifest(port, dest_path, url_base: str = '/'):
        """Generates MANIFEST.json on the specified directory."""
        wpt_exec_path = PathFinder(
            port.host.filesystem).path_from_chromium_base(
                'third_party', 'wpt_tools', 'wpt', 'wpt')
        cmd = [
            port.python3_command(),
            wpt_exec_path,
            'manifest',
            '-v',
            '--no-download',
            f'--tests-root={dest_path}',
            f'--url-base={url_base}',
        ]

        # ScriptError will be raised if the command fails.
        output = port.host.executive.run_command(
            cmd,
            timeout_seconds=600,
            # This will also include stderr in the exception message.
            return_stderr=True)
        if output:
            _log.debug('Output: %s', output)

    @staticmethod
    def _flatten_items(items):
        """Flattens the 'items' object of a v8 manifest to the v7 format.

        The v8 manifest is a trie, where each level is a directory. The v7
        format, which the blinkpy code was written around, uses flat list:

        {
            "items": {
                "crashtest": {
                    "dir1/dir2/filename1": [manifest items],
                    "dir1/dir2/filename2": [manifest items],
                    ...
                },
                "manual": {...},
                "reftest": {...},
                "print-reftest": {...},
                "testharness": {...}
            },
            // other info...
        }

        Args:
            items: an 'items' entry in the v8 trie format.

        Returns:
            The input data, rewritten to the v7 flat-list format.
        """

        def _handle_node(test_type_items, node, path):
            """Recursively walks the trie, converting to the flat format.

            Args:
                test_type_items: the root dictionary for the current test type
                    (e.g. 'testharness'). Will be updated by this function with
                    new entries for any files found.
                node: the current node in the trie
                path: the accumulated filepath so far
            """
            assert isinstance(node, dict)

            for k, v in node.items():
                # WPT urls are always joined by '/', even on Windows.
                new_path = k if not path else path + '/' + k

                # Leafs (files) map to a list rather than a dict, e.g.
                #     'filename.html': [
                #       'git object ID',
                #       [manifest item],
                #       [manifest item],
                #     ],
                if isinstance(v, list):
                    # A file should be unique, and it should always contain both
                    # a git object ID and at least one manifest item (which may
                    # be empty).
                    assert new_path not in test_type_items
                    assert len(v) >= 2

                    # We have no use for the git object ID.
                    manifest_items = v[1:]
                    for manifest_item in manifest_items:
                        # As an optimization, the v8 manifest will omit the URL
                        # if it is the same as the filepath. The v7 manifest did
                        # not, so restore that information.
                        if len(manifest_item) and manifest_item[0] is None:
                            manifest_item[0] = new_path
                    test_type_items[new_path] = manifest_items
                else:
                    # Otherwise, we should be at a directory and so can recurse.
                    _handle_node(test_type_items, v, new_path)

        new_items = {}
        for test_type, value in items.items():
            test_type_items = {}
            _handle_node(test_type_items, value, '')
            new_items[test_type] = test_type_items

        return new_items
