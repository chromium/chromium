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
from typing import (
    Any,
    Dict,
    List,
    Literal,
    NamedTuple,
    Optional,
    Sequence,
    Tuple,
)

from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder

_log = logging.getLogger(__name__)

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


Relation = Literal['==', '!=']
Reference = Tuple[str, Relation]

FuzzyRange = Tuple[int, int]
FuzzyParameters = Tuple[Optional[FuzzyRange], Optional[FuzzyRange]]


class _Test(NamedTuple):
    """A container for per-test information."""
    # To save space, `file_path` is `None` if it's identical to the URL, which
    # it is for most tests.
    file_path: Optional[str]
    test_type: str
    references: List[Reference]
    extras: Dict[str, Any]

    @property
    def slow(self) -> bool:
        return self.extras.get('timeout') == 'long'

    @property
    def pac(self) -> Optional[str]:
        return self.extras.get('pac')

    @property
    def fuzzy_params(self) -> FuzzyParameters:
        params = self.extras.get('fuzzy')
        if not params:
            return None, None
        _, (max_diff, total_pixels) = params[0]
        assert len(max_diff) == 2, max_diff
        assert len(total_pixels) == 2, total_pixels
        return max_diff, total_pixels

    @property
    def jsshell(self) -> bool:
        """Whether this manifest item is a jsshell test.

        "jsshell" is one of the scopes automatically generated from .any.js
        tests. It is intended to run in a thin JavaScript shell instead of a
        full browser, so we usually ignore it in web tests. (crbug.com/871950)
        """
        return self.extras.get('jsshell', False)


class WPTManifest:
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
                 raw_dict,
                 wpt_dir: str,
                 test_types: Optional[Sequence[str]] = None,
                 exclude_jsshell: bool = True):
        self._raw_dict = raw_dict
        self.wpt_dir = wpt_dir
        self.test_types = test_types or (
            'manual',
            'reftest',
            'print-reftest',
            'testharness',
            'crashtest',
        )
        self._tests_by_url = {}
        self._exclude_jsshell = exclude_jsshell

        items = self._raw_dict.get('items', {})
        for test_type in self.test_types:
            self._map_tests(test_type, items.get(test_type, {}))

    def _map_tests(self, test_type: str, trie, path: str = ''):
        """Record tests present in a trie for some test type.

        Arguments:
            test_type: The WPT test type.
            trie: Either:
              * A list, which represents a test file (a leaf in the trie).
              * A map representing a test directory. It maps the next path
                component to the corresponding child.
            path: The path so far to this test file or directory.

        Note:
            When constructing the external manifest, this recursive walk must
            traverse all 50k+ items, so it impacts the startup performance of
            many tools.
        """
        if isinstance(trie, dict):
            for component, child in trie.items():
                # URLs always use `/` for path separators. Don't add a leading
                # `/`, since that's the convention in `blinkpy` for test paths.
                child_path = f'{path}/{component}' if path else component
                self._map_tests(test_type, child, child_path)
            return

        assert len(trie) >= 2, f'{trie!r} must contain at least one test'
        # Ignore the first element, which is the file's Git tree ID.
        for url, *maybe_refs, extras in trie[1:]:
            assert len(maybe_refs) <= 1, f'extra item data: {maybe_refs!r}'
            refs = maybe_refs[0] if maybe_refs else []
            # To save space, the v8 manifest omits the URL if it's
            # identical to the file path, which it is for most tests.
            if url:
                # Trim any leading `/`, which WPT URLs use by convention.
                if url.startswith('/'):
                    url = url[1:]
                test = _Test(path, test_type, refs, extras)
            else:
                url, test = path, _Test(None, test_type, refs, extras)
            assert url not in self._tests_by_url, f'duplicate URL {url!r}'
            if not self._exclude_jsshell or not test.jsshell:
                self._tests_by_url[url] = test

    @classmethod
    def from_file(cls,
                  port,
                  manifest_path: str,
                  test_types: Optional[Sequence[str]] = None,
                  exclude_jsshell: bool = True) -> 'WPTManifest':
        fs = port.host.filesystem
        with fs.open_text_file_for_reading(manifest_path) as manifest_file:
            raw_dict = json.load(manifest_file)
        return cls(raw_dict,
                   fs.dirname(fs.relpath(manifest_path, port.web_tests_dir())),
                   test_types, exclude_jsshell)

    @memoized
    def all_urls(self):
        """Returns a set of the URLs for all items in the manifest."""
        return frozenset(self._tests_by_url)

    def get_test_type(self, url: str) -> Optional[str]:
        """Returns the test type of the given test file path."""
        assert not url.startswith('/')
        test = self._tests_by_url.get(url)
        return test and test.test_type

    def is_test_file(self, file_path: str) -> bool:
        """Checks if file_path is a test file according to the manifest."""
        assert not file_path.startswith('/')
        components = file_path.split('/')
        assert components, file_path
        tries_by_type = self._raw_dict.get('items', {})
        return any(
            self._contains_file(tries_by_type.get(test_type, {}), components)
            for test_type in self.test_types)

    def _contains_file(self, trie, components: Sequence[str]) -> bool:
        """Determine if a test trie contains a test file."""
        if isinstance(trie, list):
            # Not a test file if there are still components at a leaf.
            return not bool(components)
        if not components:
            # This is a test directory, not a test file.
            return False
        next_component, *rest = components
        child = trie.get(next_component)
        return bool(child) and self._contains_file(child, rest)

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
        test = self._tests_by_url.get(url)
        return test.slow if test else False

    def extract_test_pac(self, url):
        """Get the proxy configuration (PAC) for the test

        Args:
            url: A WPT URL.

        Returns:
            A relative PAC url if noted by the test, None otherwise.
        """
        test = self._tests_by_url.get(url)
        return test and test.pac

    def extract_reference_list(self, url: str) -> List[Tuple[Relation, str]]:
        """Extracts reference information of the specified (print) reference test.

        The return value is a list of (match/not-match, reference path in wpt)
        pairs, like:
           [("==", "/foo/bar/baz-match.html"),
            ("!=", "/foo/bar/baz-mismatch.html")]
        """
        test = self._tests_by_url.get(url)
        if not test:
            return []
        return [(relation, ref) for ref, relation in test.references]

    def extract_fuzzy_metadata(self, url: str) -> FuzzyParameters:
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
        test = self._tests_by_url.get(url)
        test_type = self.get_test_type(url)
        if test_type not in {'reftest', 'print-reftest'}:
            return None, None
        return test.fuzzy_params

    def file_path_for_test_url(self, url: str) -> Optional[str]:
        """Finds the file path for the given test URL.

        Args:
            url: a WPT test URL.

        Returns:
            The path to the file containing this test URL, or None if not found.
        """
        test = self._tests_by_url.get(url)
        return (test.file_path or url) if test else None

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

        def is_cog() -> bool:
            """Checks the environment is cog."""
            return fs.getcwd().startswith('/google/cog/cloud')

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
                # TODO(https://github.com/web-platform-tests/wpt/issues/47350):
                # This is a workaround to handle the hanging issue when running
                # WPTs in Cider. Not updating WPT manifest usually is not OK
                # unless the change is not fundamental.
                # We should eventually allow partial update to the manifest.
                if is_cog():
                    _log.info('Skip manifest generation in Cog.')
                    return
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
        # This will also include stderr in the exception message.
        output = port.host.executive.run_command(cmd, timeout_seconds=600)
        if output:
            _log.debug('Output: %s', output)
