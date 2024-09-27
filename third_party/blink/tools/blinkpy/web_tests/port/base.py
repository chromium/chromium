# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Abstract base class for Port classes.

The Port classes encapsulate Port-specific (platform-specific) behavior
in the web test infrastructure.
"""

import collections
import hashlib
import json
import logging
import optparse
import re
import sys
import tempfile
import time
from collections import defaultdict
from copy import deepcopy
from datetime import datetime
from typing import List, Literal, NamedTuple, Optional, Set, Tuple

import six
from six.moves import zip_longest

from urllib.parse import urljoin

from blinkpy.common import exit_codes
from blinkpy.common import find_files
from blinkpy.common import read_checksum_from_png
from blinkpy.common import path_finder
from blinkpy.common.memoized import memoized
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.path import abspath_to_uri
from blinkpy.w3c.wpt_manifest import (
    FuzzyRange,
    FuzzyParameters,
    WPTManifest,
    MANIFEST_NAME,
)
from blinkpy.web_tests.layout_package.bot_test_expectations import BotTestExpectationsFactory
from blinkpy.web_tests.models.test_configuration import TestConfiguration
from blinkpy.web_tests.models.test_run_results import TestRunException
from blinkpy.web_tests.models.typ_types import (
    TestExpectations,
    ResultType,
    SerializableTypHost,
)
from blinkpy.web_tests.port import driver
from blinkpy.web_tests.port import server_process
from blinkpy.web_tests.port.factory import PortFactory
from blinkpy.web_tests.servers import apache_http
from blinkpy.web_tests.servers import pywebsocket
from blinkpy.web_tests.servers import wptserve

_log = logging.getLogger(__name__)

# Path relative to the build directory.
CONTENT_SHELL_FONTS_DIR = "test_fonts"

FONT_FILES = [
    [[CONTENT_SHELL_FONTS_DIR], 'Ahem.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Arimo-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Cousine-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'DejaVuSans.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'GardinerModBug.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'GardinerModCat.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Garuda.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Gelasio-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Lohit-Devanagari.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Lohit-Gurmukhi.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Lohit-Tamil.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'MuktiNarrow.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'NotoColorEmoji.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'NotoSansCJK-VF.otf.ttc', None],
    [[CONTENT_SHELL_FONTS_DIR], 'NotoSansKhmer-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'NotoSansSymbols2-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'NotoSansTibetan-Regular.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-Bold.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-BoldItalic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-Italic.ttf', None],
    [[CONTENT_SHELL_FONTS_DIR], 'Tinos-Regular.ttf', None],
]

# This is the fingerprint of wpt's certificate found in
# `//third_party/wpt_tools/certs/127.0.0.1.pem`. The following line is updated
# by `//third_party/wpt_tools/update_certs.py`.
WPT_FINGERPRINT = 'Nxvaj3+bY3oVrTc+Jp7m3E3sB1n3lXtnMDCyBsqEXiY='
# One for `//third_party/wpt_tools/certs/127.0.0.1.sxg.pem` used by non-WPT
# tests under `web_tests/http/`.
SXG_FINGERPRINT = '55qC1nKu2A88ESbFmk5sTPQS/ScG+8DD7P+2bgFA9iM='
# And one for external/wpt/signed-exchange/resources/127.0.0.1.sxg.pem
SXG_WPT_FINGERPRINT = '0Rt4mT6SJXojEMHTnKnlJ/hBKMBcI4kteBlhR1eTTdk='

# A convervative rule for names that are valid for file or directory names.
VALID_FILE_NAME_REGEX = re.compile(r'^[\w\-=]+$')

# This sub directory will be inside the results directory and it will
# contain all the disc artifacts created by web tests
ARTIFACTS_SUB_DIR = 'layout-test-results'

ARCHIVED_RESULTS_LIMIT = 25

ENABLE_THREADED_COMPOSITING_FLAG = '--enable-threaded-compositing'
DISABLE_THREADED_COMPOSITING_FLAG = '--disable-threaded-compositing'
DISABLE_THREADED_ANIMATION_FLAG = '--disable-threaded-animation'


class BaselineLocation(NamedTuple):
    """A representation of a baseline that may exist on disk."""
    virtual_suite: str = ''
    platform: str = ''
    flag_specific: str = ''

    @property
    def root(self) -> bool:
        # Also check that this baseline is not flag-specific. A flag-specific
        # suite implies a platform, even without `platform/*/` in its path.
        return not self.platform and not self.flag_specific

    def __str__(self) -> str:
        parts = []
        if self.virtual_suite:
            parts.append('virtual/%s' % self.virtual_suite)
        if self.platform:
            parts.append(self.platform)
        elif self.flag_specific:
            parts.append(self.flag_specific)
        if not parts:
            parts.append('(generic)')
        return ':'.join(parts)


class Port(object):
    """Abstract class for Port-specific hooks for the web_test package."""

    # Subclasses override this. This should indicate the basic implementation
    # part of the port name, e.g., 'mac', 'win', 'gtk'; there is one unique
    # value per class.
    # FIXME: Rename this to avoid confusion with the "full port name".
    port_name = None

    # Test paths use forward slash as separator on all platforms.
    TEST_PATH_SEPARATOR = '/'

    ALL_BUILD_TYPES = ('debug', 'release')

    CONTENT_SHELL_NAME = 'content_shell'
    CHROME_NAME = 'chrome'
    HEADLESS_SHELL_NAME = 'headless_shell'

    # Update the first line in third_party/blink/web_tests/TestExpectations and
    # the documentation in docs/testing/web_test_expectations.md when this list
    # changes.
    ALL_SYSTEMS = (
        ('mac11', 'x86'),
        ('mac11-arm64', 'arm64'),
        ('mac12', 'x86_64'),
        ('mac12-arm64', 'arm64'),
        ('mac13', 'x86_64'),
        ('mac13-arm64', 'arm64'),
        ('mac14', 'x86_64'),
        ('mac14-arm64', 'arm64'),
        ('mac15', 'x86_64'),
        ('mac15-arm64', 'arm64'),
        ('win10.20h2', 'x86'),
        ('win11-arm64', 'arm64'),
        ('win11', 'x86_64'),
        ('linux', 'x86_64'),
        ('fuchsia', 'x86_64'),
        ('ios17-simulator', 'x86_64'),
    )

    CONFIGURATION_SPECIFIER_MACROS = {
        'mac': [
            'mac11', 'mac11-arm64', 'mac12', 'mac12-arm64', 'mac13',
            'mac13-arm64', 'mac14', 'mac14-arm64', 'mac15', 'mac15-arm64'
        ],
        'ios': ['ios17-simulator'],
        'win': ['win10.20h2', 'win11-arm64', 'win11'],
        'linux': ['linux'],
        'fuchsia': ['fuchsia'],
    }

    # List of ports open on the host that the tests will connect to. When tests
    # run on a separate machine (Android and Fuchsia) these ports need to be
    # forwarded back to the host.
    # 8000, 8080 and 8443 are for http/https tests;
    # 8880 is for websocket tests (see apache_http.py and pywebsocket.py).
    # 8001, 8081, 8444, and 8445 are for http/https WPT;
    # 9001 and 9444 are for websocket WPT (see wptserve.py).
    SERVER_PORTS = [8000, 8001, 8080, 8081, 8443, 8444, 8445, 8880, 9001, 9444]

    FALLBACK_PATHS = {}

    SUPPORTED_VERSIONS = []

    # URL to the build requirements page.
    BUILD_REQUIREMENTS_URL = ''

    # The suffixes of baseline files (not extensions).
    BASELINE_SUFFIX = '-expected'
    BASELINE_MISMATCH_SUFFIX = '-expected-mismatch'

    # All of the non-reftest baseline extensions we use.
    BASELINE_EXTENSIONS = ('.wav', '.txt', '.png')

    FLAG_EXPECTATIONS_PREFIX = 'FlagExpectations'

    # The following two constants must match. When adding a new WPT root, also
    # remember to update configurations in:
    #     //third_party/blink/web_tests/external/wpt/config.json
    #     //third_party/blink/web_tests/wptrunner.blink.ini
    #
    # WPT_DIRS maps WPT roots on the file system to URL prefixes on wptserve.
    # The order matters: '/' MUST be the last URL prefix.
    WPT_DIRS = collections.OrderedDict([
        ('wpt_internal', '/wpt_internal/'),
        ('external/wpt', '/'),
    ])
    # WPT_REGEX captures: 1. the root directory of WPT relative to web_tests
    # (without a trailing slash), 2. the path of the test within WPT (without a
    # leading slash).
    WPT_REGEX = re.compile(
        r'^(?:virtual/[^/]+/)?(external/wpt|wpt_internal)/(.*)$')

    # This regex parses the WPT-style style fuzzy match syntax. For actual WPT
    # tests, this is not needed since this information is contained in the
    # manifest. However, we reuse this syntax for some non-WPT tests as well.
    WPT_FUZZY_REGEX = re.compile(
        r'<(?:html:)?meta\s+name=(?:fuzzy|"fuzzy")\s+content='
        r'"(?:(.+):)?(?:\s*maxDifference\s*=\s*)?(?:(\d+)-)?(\d+);(?:\s*totalPixels\s*=\s*)?(?:(\d+)-)?(\d+)"\s*/?>'
    )

    # Pattern for detecting testharness tests from their contents. Like
    # `WPT_FUZZY_REGEX`, this pattern is only used for non-WPT tests. The
    # manifest supplies this information for WPTs.
    _TESTHARNESS_PATTERN = re.compile(
        r'<script\s.*src=.*resources/testharness\.js.*>', re.IGNORECASE)

    # Add fully-qualified test names here to generate per-test traces.
    #
    # To generate traces for a limited set of tests running on CI bots, upload
    # a patch that adds entries to TESTS_TO_TRACE and do a CQ dry run.
    #
    # To generate traces when running locally, either modify TESTS_TO_TRACE or
    # specify --enable-per-test-tracing to generate traces for all tests.
    TESTS_TO_TRACE = set([])

    # Because this is an abstract base class, arguments to functions may be
    # unused in this class - pylint: disable=unused-argument

    @classmethod
    def latest_platform_fallback_path(cls):
        return cls.FALLBACK_PATHS[cls.SUPPORTED_VERSIONS[-1]]

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        """Return a fully-specified port name that can be used to construct objects."""
        # Subclasses will usually override this.
        assert port_name.startswith(cls.port_name)
        return port_name

    def __init__(self, host, port_name, options=None, **kwargs):

        # This value is the "full port name", and may be different from
        # cls.port_name by having version modifiers appended to it.
        self._name = port_name

        # These are default values that should be overridden in a subclasses.
        self._version = ''
        self._architecture = 'x86'

        # FIXME: Ideally we'd have a package-wide way to get a well-formed
        # options object that had all of the necessary options defined on it.
        self._options = deepcopy(options) or optparse.Values()

        self.host = host
        self._executive = host.executive
        self._filesystem = host.filesystem
        self._path_finder = path_finder.PathFinder(host.filesystem)

        self._http_server = None
        self._websocket_server = None
        self._wpt_server = None
        self._image_differ = None
        self.server_process_constructor = server_process.ServerProcess  # This can be overridden for testing.
        self._http_lock = None  # FIXME: Why does this live on the port object?
        self._dump_reader = None
        # This is a map of the form dir->[all skipped tests in that dir]
        # It is used to optimize looking up for a test, as it allows a quick look up of the test dir
        # while still using the "startwith" function to match with a single entry
        self._skip_base_test_map = defaultdict(list)

        # Configuration and target are always set by PortFactory so this is only
        # relevant in cases where a Port is created without it (testing mostly).
        if not hasattr(options, 'configuration') or not options.configuration:
            self.set_option_default('configuration',
                                    self.default_configuration())
        if not hasattr(options, 'target') or not options.target:
            self.set_option_default('target', self._options.configuration)
        # set the default to make unit tests happy
        if not hasattr(options, 'wpt_only'):
            self.set_option_default('wpt_only', False)
        if not hasattr(options, 'no_virtual_tests'):
            self.set_option_default('virtual_tests', True)
        else:
            self.set_option_default('virtual_tests',
                                    not options.no_virtual_tests)
        self._test_configuration = None
        self._results_directory = None
        self._used_expectation_files = None

    def __str__(self):
        return 'Port{name=%s, version=%s, architecture=%s, test_configuration=%s}' % (
            self._name, self._version, self._architecture,
            self._test_configuration)

    def version(self):
        return self._version

    def get_platform_tags(self):
        """Returns system condition tags that are used to find active expectations
           for a test run on a specific system"""
        return frozenset([
            self._options.configuration.lower(), self._version, self.port_name,
            self._architecture
        ])

    @memoized
    def flag_specific_config_name(self):
        """Returns the name of the flag-specific configuration if it's specified in
           --flag-specific option, or None. The name must be defined in
           FlagSpecificConfig or an AssertionError will be raised.
        """
        config_name = self.get_option('flag_specific')
        if config_name:
            configs = self.flag_specific_configs()
            assert config_name in configs, '{} is not defined in FlagSpecificConfig'.format(
                config_name)
            return config_name
        return None

    @memoized
    def flag_specific_configs(self):
        """Reads configuration from FlagSpecificConfig and returns a dictionary from name to args."""
        config_file = self._filesystem.join(self.web_tests_dir(),
                                            'FlagSpecificConfig')
        if not self._filesystem.exists(config_file):
            return {}

        try:
            json_configs = json.loads(
                self._filesystem.read_text_file(config_file))
        except ValueError as error:
            raise ValueError('{} is not a valid JSON file: {}'.format(
                config_file, error))

        configs = {}
        for config in json_configs:
            name = config['name']
            args = config['args']
            smoke_file = config.get('smoke_file')
            if not VALID_FILE_NAME_REGEX.match(name):
                raise ValueError(
                    '{}: name "{}" contains invalid characters'.format(
                        config_file, name))
            if name in configs:
                raise ValueError('{} contains duplicated name {}.'.format(
                    config_file, name))
            if args in [x for x, _ in configs.values()]:
                raise ValueError(
                    '{}: name "{}" has the same args as another entry.'.format(
                        config_file, name))
            configs[name] = (args, smoke_file)
        return configs

    def _specified_additional_driver_flags(self):
        """Returns the list of additional driver flags specified by the user in
           the following ways, concatenated:
           1. Flags in web_tests/additional-driver-flag.setting.
           2. flags expanded from --flag-specific=<name> based on flag-specific config.
           3. Zero or more flags passed by --additional-driver-flag.
        """
        flags = []
        flag_file = self._filesystem.join(self.web_tests_dir(),
                                          'additional-driver-flag.setting')
        if self._filesystem.exists(flag_file):
            flags = self._filesystem.read_text_file(flag_file).split()

        flag_specific_option = self.flag_specific_config_name()
        if flag_specific_option:
            flags += self.flag_specific_configs()[flag_specific_option][0]

        flags += self.get_option('additional_driver_flag', [])
        return flags

    def additional_driver_flags(self) -> List[str]:
        """Get extra switches to apply to all tests in this run.

        Note on layering: Only hardcode switches here if they're useful for all
        embedders and test scenarios (e.g., `//content/public/` switches).
        Embedder-specific switches should be added to the corresponding
        `Driver.cmd_line()` implementation.
        """
        flags = self._specified_additional_driver_flags()
        known_fingerprints = [
            WPT_FINGERPRINT,
            SXG_FINGERPRINT,
            SXG_WPT_FINGERPRINT,
        ]
        flags.extend([
            '--ignore-certificate-errors-spki-list=' +
            ','.join(known_fingerprints),
            # Required for WebTransport tests.
            '--webtransport-developer-mode',
        ])
        return flags

    def supports_per_test_timeout(self):
        return False

    def _default_timeout_ms(self):
        return 6000

    def timeout_ms(self):
        timeout_ms = self._default_timeout_ms()
        if self.get_option('configuration') == 'Debug':
            # Debug is about 5x slower than Release.
            return 5 * timeout_ms
        if self._build_has_dcheck_always_on():
            # Release with DCHECK is also slower than pure Release.
            return 2 * timeout_ms
        return timeout_ms

    @memoized
    def _build_args_gn_content(self):
        args_gn_file = self.build_path('args.gn')
        if not self._filesystem.exists(args_gn_file):
            _log.error('Unable to find %s', args_gn_file)
            return ''
        return self._filesystem.read_text_file(args_gn_file)

    @memoized
    def _build_has_dcheck_always_on(self):
        contents = self._build_args_gn_content()
        return bool(
            re.search(r'^\s*dcheck_always_on\s*=\s*true\s*(#.*)?$', contents,
                      re.MULTILINE))

    @memoized
    def _build_is_chrome_branded(self):
        chrome_branded = self.get_option('chrome_branded')
        if chrome_branded:
            return bool(chrome_branded)
        contents = self._build_args_gn_content()
        return bool(
            re.search(r'^\s*is_chrome_branded\s*=\s*true\s*(#.*)?$', contents,
                      re.MULTILINE))

    def driver_stop_timeout(self):
        """Returns the amount of time in seconds to wait before killing the process in driver.stop()."""
        # We want to wait for at least 3 seconds, but if we are really slow, we
        # want to be slow on cleanup as well (for things like ASAN, Valgrind, etc.)
        return (3.0 * float(self.get_option('timeout_ms', '0')) /
                self._default_timeout_ms())

    def default_batch_size(self):
        """Returns the default batch size to use for this port."""
        if self.get_option('enable_sanitizer'):
            # ASAN/MSAN/TSAN use more memory than regular content_shell. Their
            # memory usage may also grow over time, up to a certain point.
            # Relaunching the driver periodically helps keep it under control.
            return 40
        # The default batch size now is 100, to battle against resource leak.
        return 100

    def default_child_processes(self):
        """Returns the number of child processes to use for this port."""
        return self._executive.cpu_count()

    def default_max_locked_shards(self):
        """Returns the number of "locked" shards to run in parallel (like the http tests)."""
        max_locked_shards = int(self.default_child_processes()) // 4
        if not max_locked_shards:
            return 1
        return max_locked_shards

    def baseline_version_dir(self):
        """Returns the absolute path to the platform-and-version-specific results."""
        baseline_search_paths = self.baseline_search_path()
        return baseline_search_paths[0]

    def baseline_flag_specific_dir(self):
        """If --flag-specific is specified, returns the absolute path to the flag-specific
           platform-independent results. Otherwise returns None."""
        config_name = self.flag_specific_config_name()
        if not config_name:
            return None
        return self._filesystem.join(self.web_tests_dir(), 'flag-specific',
                                     config_name)

    def baseline_search_path(self):
        return (self.get_option('additional_platform_directory', []) +
                self._flag_specific_baseline_search_path() +
                self._compare_baseline() +
                list(self.default_baseline_search_path()))

    def default_baseline_search_path(self):
        """Returns a list of absolute paths to directories to search under for baselines.

        The directories are searched in order.
        """
        return map(self._absolute_baseline_path,
                   self.FALLBACK_PATHS[self.version()])

    @memoized
    def _compare_baseline(self):
        factory = PortFactory(self.host)
        target_port = self.get_option('compare_port')
        if target_port:
            return factory.get(target_port).default_baseline_search_path()
        return []

    def _check_file_exists(self,
                           path_to_file,
                           file_description,
                           override_step=None,
                           more_logging=True):
        """Verifies that the file is present where expected, or logs an error.

        Args:
            file_name: The (human friendly) name or description of the file
                you're looking for (e.g., "HTTP Server"). Used for error logging.
            override_step: An optional string to be logged if the check fails.
            more_logging: Whether or not to log the error messages.

        Returns:
            True if the file exists, else False.
        """
        if not self._filesystem.exists(path_to_file):
            if more_logging:
                _log.error('Unable to find %s', file_description)
                _log.error('    at %s', path_to_file)
                if override_step:
                    _log.error('    %s', override_step)
                    _log.error('')
            return False
        return True

    def check_build(self, needs_http, printer):
        if not self._check_file_exists(self.path_to_driver(), 'test driver'):
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if not self._check_driver_build_up_to_date(
                self.get_option('configuration')):
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if not self._check_file_exists(self._path_to_image_diff(),
                                       'image_diff'):
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if self._dump_reader and not self._dump_reader.check_is_functional():
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        if needs_http and not self.check_httpd():
            return exit_codes.UNEXPECTED_ERROR_EXIT_STATUS

        return exit_codes.OK_EXIT_STATUS

    def check_sys_deps(self):
        """Checks whether the system is properly configured.

        Most checks happen during invocation of the driver prior to running
        tests. This can be overridden to run custom checks.

        Returns:
            An exit status code.
        """
        return exit_codes.OK_EXIT_STATUS

    def check_httpd(self):
        httpd_path = self.path_to_apache()
        if httpd_path:
            try:
                env = self.setup_environ_for_server()
                if self._executive.run_command(
                    [httpd_path, '-v'], env=env, return_exit_code=True) != 0:
                    _log.error('httpd seems broken. Cannot run http tests.')
                    return False
                return True
            except OSError as e:
                _log.error('while trying to run: ' + httpd_path)
                _log.error('httpd launch error: ' + repr(e))
        _log.error('No httpd found. Cannot run http tests.')
        return False

    def do_text_results_differ(self, expected_text, actual_text):
        return expected_text != actual_text

    def do_audio_results_differ(self, expected_audio, actual_audio):
        return expected_audio != actual_audio

    def diff_image(self,
                   expected_contents,
                   actual_contents,
                   max_channel_diff=None,
                   max_pixels_diff=None):
        """Compares two images and returns an (image diff, error string) pair.

        If an error occurs (like image_diff isn't found, or crashes), we log an
        error and return True (for a diff).
        """
        # If only one of them exists, return that one.
        if not actual_contents and not expected_contents:
            return (None, None, None)
        if not actual_contents:
            return (expected_contents, None, None)
        if not expected_contents:
            return (actual_contents, None, None)

        tempdir = self._filesystem.mkdtemp()

        expected_filename = self._filesystem.join(str(tempdir), 'expected.png')
        self._filesystem.write_binary_file(expected_filename,
                                           expected_contents)

        actual_filename = self._filesystem.join(str(tempdir), 'actual.png')
        self._filesystem.write_binary_file(actual_filename, actual_contents)

        diff_filename = self._filesystem.join(str(tempdir), 'diff.png')

        executable = self._path_to_image_diff()
        # Although we are handed 'old', 'new', image_diff wants 'new', 'old'.
        command = [
            executable, '--diff', actual_filename, expected_filename,
            diff_filename
        ]
        # Notifies image_diff to allow a tolerance when calculating the pixel
        # diff. To account for variances when the tests are ran on an actual
        # GPU.
        if self.get_option('fuzzy_diff'):
            command.append('--fuzzy-diff')
        # The max_channel_diff and max_pixels_diff arguments are used by WPT
        # tests for fuzzy reftests. See
        # https://web-platform-tests.org/writing-tests/reftests.html#fuzzy-matching
        if max_channel_diff is not None:
            command.append('--fuzzy-max-channel-diff={}'.format('-'.join(
                map(str, max_channel_diff))))
        if max_pixels_diff is not None:
            command.append('--fuzzy-max-pixels-diff={}'.format('-'.join(
                map(str, max_pixels_diff))))

        result = None
        stats = None
        err_str = None

        def handle_output(output):
            if output:
                match = re.search(
                    "Found pixels_different: (\d+), max_channel_diff: (\d+)",
                    output)
                _log.debug(output)

                if match:
                    return {
                        "maxDifference": int(match.group(2)),
                        "totalPixels": int(match.group(1))
                    }
            return None

        try:
            output = self._executive.run_command(command)
            stats = handle_output(output)
        except ScriptError as error:
            if error.exit_code == 1:
                result = self._filesystem.read_binary_file(diff_filename)
                stats = handle_output(error.output)
            else:
                err_str = 'Image diff returned an exit code of %s. See http://crbug.com/278596' % error.exit_code
        except OSError as error:
            err_str = 'error running image diff: %s' % error
        finally:
            self._filesystem.rmtree(str(tempdir))

        return (result, stats, err_str or None)

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        product = self.get_option('product')
        if product == 'chrome':
            return self.CHROME_NAME
        elif product == 'headless_shell':
            return self.HEADLESS_SHELL_NAME
        return self.CONTENT_SHELL_NAME

    def expected_baselines_by_extension(self, test_name):
        """Returns a dict mapping baseline suffix to relative path for each baseline in a test.

        For reftests, it returns ".==" or ".!=" instead of the suffix.
        """
        # FIXME: The name similarity between this and expected_baselines()
        # below, is unfortunate. We should probably rename them both.
        baseline_dict = {}
        reference_files = self.reference_files(test_name)
        if reference_files:
            # FIXME: How should this handle more than one type of reftest?
            baseline_dict['.' + reference_files[0][0]] = \
                self.relative_test_filename(reference_files[0][1])

        for extension in self.BASELINE_EXTENSIONS:
            path = self.expected_filename(
                test_name, extension, return_default=False)
            baseline_dict[extension] = self.relative_test_filename(
                path) if path else path

        return baseline_dict

    def output_filename(self, test_name, suffix, extension):
        """Generates the output filename for a test.

        This method gives a proper filename for various outputs of a test,
        including baselines and actual results. Usually, the output filename
        follows the pattern: test_name_without_ext+suffix+extension, but when
        the test name contains query strings, e.g. external/wpt/foo.html?wss,
        test_name_without_ext is mangled to be external/wpt/foo_wss.

        It is encouraged to use this method instead of writing another mangling.

        Args:
            test_name: The name of a test.
            suffix: A suffix string to add before the extension
                (e.g. "-expected").
            extension: The extension of the output file (starting with .).

        Returns:
            A string, the output filename.
        """
        # WPT names might contain query strings, e.g. external/wpt/foo.html?wss,
        # in which case we mangle test_name_root (the part of a path before the
        # last extension point) to external/wpt/foo_wss, and the output filename
        # becomes external/wpt/foo_wss-expected.txt.
        index = test_name.find('?')
        if index != -1:
            test_name_root, _ = self._filesystem.splitext(test_name[:index])
            query_part = test_name[index:]
            test_name_root += self._filesystem.sanitize_filename(query_part)
        else:
            test_name_root, _ = self._filesystem.splitext(test_name)
        return test_name_root + suffix + extension

    def parse_output_filename(
            self, baseline_path: str) -> Tuple[BaselineLocation, str]:
        """Parse a baseline path into its virtual/platform/flag-specific pieces.

        Note that this method doesn't validate that the underlying baseline
        exists and corresponds to a real test.

        Arguments:
            baseline_path: Absolute or relative path to an `*-expected.*` file.

        Returns:
            A `BaselineLocation` with the parsed parameters, and the rest of
            the baseline's relative path.

        Raises:
            ValueError: If the provided path is a non-web test absolute path.
        """
        if self._filesystem.isabs(baseline_path):
            if baseline_path.startswith(self.web_tests_dir()):
                baseline_path = self._filesystem.relpath(
                    baseline_path, self.web_tests_dir())
            else:
                raise ValueError(
                    f'{baseline_path!r} is not under `web_tests/`')

        parts = baseline_path.split(self._filesystem.sep)
        platform = flag_specific = virtual_suite = ''
        if len(parts) >= 2:
            if parts[0] == 'platform':
                platform, parts = parts[1], parts[2:]
            elif parts[0] == 'flag-specific':
                flag_specific, parts = parts[1], parts[2:]
        if len(parts) >= 2 and parts[0] == 'virtual':
            virtual_suite, parts = parts[1], parts[2:]
        base_path = self._filesystem.join(*parts) if parts else ''
        location = BaselineLocation(virtual_suite, platform, flag_specific)
        return location, base_path

    @memoized
    def test_from_output_filename(self, baseline_path: str) -> Optional[str]:
        """Derive the test corresponding to a baseline.

        Arguments:
            baseline_path: Path to a generic baseline relative to `web_tests/`
                (i.e., not platform- or flag-specific). The baseline does not
                need to exist on the filesystem. A virtual baseline resolves
                to the corresponding virtual test.

        Returns:
            The test name, if found, and `None` otherwise.
        """
        stem, extension = self._filesystem.splitext(baseline_path)
        if stem.endswith(self.BASELINE_SUFFIX):
            suffix = self.BASELINE_SUFFIX
        elif stem.endswith(self.BASELINE_MISMATCH_SUFFIX):
            suffix = self.BASELINE_MISMATCH_SUFFIX
        else:
            return None

        # This is a fast path for the common case of `.html` test files.
        maybe_test = stem[:-len(suffix)] + '.html'
        if self.tests([maybe_test]) == [maybe_test]:
            return maybe_test

        test_dir = self._filesystem.dirname(stem)
        # `tests()` can be arbitrarily slow, but there's no better way to do the
        # inversion because `output_filename()` sanitizes the original test name
        # lossily.
        for test in self.tests([test_dir]):
            if baseline_path == self.output_filename(test, suffix, extension):
                return test
        return None

    def expected_baselines(self,
                           test_name,
                           extension,
                           all_baselines=False,
                           match=True):
        """Given a test name, finds where the baseline results are located.

        Return values will be in the format appropriate for the current
        platform (e.g., "\\" for path separators on Windows). If the results
        file is not found, then None will be returned for the directory,
        but the expected relative pathname will still be returned.

        This routine is generic but lives here since it is used in
        conjunction with the other baseline and filename routines that are
        platform specific.

        Args:
            test_name: Name of test file (usually a relative path under web_tests/)
            extension: File extension of the expected results, including dot;
                e.g. '.txt' or '.png'.  This should not be None, but may be an
                empty string.
            all_baselines: If True, return an ordered list of all baseline paths
                for the given platform. If False, return only the first one.
            match: Whether the baseline is a match or a mismatch.

        Returns:
            A list of (baseline_dir, results_filename) pairs, where
                baseline_dir - abs path to the top of the results tree (or test
                    tree)
                results_filename - relative path from top of tree to the results
                    file
                (port.join() of the two gives you the full path to the file,
                    unless None was returned.)
        """
        baseline_filename = self.output_filename(
            test_name,
            self.BASELINE_SUFFIX if match else self.BASELINE_MISMATCH_SUFFIX,
            extension)
        baseline_search_path = self.baseline_search_path()

        baselines = []
        for baseline_dir in baseline_search_path:
            if self._filesystem.exists(
                    self._filesystem.join(baseline_dir, baseline_filename)):
                baselines.append((baseline_dir, baseline_filename))

            if not all_baselines and baselines:
                return baselines

        # If it wasn't found in a platform directory, return the expected
        # result in the test directory.
        baseline_dir = self.web_tests_dir()
        if self._filesystem.exists(
                self._filesystem.join(baseline_dir, baseline_filename)):
            baselines.append((baseline_dir, baseline_filename))

        if baselines:
            return baselines

        return [(None, baseline_filename)]

    def expected_filename(self,
                          test_name,
                          extension,
                          return_default=True,
                          fallback_base_for_virtual=True,
                          match=True):
        """Given a test name, returns an absolute path to its expected results.

        If no expected results are found in any of the searched directories,
        the directory in which the test itself is located will be returned.
        The return value is in the format appropriate for the platform
        (e.g., "\\" for path separators on windows).

        This routine is generic but is implemented here to live alongside
        the other baseline and filename manipulation routines.

        Args:
            test_name: Name of test file (usually a relative path under web_tests/)
            extension: File extension of the expected results, including dot;
                e.g. '.txt' or '.png'.  This should not be None, but may be an
                empty string.
            return_default: If True, returns the path to the generic expectation
                if nothing else is found; if False, returns None.
            fallback_base_for_virtual: For virtual test only. When no virtual
                specific baseline is found, if this parameter is True, fallback
                to find baselines of the base test; if False, depending on
                |return_default|, returns the generic virtual baseline or None.
            match: Whether the baseline is a match or a mismatch.

        Returns:
            An absolute path to its expected results, or None if not found.
        """
        # The [0] means the first expected baseline (which is the one to be
        # used) in the fallback paths.
        baseline_dir, baseline_filename = self.expected_baselines(
            test_name, extension, match=match)[0]
        if baseline_dir:
            return self._filesystem.join(baseline_dir, baseline_filename)

        if fallback_base_for_virtual:
            actual_test_name = self.lookup_virtual_test_base(test_name)
            if actual_test_name:
                return self.expected_filename(actual_test_name,
                                              extension,
                                              return_default,
                                              match=match)

        if return_default:
            return self._filesystem.join(self.web_tests_dir(),
                                         baseline_filename)
        return None

    def fallback_expected_filename(self, test_name, extension):
        """Given a test name, returns an absolute path to its next fallback baseline.
        Args:
            same as expected_filename()
        Returns:
            An absolute path to the next fallback baseline, or None if not found.
        """
        baselines = self.expected_baselines(
            test_name, extension, all_baselines=True)
        if len(baselines) < 2:
            actual_test_name = self.lookup_virtual_test_base(test_name)
            if actual_test_name:
                if len(baselines) == 0:
                    return self.fallback_expected_filename(
                        actual_test_name, extension)
                # In this case, baselines[0] is the current baseline of the
                # virtual test, so the first base test baseline is the fallback
                # baseline of the virtual test.
                return self.expected_filename(
                    actual_test_name, extension, return_default=False)
            return None

        baseline_dir, baseline_filename = baselines[1]
        if baseline_dir:
            return self._filesystem.join(baseline_dir, baseline_filename)
        return None

    def expected_checksum(self, test_name):
        """Returns the checksum of the image we expect the test to produce,
        or None if it is a text-only test.
        """
        png_path = self.expected_filename(test_name, '.png')

        if self._filesystem.exists(png_path):
            with self._filesystem.open_binary_file_for_reading(
                    png_path) as filehandle:
                return read_checksum_from_png.read_checksum(filehandle)

        return None

    def expected_image(self, test_name):
        """Returns the image we expect the test to produce."""
        baseline_path = self.expected_filename(test_name, '.png')
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def expected_audio(self, test_name):
        baseline_path = self.expected_filename(test_name, '.wav')
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def expected_text(self, test_name):
        """Returns the text output we expect the test to produce, or None
        if we don't expect there to be any text output.

        End-of-line characters are normalized to '\n'.
        """
        # FIXME: DRT output is actually utf-8, but since we don't decode the
        # output from DRT (instead treating it as a binary string), we read the
        # baselines as a binary string, too.
        baseline_path = self.expected_filename(test_name, '.txt')
        if not self._filesystem.exists(baseline_path):
            return None
        text = self._filesystem.read_binary_file(baseline_path)
        return text.replace(b'\r\n', b'\n')

    def expected_subtest_failure(self, test_name):
        baseline = self.expected_text(test_name)
        if baseline:
            baseline = baseline.decode('utf8', 'replace')
            if re.search(r"^(FAIL|NOTRUN|TIMEOUT)", baseline, re.MULTILINE):
                return True
        return False

    def expected_harness_error(self, test_name):
        baseline = self.expected_text(test_name)
        if baseline:
            baseline = baseline.decode('utf8', 'replace')
            if re.search(r"^Harness Error\.", baseline, re.MULTILINE):
                return True
        return False

    def reference_files(self, test_name):
        """Returns a list of expectation (== or !=) and filename pairs"""

        # Try to find -expected.* or -expected-mismatch.* in the same directory.
        reftest_list = []
        for expectation in ('==', '!='):
            for extension in Port.supported_file_extensions:
                path = self.expected_filename(test_name,
                                              extension,
                                              match=(expectation == '=='))
                if self._filesystem.exists(path):
                    reftest_list.append((expectation, path))
        if reftest_list:
            return reftest_list

        # Try to extract information from MANIFEST.json.
        match = self.WPT_REGEX.match(test_name)
        if not match:
            return []
        wpt_path = match.group(1)
        path_in_wpt = match.group(2)
        for expectation, ref_path_in_wpt in self.wpt_manifest(
                wpt_path).extract_reference_list(path_in_wpt):
            if ref_path_in_wpt.startswith('about:'):
                ref_absolute_path = ref_path_in_wpt
            else:
                if 'external/wpt' in wpt_path:
                    ref_path_in_web_tests = wpt_path + ref_path_in_wpt
                else:
                    # References in this manifest are already generated with
                    # `/wpt_internal` in the URL. Remove the leading '/' for
                    # joining.
                    ref_path_in_web_tests = ref_path_in_wpt[1:]
                ref_absolute_path = self._filesystem.join(
                    self.web_tests_dir(), ref_path_in_web_tests)
            reftest_list.append((expectation, ref_absolute_path))
        return reftest_list

    def max_allowed_failures(self, num_tests):
        return (self._options.exit_after_n_failures
                or max(5000, num_tests // 2))

    def max_allowed_crash_or_timeouts(self, num_tests):
        return (self._options.exit_after_n_crashes_or_timeouts
                or max(100, num_tests // 33))

    def tests(self, paths=None):
        """Returns all tests or tests matching supplied paths.

        Args:
            paths: Array of paths to match. If supplied, this function will only
                return tests matching at least one path in paths.

        Returns:
            A list of concrete test URLs. Each URL usually corresponds to a
            file under `web_tests/`, but not always [0, 1, 2].

        [0]: https://web-platform-tests.org/writing-tests/testharness.html#variants
        [1]: https://web-platform-tests.org/writing-tests/testharness.html#tests-for-other-or-multiple-globals-any-js
        [2]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_tests.md#Virtual-test-suites
        """
        tests = self.real_tests(paths)

        # TODO(crbug.com/338295229): Consolidate the code paths for all/explicit
        # tests so that they're less likely to diverge.
        if paths:
            if self._options.virtual_tests:
                tests.extend(self._virtual_tests_matching_paths(paths))
            if (any(wpt_path in path for wpt_path in self.WPT_DIRS
                    for path in paths)
                    # TODO(robertma): Remove this special case when external/wpt is moved to wpt.
                    or any('external' in path for path in paths)):
                tests.extend(self._wpt_test_urls_matching_paths(paths))
        else:
            # '/' is used instead of filesystem.sep as the WPT manifest always
            # uses '/' for paths (it is not OS dependent).
            wpt_tests = [
                wpt_path + '/' + test for wpt_path in self.WPT_DIRS
                for test in self.wpt_manifest(wpt_path).all_urls()
            ]
            tests_by_dir = defaultdict(list)
            for test in tests + wpt_tests:
                dirname = self._filesystem.dirname(test) + '/'
                tests_by_dir[dirname].append(test)

            if self._options.virtual_tests:
                tests.extend(self._all_virtual_tests(tests_by_dir))
            tests.extend(wpt_tests)
        return tests

    def real_tests_from_dict(self, paths, tests_by_dir):
        """Find all real tests in paths, using results saved in dict."""
        files = []
        for path in paths:
            # Some WPT files can expand to multiple tests, and the file itself
            # is not a test so it is not in tests_by_dir. Do special handling
            # when we find a WPT URL, file, or directory in virtual bases.
            if any(path.startswith(wpt_dir) for wpt_dir in self.WPT_DIRS):
                files.extend(self._wpt_test_urls_matching_paths([path]))
                continue
            if self._has_supported_extension_for_all(path):
                # only append the file when it is in tests_by_dir
                dirname = self._filesystem.dirname(path) + '/'
                if path in tests_by_dir.get(dirname, []):
                    files.append(path)
                continue
            path = path + '/' if path[-1] != '/' else path
            for key, value in tests_by_dir.items():
                if key.startswith(path):
                    files.extend(value)
        return files

    def real_tests(self, paths):
        """Find all real tests in paths except WPT."""
        if self._options.wpt_only:
            return []

        # When collecting test cases, skip these directories.
        skipped_directories = set([
            'platform', 'resources', 'support', 'script-tests', 'reference',
            'reftest', 'TestLists'
        ])
        # Also ignore all WPT directories. Note that this is only an
        # optimization; is_non_wpt_test_file should skip WPT regardless.
        skipped_directories |= set(self.WPT_DIRS)
        files = find_files.find(self._filesystem, self.web_tests_dir(), paths, skipped_directories,
                                lambda _, dirname, filename: self.is_non_wpt_test_file(dirname, filename),
                                self.test_key)
        return [self.relative_test_filename(f) for f in files]

    @staticmethod
    def is_reference_html_file(filesystem, dirname, filename):
        # TODO(robertma): We probably do not need prefixes/suffixes other than
        # -expected{-mismatch} any more. Or worse, there might be actual tests
        # with these prefixes/suffixes.
        if filename.startswith('ref-') or filename.startswith('notref-'):
            return True
        filename_without_ext, _ = filesystem.splitext(filename)
        for suffix in ['-expected', '-expected-mismatch', '-ref', '-notref']:
            if filename_without_ext.endswith(suffix):
                return True
        return False

    # When collecting test cases, we include any file with these extensions.
    supported_file_extensions = set([
        '.html',
        '.xml',
        '.xhtml',
        '.xht',
        '.pl',
        '.htm',
        '.php',
        '.svg',
        '.mht',
        '.pdf',
    ])

    def _has_supported_extension_for_all(self, filename):
        extension = self._filesystem.splitext(filename)[1]
        if 'inspector-protocol' in filename and extension == '.js':
            return True
        if 'devtools' in filename and extension == '.js':
            return True
        return extension in self.supported_file_extensions

    def _has_supported_extension(self, filename):
        """Returns True if filename is one of the file extensions we want to run a test on."""
        extension = self._filesystem.splitext(filename)[1]
        return extension in self.supported_file_extensions

    def is_non_wpt_test_file(self, dirname, filename):
        # Convert dirname to a relative path to web_tests with slashes
        # normalized and ensure it has a trailing slash.
        normalized_test_dir = self.relative_test_filename(
            dirname) + self.TEST_PATH_SEPARATOR
        if any(
                normalized_test_dir.startswith(d + self.TEST_PATH_SEPARATOR)
                for d in self.WPT_DIRS):
            return False
        extension = self._filesystem.splitext(filename)[1]
        if 'inspector-protocol' in dirname and extension == '.js':
            return True
        if 'devtools' in dirname and extension == '.js':
            return True
        return (self._has_supported_extension(filename)
                and not Port.is_reference_html_file(self._filesystem, dirname,
                                                    filename))

    @memoized
    def wpt_manifest(self,
                     path: str,
                     exclude_jsshell: bool = True) -> WPTManifest:
        assert path in self.WPT_DIRS
        # Convert '/' to the platform-specific separator.
        path = self._filesystem.normpath(path)
        self._filesystem.maybe_make_directory(
            self._filesystem.join(self.web_tests_dir(), path))
        manifest_path = self._filesystem.join(self.web_tests_dir(), path,
                                              MANIFEST_NAME)
        if self.should_update_manifest(path):
            _log.debug('Generating MANIFEST.json for %s...', path)
            WPTManifest.ensure_manifest(self, path)
        return WPTManifest.from_file(self, manifest_path,
                                     self.get_option('test_types'),
                                     exclude_jsshell)

    def should_update_manifest(self, path: Literal[WPT_DIRS]) -> bool:
        """Check if a WPT manifest should be updated.

        If no `--{no-,}manifest-update` switch is explicitly passed, then guess
        if an update is needed by checking if a hash of the WPT directory's
        contents changed from the last update. The previous hash is cached on
        the filesystem.
        """
        manifest_path = self._path_finder.path_from_web_tests(
            path, MANIFEST_NAME)
        if not self._filesystem.exists(manifest_path):
            return True
        manifest_update: Optional[bool] = self.get_option('manifest_update')
        # Use an explicit manifest setting, if given. `None` will guess if the
        # manifest needs an update.
        #
        # TODO(crbug.com/)1411505: If this logic holds up for
        # `lint_test_expectations.py`, consider activating this for other
        # `blinkpy` tools by defaulting to `manifest_update=None`.
        if manifest_update is not None:
            return manifest_update

        # `.wptcache` is the default cache directory for `wpt manifest`, and
        # it's gitignored by `//third_party/wpt_tools/wpt/.gitignore`.
        last_digest_file = self._path_finder.path_from_chromium_base(
            'third_party', 'wpt_tools', 'wpt', '.wptcache', path, 'digest')
        self._filesystem.maybe_make_directory(
            self._filesystem.dirname(last_digest_file))
        try:
            last_digest = self._filesystem.read_text_file(last_digest_file)
        except FileNotFoundError:
            last_digest = ''
        current_digest = self._wpt_digest(path)
        if current_digest != last_digest:
            self._filesystem.write_text_file(last_digest_file, current_digest)
            return True
        return False

    def _wpt_digest(self, path: Literal[WPT_DIRS]) -> str:
        """Create a digest of the given WPT directory's contents.

        The hashing scheme is designed so that a change in the contents of a
        non-gitignored file under the WPT directory implies a different digest,
        which in turn implies a manifest update. The hash preimage consists of
        the current revision for the WPT tree object, followed by uncommitted
        files (including untracked ones) and their own digests:

            5cf97fce0437ed53da24111f1451bfcef962db0c
            /path/to/external/wpt/staged.html:abbf720ea8b3f4db6331d534d5a0030c69781187
            /path/to/external/wpt/untracked.html:818e59092bd8e12bd2fb9c7d4775a4ce1ee816cb
            ...

        This skips manifest rebuilds when:
          * Changing non-WPT files (e.g., the browser code under test).
          * Rebasing with no WPT changes (the WPT revision stays the same).
            This will often skip updates for `wpt_internal/`, which doesn't
            receive many changes.
        """
        wpt_dir = self._path_finder.path_from_web_tests(path)
        pathspec = self._filesystem.relpath(
            wpt_dir, self._path_finder.path_from_chromium_base())
        # `git` uses forward slashes for pathspecs, even on Windows.
        pathspec = pathspec.replace(self._filesystem.sep, '/')
        git = self.host.git()
        # As of this writing, checking for tracked changes takes:
        #   * ~0.3s for `external/wpt`
        #   * ~0.2s for `wpt_internal`
        #
        # Checking for untracked changes takes:
        #   * ~0.4s for `external/wpt`
        #   * ~0.2s for `wpt_internal`
        #
        # `git rev-parse HEAD:<wpt-dir>` is <0.02s in both directories. In
        # total, the initial check for deciding whether to update the manifest
        # or not takes ~1.1s. Paying this fixed cost seems worthwhile to
        # almost always skip updating the `wpt_internal/` manifest (~1s), and
        # sometimes skip updating `external/wpt` (~10s).
        base_rev = git.run(['rev-parse', f'HEAD:{pathspec}'])
        tracked_files = git.changed_files(path=wpt_dir)
        untracked_files = git.run([
            'ls-files',
            '--other',
            '--exclude-standard',
            '-z',
            'HEAD',
            wpt_dir,
        ]).split('\x00')[:-1]

        hasher = hashlib.sha256()
        hasher.update(base_rev.encode())
        changed_files = map(self._path_from_chromium_base,
                            {*tracked_files, *untracked_files})
        for changed_file in sorted(changed_files):
            try:
                file_digest = self._filesystem.sha1(changed_file)
            except FileNotFoundError:
                file_digest = ''
            hasher.update(f'{changed_file}:{file_digest}\n'.encode())
        return hasher.hexdigest()

    @classmethod
    def split_wpt_dir(cls, test: str) -> Tuple[Optional[str], str]:
        """Split a test path into its WPT directory (if any) and the rest."""
        for wpt_dir in cls.WPT_DIRS:
            if test.startswith(wpt_dir):
                return wpt_dir, test[len(f'{wpt_dir}/'):]
        return None, test

    def is_wpt_file(self, path):
        """Returns whether a path is a WPT test file."""

        if self.WPT_REGEX.match(path):
            return self._filesystem.isfile(self.abspath_for_test(path))
        return False

    def is_wpt_crash_test(self, test_name):
        """Returns whether a WPT test is a crashtest.

        See https://web-platform-tests.org/writing-tests/crashtest.html.
        """
        match = self.WPT_REGEX.match(test_name)
        if not match:
            return False
        wpt_path = match.group(1)
        path_in_wpt = match.group(2)
        return self.wpt_manifest(wpt_path).is_crash_test(path_in_wpt)

    def is_manual_test(self, test_name):
        """Returns whether a WPT test is a manual."""
        match = self.WPT_REGEX.match(test_name)
        if not match:
            return False
        wpt_path = match.group(1)
        path_in_wpt = match.group(2)
        return self.wpt_manifest(wpt_path).is_manual_test(path_in_wpt)

    def is_wpt_print_reftest(self, test):
        """Returns whether a WPT test is a print reftest."""
        match = self.WPT_REGEX.match(test)
        if not match:
            return False

        wpt_path = match.group(1)
        path_in_wpt = match.group(2)
        return self.wpt_manifest(wpt_path).is_print_reftest(path_in_wpt)

    def is_slow_wpt_test(self, test_name):
        # When DCHECK is enabled, idlharness tests run 5-6x slower due to the
        # amount of JavaScript they use (most web_tests run very little JS).
        # This causes flaky timeouts for a lot of them, as a 0.5-1s test becomes
        # close to the default 6s timeout.
        if (self.is_wpt_idlharness_test(test_name)
                and self._build_has_dcheck_always_on()):
            return True

        match = self.WPT_REGEX.match(test_name)
        if not match:
            return False
        wpt_path = match.group(1)
        path_in_wpt = match.group(2)
        return self.wpt_manifest(wpt_path).is_slow_test(path_in_wpt)

    def is_testharness_test(self, test_name: str) -> bool:
        """Detect whether a test uses the testharness.js framework."""
        base_test = self.lookup_virtual_test_base(test_name) or test_name
        wpt_dir, path_from_root = self.split_wpt_dir(base_test)
        if wpt_dir:
            manifest = self.wpt_manifest(wpt_dir)
            return manifest.get_test_type(path_from_root) == 'testharness'
        maybe_test_contents = self.read_test(base_test, 'latin-1')
        return maybe_test_contents and bool(
            self._TESTHARNESS_PATTERN.search(maybe_test_contents))

    def extract_wpt_pac(self, test_name):
        match = self.WPT_REGEX.match(test_name)
        if not match:
            return None
        wpt_path = match.group(1)
        path_in_wpt = match.group(2)
        pac = self.wpt_manifest(wpt_path).extract_test_pac(path_in_wpt)
        if pac is None:
            return None

        hosts_and_ports = self.create_driver(0).WPT_HOST_AND_PORTS

        return urljoin(
            "http://{}:{}".format(hosts_and_ports[0], hosts_and_ports[1]),
            urljoin(path_in_wpt, pac))

    def get_wpt_fuzzy_metadata(self, test_name: str) -> FuzzyParameters:
        """Returns the WPT-style fuzzy metadata for the given test.

        The metadata is a pair of lists, (maxDifference, totalPixels), where
        each list is a [min, max] range, inclusive, adjusted by the device
        scale factor. If the test has no fuzzy metadata, returns (None, None).

        See https://web-platform-tests.org/writing-tests/reftests.html#fuzzy-matching
        """
        match = self.WPT_REGEX.match(test_name)

        if match:
            # This is an actual WPT test, so we can get the metadata from the manifest.
            wpt_path = match.group(1)
            path_in_wpt = match.group(2)
            return self._adjust_fuzzy_metadata_by_dsf(
                test_name,
                self.wpt_manifest(wpt_path).extract_fuzzy_metadata(
                    path_in_wpt))

        # This is not a WPT test, so we will parse the metadata ourselves.
        if not self.test_isfile(test_name):
            return (None, None)

        # We use a safe encoding because some test files are incompatible with utf-8.
        test_file = self.read_test(test_name, "latin-1")
        if not test_file:
            return (None, None)

        # We only take the first match which is in line with what we do for WPT tests.
        fuzzy_match = self.WPT_FUZZY_REGEX.search(test_file)
        if not fuzzy_match:
            return (None, None)

        _, max_diff_min, max_diff_max, tot_pix_min, tot_pix_max = \
            fuzzy_match.groups()
        if not max_diff_min:
            max_diff_min = max_diff_max
        if not tot_pix_min:
            tot_pix_min = tot_pix_max

        return self._adjust_fuzzy_metadata_by_dsf(
            test_name,
            ([int(max_diff_min), int(max_diff_max)
              ], [int(tot_pix_min), int(tot_pix_max)]))

    def _adjust_fuzzy_metadata_by_dsf(self, test_name, metadata):
        if metadata == (None, None):
            return metadata

        ([max_diff_min, max_diff_max], [tot_pix_min, tot_pix_max]) = metadata
        for flag in reversed(self._specified_additional_driver_flags() +
                             self.args_for_test(test_name)):
            if "--force-device-scale-factor=" in flag:
                _, scale_factor = flag.split("=")
                dsf = float(scale_factor)
                tot_pix_min = int(tot_pix_min * dsf * dsf)
                tot_pix_max = int(tot_pix_max * dsf * dsf)
                break

        return ([max_diff_min, max_diff_max], [tot_pix_min, tot_pix_max])

    def get_file_path_for_wpt_test(self, test_name):
        """Returns the real file path for the given WPT test.

        Or None if the test is not a WPT.
        """
        match = self.WPT_REGEX.match(test_name)
        if not match:
            return None
        wpt_path = match.group(1)
        path_in_wpt = match.group(2)
        file_path_in_wpt = self.wpt_manifest(wpt_path).file_path_for_test_url(
            path_in_wpt)
        if not file_path_in_wpt:
            return None
        return self._filesystem.join(wpt_path, file_path_in_wpt)

    def test_key(self, test_name):
        """Turns a test name into a pair of sublists: the natural sort key of the
        dirname, and the natural sort key of the basename.

        This can be used when sorting paths so that files in a directory.
        directory are kept together rather than being mixed in with files in
        subdirectories.
        """
        dirname, basename = self.split_test(test_name)
        return (self._natural_sort_key(dirname + self.TEST_PATH_SEPARATOR),
                self._natural_sort_key(basename))

    def _natural_sort_key(self, string_to_split):
        """Turns a string into a list of string and number chunks.

        For example: "z23a" -> ["z", 23, "a"]
        This can be used to implement "natural sort" order. See:
        http://www.codinghorror.com/blog/2007/12/sorting-for-humans-natural-sort-order.html
        http://nedbatchelder.com/blog/200712.html#e20071211T054956
        """

        def tryint(val):
            try:
                return int(val)
            except ValueError:
                return val

        return [tryint(chunk) for chunk in re.split(r'(\d+)', string_to_split)]

    def read_test(self, test_name, encoding="utf8"):
        """Returns the contents of the given Non WPT test according to the given encoding.
        If no corresponding file can be found, returns None instead.
        Warning: some tests are in utf8-incompatible encodings.
        """
        assert not self.WPT_REGEX.match(
            test_name), "read_test only works with legacy layout test"
        path = self.abspath_for_test(test_name)
        if self._filesystem.isfile(path):
            return self._filesystem.read_binary_file(path).decode(encoding)

        base = self.lookup_virtual_test_base(test_name)
        if not base:
            return None
        path = self.abspath_for_test(base)
        if self._filesystem.isfile(path):
            return self._filesystem.read_binary_file(path).decode(encoding)

        return None

    @memoized
    def test_isfile(self, test_name):
        """Returns True if the test name refers to an existing test file."""
        # Used by test_expectations.py to apply rules to a file.
        if self._filesystem.isfile(self.abspath_for_test(test_name)):
            return True
        base = self.lookup_virtual_test_base(test_name)
        return base and self._filesystem.isfile(self.abspath_for_test(base))

    @memoized
    def test_isdir(self, test_name):
        """Returns True if the test name refers to an existing directory of tests."""
        # Used by test_expectations.py to apply rules to whole directories.
        if self._filesystem.isdir(self.abspath_for_test(test_name)):
            return True
        base = self.lookup_virtual_test_base(test_name)
        return base and self._filesystem.isdir(self.abspath_for_test(base))

    @memoized
    def test_exists(self, test_name):
        """Returns True if the test name refers to an existing test directory or file."""
        # Used by lint_test_expectations.py to determine if an entry refers to a
        # valid test.
        if self.is_wpt_test(test_name):
            # A virtual WPT test must have valid virtual prefix and base.
            if test_name.startswith('virtual/'):
                return bool(self.lookup_virtual_test_base(test_name))
            # Otherwise treat any WPT test as existing regardless of their real
            # existence on the file system.
            # TODO(crbug.com/959958): Actually check existence of WPT tests.
            return True
        return self.test_isfile(test_name) or self.test_isdir(test_name)

    def split_test(self, test_name):
        """Splits a test name into the 'directory' part and the 'basename' part."""
        index = test_name.rfind(self.TEST_PATH_SEPARATOR)
        if index < 1:
            return ('', test_name)
        return (test_name[0:index], test_name[index:])

    @memoized
    def normalize_test_name(self, test_name):
        """Returns a normalized version of the test name or test directory."""
        if test_name.endswith('/'):
            return test_name
        if self.test_isdir(test_name):
            return test_name + '/'
        return test_name

    def driver_cmd_line(self):
        """Prints the DRT (DumpRenderTree) command that will be used."""
        return self.create_driver(0).cmd_line([])

    def update_baseline(self, baseline_path, data):
        """Updates the baseline for a test.

        Args:
            baseline_path: the actual path to use for baseline, not the path to
              the test. This function is used to update either generic or
              platform-specific baselines, but we can't infer which here.
            data: contents of the baseline.
        """
        self._filesystem.write_binary_file(baseline_path, data)

    def _path_from_chromium_base(self, *comps):
        return self._path_finder.path_from_chromium_base(*comps)

    def _perf_tests_dir(self):
        return self._path_finder.perf_tests_dir()

    def web_tests_dir(self):
        custom_web_tests_dir = self.get_option('layout_tests_directory')
        if custom_web_tests_dir:
            return self._filesystem.abspath(custom_web_tests_dir)
        return self._path_finder.web_tests_dir()

    def skips_test(self, test):
        """Checks whether the given test is skipped for this port.

        Returns True if:
          - the port runs smoke tests only and the test is not in the list
          - the test is marked as Skip in NeverFixTest
          - the test is a virtual test not intended to run on this platform.
        """
        return (self.skipped_due_to_smoke_tests(test)
                or self.skipped_in_never_fix_tests(test)
                or self.virtual_test_skipped_due_to_platform_config(test)
                or self.virtual_test_skipped_due_to_disabled(test)
                or self.skipped_due_to_exclusive_virtual_tests(test)
                or self.skipped_due_to_skip_base_tests(test))

    @memoized
    def tests_from_file(self, filename: str) -> Set[str]:
        tests = set()
        file_contents = self._filesystem.read_text_file(filename)
        for line in file_contents.splitlines():
            line = line.strip()
            if line.startswith('#') or not line:
                continue
            tests.add(line)
        return tests

    def skipped_due_to_manual_test(self, test_name):
        """Checks whether a manual test should be skipped."""
        base_test = self.lookup_virtual_test_base(test_name)
        if not base_test:
            base_test = test_name
        # TODO(crbug.com/346563686): remove this once we have testdriver api for
        # file-system-access
        if base_test.startswith("external/wpt/file-system-access/"):
            return False
        return self.is_manual_test(base_test)

    def skipped_due_to_smoke_tests(self, test):
        """Checks if the test is skipped based on the set of Smoke tests.

        Returns True if this port runs only smoke tests, and the test is not
        in the smoke tests file; returns False otherwise.
        """
        if not self.default_smoke_test_only():
            return False
        smoke_test_filename = self.path_to_smoke_tests_file()
        if not self._filesystem.exists(smoke_test_filename):
            return False
        smoke_tests = self.tests_from_file(smoke_test_filename)
        return test not in smoke_tests

    def default_smoke_test_only(self):
        config_name = self.flag_specific_config_name()
        if config_name:
            _, smoke_file = self.flag_specific_configs()[config_name]
            if smoke_file is None:
                return False
            if not self._filesystem.exists(
                    self._filesystem.join(self.web_tests_dir(), smoke_file)):
                _log.error('Unable to find smoke file(%s) for %s', smoke_file,
                           config_name)
                return False
            return True
        return False

    def path_to_smoke_tests_file(self):
        config_name = self.flag_specific_config_name()
        if config_name:
            _, smoke_file = self.flag_specific_configs()[config_name]
            return self._filesystem.join(self.web_tests_dir(), smoke_file)

        # Historically we only have one smoke tests list. That one now becomes
        # the default
        return self._filesystem.join(self.web_tests_dir(), 'TestLists',
                                     'Default.txt')

    @memoized
    def _never_fix_test_expectations(self):
        # Note: The parsing logic here (reading the file, constructing a
        # parser, etc.) is very similar to blinkpy/w3c/test_copier.py.
        path = self.path_to_never_fix_tests_file()
        contents = self._filesystem.read_text_file(path)
        test_expectations = TestExpectations(tags=self.get_platform_tags())
        test_expectations.parse_tagged_list(contents)
        return test_expectations

    def skipped_in_never_fix_tests(self, test):
        """Checks if the test is marked as Skip in NeverFixTests for this port.

        Skip in NeverFixTests indicate we will never fix the failure and
        permanently skip the test. Only Skip lines are allowed in NeverFixTests.
        Some lines in NeverFixTests are platform-specific.

        Note: this will not work with skipped directories. See also the same
        issue with update_all_test_expectations_files in test_importer.py.
        """
        test_expectations = self._never_fix_test_expectations()
        return ResultType.Skip in test_expectations.expectations_for(
            test).results

    def path_to_never_fix_tests_file(self):
        return self._filesystem.join(self.web_tests_dir(), 'NeverFixTests')

    def virtual_test_skipped_due_to_platform_config(self, test):
        """Checks if the virtual test is skipped based on the platform config.

        Returns True if the virtual test is not intend to run on this port, due
        to the platform config in VirtualTestSuites; returns False otherwise.
        """
        suite = self._lookup_virtual_suite(test)
        if suite is not None:
            return self.operating_system() not in suite.platforms
        return False

    def virtual_test_skipped_due_to_disabled(self, test):
        """Checks if the virtual test is skipped based on the 'disabled' config.

        Returns True if the virtual test is marked as disabled, due to config in
        VirtualTestSuites; returns False otherwise.
        """
        suite = self._lookup_virtual_suite(test)
        if suite is not None:
            return suite.disabled
        return False

    @memoized
    def skipped_due_to_exclusive_virtual_tests(self, test):
        """Checks if the test should be skipped due to the exclusive_tests rule
        of any virtual suite.

        If the test is not a virtual test, it will be skipped if it's in the
        exclusive_tests list of any virtual suite. If the test is a virtual
        test, it will be skipped if the base test is in the exclusive_tests list
        of any virtual suite, and the base test is not in the test's own virtual
        suite's exclusive_tests list.
        """
        base_test = self.lookup_virtual_test_base(test)
        if base_test:
            # For a virtual test, if the base test is in exclusive_tests of the
            # test's own virtual suite, then we should not skip the test.
            virtual_suite = self._lookup_virtual_suite(test)
            for exclusive_test in self._normalized_exclusive_tests(
                    virtual_suite):
                if base_test.startswith(exclusive_test):
                    return False
                if exclusive_test.startswith(base_test):
                    # This means base_test is a directory containing exclusive
                    # tests, so should not skip the directory.
                    return False
        else:
            base_test = self.normalize_test_name(test)

        # For a non-virtual test or a virtual test not listed in exclusive_tests
        # of the test's own virtual suite, we should skip the test if the base
        # test is in exclusive_tests of any virtual suite.
        for exclusive_test in self._all_normalized_exclusive_tests():
            if base_test.startswith(exclusive_test):
                return True
        return False

    @memoized
    def _all_normalized_exclusive_tests(self):
        tests = set()
        for suite in self.virtual_test_suites():
            tests.update(self._normalized_exclusive_tests(suite))
        return tests

    @memoized
    def _normalized_exclusive_tests(self, virtual_suite):
        tests = set()
        for exclusive_test in virtual_suite.exclusive_tests:
            normalized_test = self.normalize_test_name(exclusive_test)
            # WPT JS tests can expand to multiple tests. Remove the "js" suffix
            # so that generated variants (e.g. "test.any.worker.html") match
            # against the base with startswith and are correctly excluded.
            if self.is_wpt_test(normalized_test) and normalized_test.endswith(
                    '.js'):
                normalized_test = normalized_test[:-2]
            tests.add(normalized_test)
        return tests

    @memoized
    def skipped_due_to_skip_base_tests(self, test):
        """Checks if the test should be skipped due to the skip_base_test rule
        of any virtual suite.

        If the test is not a virtual test, it will be skipped if it's in the
        skip_base_test list of any virtual suite. If the test is a virtual
        test, it will not be skipped.
        """
        # This check doesn't apply to virtual tests
        if self.lookup_virtual_test_base(test):
            return False

        # Ensure that this was called at least once, to process all suites
        # information
        vts = self.virtual_test_suites()
        # Our approach of using a map keyed on paths will only work if the test name is not a directory.
        assert (not self._filesystem.isdir(test))
        dirname, _ = self.split_test(test)
        for skipped_base_test in self._skip_base_test_map.get(dirname, []):
            if test.startswith(skipped_base_test):
                return True
        return False

    def name(self):
        """Returns a name that uniquely identifies this particular type of port.

        This is the full port name including both base port name and version,
        and can be passed to PortFactory.get() to instantiate a port.
        """
        return self._name

    def operating_system(self):
        raise NotImplementedError

    def version(self):
        """Returns a string indicating the version of a given platform

        For example, "win10" or "linux". This is used to help identify the
        exact port when parsing test expectations, determining search paths,
        and logging information.
        """
        return self._version

    def architecture(self):
        return self._architecture

    def python3_command(self):
        """Returns the correct command to use to run python3.

        This exists because Windows has inconsistent behavior between the bots
        and local developer machines, such that determining which python3 name
        to use is non-trivial. See https://crbug.com/1155616.

        Once blinkpy runs under python3, this can be removed in favour of
        callers using sys.executable.
        """
        if six.PY3:
            # Prefer sys.executable when the current script runs under python3.
            # The current script might be running with vpython3 and in that case
            # using the same executable will share the same virtualenv.
            return sys.executable
        return 'python3'

    def get_option(self, name, default_value=None):
        return getattr(self._options, name, default_value)

    def set_option_default(self, name, default_value):
        return self._options.ensure_value(name, default_value)

    def relative_test_filename(self, filename):
        """Returns a Unix-style path for a filename relative to web_tests.

        Ports may legitimately return absolute paths here if no relative path
        makes sense.
        """
        # Ports that run on windows need to override this method to deal with
        # filenames with backslashes in them.
        if filename.startswith(self.web_tests_dir()):
            return self.host.filesystem.relpath(filename, self.web_tests_dir())
        else:
            return self.host.filesystem.abspath(filename)

    @memoized
    def abspath_for_test(self, test_name):
        """Returns the full path to the file for a given test name.

        This is the inverse of relative_test_filename().
        """
        return self._filesystem.join(self.web_tests_dir(), test_name)

    # This is used to generate tracing data covering the execution of a single
    # test case; it omits startup and shutdown time for the test binary.
    @memoized
    def trace_file_for_test(self, test):
        if (self.get_option('enable_per_test_tracing')
                or test in self.TESTS_TO_TRACE):
            basename = '{}.pftrace'.format(
                self._filesystem.sanitize_filename(test))
            return self._filesystem.join(tempfile.gettempdir(), basename)
        return None

    # This is used to generate tracing data covering the entire execution of the
    # test binary, including startup and shutdown time.
    @memoized
    def startup_trace_file_for_test(self, test_name):
        # Note that this method is memoized, so subsequent runs of a test will
        # overwrite this file.
        if not self.get_option('enable_tracing'):
            return None
        current_time = time.strftime("%Y-%m-%d-%H-%M-%S")
        return 'trace_layout_test_{}_{}.pftrace'.format(
            self._filesystem.sanitize_filename(test_name), current_time)

    @memoized
    def args_for_test(self, test_name):
        args = self._lookup_virtual_test_args(test_name)
        pac_url = self.extract_wpt_pac(test_name)
        if pac_url is not None:
            args.append("--proxy-pac-url=" + pac_url)

        # We run single-threaded by default (overriding content_shell's default,
        # which is to enable threading).
        #
        # But we use threading if we find --enable-threaded-compositing in
        # virtual test suite args, or if the user explicitly passed it as:
        #
        #   --additional-driver-flag=--enable-threaded-compositing
        #
        # (Note content_shell only understands --disable-threaded-compositing,
        # not --enable-threaded-compositing.)
        #
        if ENABLE_THREADED_COMPOSITING_FLAG not in (
                args + self._specified_additional_driver_flags()):
            args.append(DISABLE_THREADED_COMPOSITING_FLAG)
            args.append(DISABLE_THREADED_ANIMATION_FLAG)
        else:
            # Explicitly enabling threaded compositing takes precedence over
            # explicitly disabling it.
            if DISABLE_THREADED_COMPOSITING_FLAG in args:
                args.remove(DISABLE_THREADED_COMPOSITING_FLAG)
            if DISABLE_THREADED_ANIMATION_FLAG in args:
                args.remove(DISABLE_THREADED_ANIMATION_FLAG)

        # Always support running web tests using SwiftShader for compositing or WebGL
        args.append('--enable-unsafe-swiftshader')

        startup_trace_file = self.startup_trace_file_for_test(test_name)
        if startup_trace_file is not None:
            tracing_categories = self.get_option('enable_tracing')
            args.append('--trace-startup=' + tracing_categories)
            # Do not finish the trace until the test is finished.
            args.append('--trace-startup-duration=0')
            args.append('--trace-startup-file=' + startup_trace_file)

        return args

    @memoized
    def name_for_test(self, test_name):
        test_base = self.lookup_virtual_test_base(test_name)
        if test_base and not self._filesystem.exists(
                self.abspath_for_test(test_name)):
            return test_base
        return test_name

    def bot_test_times_path(self):
        # TODO(crbug.com/1030434): For the not_site_per_process_blink_web_tests step on linux,
        # an exception is raised when merging the bot times json files. This happens  whenever they
        # are outputted into the results directory. Temporarily we will return the bot times json
        # file relative to the target directory.
        return self.build_path('webkit_test_times', 'bot_times_ms.json')

    def results_directory(self):
        """Returns the absolute path directory which will store all web tests outputted
        files. It may include a sub directory for artifacts and it may store performance test results."""
        if not self._results_directory:
            option_val = self.get_option(
                'results_directory') or self.default_results_directory()
            assert not self._filesystem.basename(option_val) == 'layout-test-results', (
                'crbug.com/1026494, crbug.com/1027708: The layout-test-results sub directory should '
                'not be passed as part of the --results-directory command line argument.')
            self._results_directory = self._filesystem.abspath(option_val)
        return self._results_directory

    def artifacts_directory(self):
        """Returns path to artifacts sub directory of the results directory. This
        directory will store test artifacts, which may include actual and expected
        output from web tests."""
        return self._filesystem.join(self.results_directory(),
                                     ARTIFACTS_SUB_DIR)

    def perf_results_directory(self):
        return self.results_directory()

    def inspector_build_directory(self):
        if self._build_is_chrome_branded():
            return self.build_path('gen', 'third_party',
                                   'devtools-frontend-internal',
                                   'devtools-frontend', 'front_end')
        return self.build_path('gen', 'third_party', 'devtools-frontend',
                               'src', 'front_end')

    def generated_sources_directory(self):
        return self.build_path('gen')

    def apache_config_directory(self):
        return self._path_finder.path_from_blink_tools('apache_config')

    def default_results_directory(self):
        """Returns the absolute path to the build directory."""
        return self.build_path()

    @memoized
    def typ_host(self):
        return SerializableTypHost()

    def setup_test_run(self):
        """Performs port-specific work at the beginning of a test run."""
        # Delete the disk cache if any to ensure a clean test run.
        dump_render_tree_binary_path = self.path_to_driver()
        cachedir = self._filesystem.dirname(dump_render_tree_binary_path)
        cachedir = self._filesystem.join(cachedir, 'cache')
        if self._filesystem.exists(cachedir):
            self._filesystem.rmtree(cachedir)

        if self._dump_reader:
            self._filesystem.maybe_make_directory(
                self._dump_reader.crash_dumps_directory())

    def num_workers(self, requested_num_workers):
        """Returns the number of available workers (possibly less than the number requested)."""
        return requested_num_workers

    def child_kwargs(self):
        """Returns additional kwargs to pass to the Port objects in the worker processes.
        This can be used to transmit additional state such as initialized emulators.

        Note: these must be able to be pickled.
        """
        return {}

    def clean_up_test_run(self):
        """Performs port-specific work at the end of a test run."""
        if self._image_differ:
            self._image_differ.stop()
            self._image_differ = None

    def setup_environ_for_server(self):
        # We intentionally copy only a subset of the environment when
        # launching subprocesses to ensure consistent test results.
        clean_env = {}
        variables_to_copy = [
            'CHROME_DEVEL_SANDBOX',
            'CHROME_IPC_LOGGING',
            'ASAN_OPTIONS',
            'TSAN_OPTIONS',
            'MSAN_OPTIONS',
            'LSAN_OPTIONS',
            'UBSAN_OPTIONS',
            'VALGRIND_LIB',
            'VALGRIND_LIB_INNER',
            'TMPDIR',
        ]
        if 'TMPDIR' not in self.host.environ:
            self.host.environ['TMPDIR'] = tempfile.gettempdir()
        # CGIs are run directory-relative so they need an absolute TMPDIR
        self.host.environ['TMPDIR'] = self._filesystem.abspath(
            self.host.environ['TMPDIR'])
        if self.host.platform.is_linux() or self.host.platform.is_freebsd():
            variables_to_copy += [
                'XAUTHORITY', 'HOME', 'LANG', 'LD_LIBRARY_PATH',
                'DBUS_SESSION_BUS_ADDRESS', 'XDG_DATA_DIRS', 'XDG_RUNTIME_DIR'
            ]
            clean_env['DISPLAY'] = self.host.environ.get('DISPLAY', ':1')
        if self.host.platform.is_mac():
            variables_to_copy += [
                'HOME',
            ]
        if self.host.platform.is_win():
            variables_to_copy += [
                'PATH',
            ]

        for variable in variables_to_copy:
            if variable in self.host.environ:
                clean_env[variable] = self.host.environ[variable]

        for string_variable in self.get_option('additional_env_var', []):
            [name, value] = string_variable.split('=', 1)
            clean_env[name] = value

        if self.host.platform.is_linux() and not self.use_system_httpd():
            # set up LD_LIBRARY_PATH when we are using httpd built from 3pp.
            path_to_libs = self._filesystem.join(self.apache_server_root(), 'lib')
            if clean_env.get('LD_LIBRARY_PATH'):
                clean_env['LD_LIBRARY_PATH'] = path_to_libs + ':' + clean_env['LD_LIBRARY_PATH']
            else:
                clean_env['LD_LIBRARY_PATH'] = path_to_libs

        return clean_env

    def show_results_html_file(self, results_filename):
        """Displays the given HTML file in a user's browser."""
        return self.host.user.open_url(
            abspath_to_uri(self.host.platform, results_filename))

    def create_driver(self, worker_number, no_timeout=False):
        """Returns a newly created Driver subclass for starting/stopping the
        test driver.
        """
        return self._driver_class()(self, worker_number, no_timeout=no_timeout)

    def requires_http_server(self):
        # Does the port require an HTTP server for running tests? This could
        # be the case when the tests aren't run on the host platform.
        return False

    def start_http_server(self,
                          additional_dirs,
                          number_of_drivers,
                          output_dir=''):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a web server to be running.
        """
        assert not self._http_server, 'Already running an http server.'
        output_dir = output_dir or self.artifacts_directory()
        server = apache_http.ApacheHTTP(
            self,
            output_dir,
            additional_dirs=additional_dirs,
            number_of_servers=(number_of_drivers * 4))
        server.start()
        self._http_server = server

    def start_websocket_server(self, output_dir=''):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a websocket server to be running.
        """
        assert not self._websocket_server, 'Already running a websocket server.'
        output_dir = output_dir or self.artifacts_directory()
        server = pywebsocket.PyWebSocket(self, output_dir)
        server.start()
        self._websocket_server = server

    @staticmethod
    def is_wpt_test(test):
        """Whether a test is considered a web-platform-tests test."""
        return Port.WPT_REGEX.match(test)

    @staticmethod
    def is_wpt_idlharness_test(test_file):
        """Returns whether a WPT test is (probably) an idlharness test.

        There are no rules in WPT that can be used to identify idlharness tests
        without examining the file contents (which would be expensive). This
        method utilizes a filename heuristic, based on the convention of
        including 'idlharness' in the appropriate test names.
        """
        match = Port.WPT_REGEX.match(test_file)
        if not match:
            return False
        filename = match.group(2).split('/')[-1]
        return 'idlharness' in filename

    @staticmethod
    def should_use_wptserve(test):
        return Port.is_wpt_test(test)

    def start_wptserve(self, output_dir=''):
        """Starts a WPT web server.

        Raises an error if it can't start or is already running.
        """
        assert not self._wpt_server, 'Already running a WPT server.'
        output_dir = output_dir or self.artifacts_directory()
        # We currently don't support any output mechanism for the WPT server.
        server = wptserve.WPTServe(self, output_dir)
        server.start()
        self._wpt_server = server

    def stop_wptserve(self):
        """Shuts down the WPT server if it is running."""
        if self._wpt_server:
            self._wpt_server.stop()
            self._wpt_server = None

    def http_server_requires_http_protocol_options_unsafe(self):
        httpd_path = self.path_to_apache()
        intentional_syntax_error = 'INTENTIONAL_SYNTAX_ERROR'
        # yapf: disable
        cmd = [
            httpd_path,
            '-t',
            '-f', self.path_to_apache_config_file(),
            '-C', 'ServerRoot "%s"' % self.apache_server_root(),
            '-C', 'HttpProtocolOptions Unsafe',
            '-C', intentional_syntax_error
        ]
        # yapf: enable
        env = self.setup_environ_for_server()

        def error_handler(err):
            pass

        output = self._executive.run_command(
            cmd, env=env, error_handler=error_handler)
        # If apache complains about the intentional error, it apparently
        # accepted the HttpProtocolOptions directive, and we should add it.
        return intentional_syntax_error in output

    def http_server_supports_ipv6(self):
        # Apache < 2.4 on win32 does not support IPv6.
        return not self.host.platform.is_win()

    def stop_http_server(self):
        """Shuts down the http server if it is running."""
        if self._http_server:
            self._http_server.stop()
            self._http_server = None

    def stop_websocket_server(self):
        """Shuts down the websocket server if it is running."""
        if self._websocket_server:
            self._websocket_server.stop()
            self._websocket_server = None

    #
    # TEST EXPECTATION-RELATED METHODS
    #

    def test_configuration(self):
        """Returns the current TestConfiguration for the port."""
        if not self._test_configuration:
            self._test_configuration = TestConfiguration(
                self._version, self._architecture,
                self._options.configuration.lower())
        return self._test_configuration

    # FIXME: Belongs on a Platform object.
    @memoized
    def all_test_configurations(self):
        """Returns a list of TestConfiguration instances, representing all available
        test configurations for this port.
        """
        return self._generate_all_test_configurations()

    # FIXME: Belongs on a Platform object.
    def configuration_specifier_macros(self):
        """Ports may provide a way to abbreviate configuration specifiers to conveniently
        refer to them as one term or alias specific values to more generic ones. For example:

        (win10, win11) -> win # Abbreviate all Windows versions into one namesake.

        Returns a dictionary, each key representing a macro term ('win', for example),
        and value being a list of valid configuration specifiers (such as ['win10', 'win11']).
        """
        return self.CONFIGURATION_SPECIFIER_MACROS

    def _generate_all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for version, architecture in self.ALL_SYSTEMS:
            for build_type in self.ALL_BUILD_TYPES:
                test_configurations.append(
                    TestConfiguration(version, architecture, build_type))
        return test_configurations

    def _flag_specific_expectations_path(self):
        config_name = self.flag_specific_config_name()
        if config_name:
            return self.path_to_flag_specific_expectations_file(config_name)

    def _flag_specific_baseline_search_path(self):
        dir = self.baseline_flag_specific_dir()
        return [dir] if dir else []

    def expectations_dict(self):
        """Returns an OrderedDict of name -> expectations strings.

        The names are expected to be (but not required to be) paths in the
        filesystem. If the name is a path, the file can be considered updatable
        for things like rebaselining, so don't use names that are paths if
        they're not paths.

        Generally speaking the ordering should be files in the filesystem in
        cascade order (TestExpectations followed by Skipped, if the port honors
        both formats), then any built-in expectations (e.g., from compile-time
        exclusions), then --additional-expectations options.
        """
        # FIXME: rename this to test_expectations() once all the callers are
        # updated to know about the ordered dict.
        expectations = collections.OrderedDict()

        default_expectations_files = set(self.default_expectations_files())
        ignore_default = self.get_option('ignore_default_expectations', False)
        for path in self.used_expectations_files():
            is_default = path in default_expectations_files
            if ignore_default and is_default:
                continue
            path_exists = self._filesystem.exists(path)
            if is_default:
                if path_exists:
                    expectations[path] = self._filesystem.read_text_file(path)
            else:
                if path_exists:
                    _log.debug(
                        "reading additional_expectations from path '%s'", path)
                    expectations[path] = self._filesystem.read_text_file(path)
                else:
                    # TODO(weizhong): Fix additional expectation paths for
                    # not_site_per_process_blink_web_tests, then change this
                    # back to raising exceptions for incorrect expectation
                    # paths.
                    _log.warning(
                        "additional_expectations path '%s' does not exist",
                        path)
        return expectations

    def all_expectations_dict(self):
        """Returns an OrderedDict of name -> expectations strings."""
        expectations = self.expectations_dict()

        flag_path = self._filesystem.join(self.web_tests_dir(),
                                          self.FLAG_EXPECTATIONS_PREFIX)
        if not self._filesystem.exists(flag_path):
            return expectations

        for (_, _, filenames) in self._filesystem.walk(flag_path):
            for filename in filenames:
                if (filename.startswith('README') or filename.endswith('~')
                        or filename == 'PRESUBMIT.py'):
                    continue
                path = self._filesystem.join(flag_path, filename)
                try:
                    expectations[path] = self._filesystem.read_text_file(path)
                except UnicodeDecodeError:
                    _log.error('Failed to read expectations file: \'%s\'',
                               path)
                    raise

        return expectations

    def bot_expectations(self):
        if not self.get_option('ignore_flaky_tests'):
            return {}

        full_port_name = self.determine_full_port_name(
            self.host, self._options, self.port_name)
        builder_category = self.get_option('ignore_builder_category', 'layout')
        step_names = ['blink_web_tests', 'blink_wpt_tests']
        retval = {}
        for step_name in step_names:
            factory = BotTestExpectationsFactory(self.host.builders, step_name)
            # FIXME: This only grabs release builder's flakiness data. If we're running debug,
            # when we should grab the debug builder's data.
            expectations = factory.expectations_for_port(full_port_name,
                                                         builder_category)

            if not expectations:
                continue

            ignore_mode = self.get_option('ignore_flaky_tests')
            if ignore_mode == 'very-flaky' or ignore_mode == 'maybe-flaky':
                retval.update(expectations.flakes_by_path(ignore_mode == 'very-flaky'))
            elif ignore_mode == 'unexpected':
                retval.update(expectations.unexpected_results_by_path())
            else:
                _log.warning("Unexpected ignore mode: '%s'.", ignore_mode)

        return retval

    def default_expectations_files(self):
        """Returns a list of paths to expectations files that apply by default.

        There are other "test expectations" files that may be applied if
        the --additional-expectations flag is passed; those aren't included
        here.
        """
        return filter(None, [
            self.path_to_generic_test_expectations_file(),
            self._filesystem.join(self.web_tests_dir(), 'NeverFixTests'),
            self._filesystem.join(self.web_tests_dir(),
                                  'StaleTestExpectations'),
            self._filesystem.join(self.web_tests_dir(), 'SlowTests')
        ])

    def used_expectations_files(self):
        """Returns a list of paths to expectation files that are used."""
        if self._used_expectation_files is None:
            self._used_expectation_files = list(
                self.default_expectations_files())
            flag_specific = self._flag_specific_expectations_path()
            if flag_specific:
                self._used_expectation_files.append(flag_specific)
            for path in self.get_option('additional_expectations', []):
                expanded_path = self._filesystem.expanduser(path)
                abs_path = self._filesystem.abspath(expanded_path)
                self._used_expectation_files.append(abs_path)
        return self._used_expectation_files

    def extra_expectations_files(self):
        """Returns a list of paths to test expectations not loaded by default.

        These paths are passed via --additional-expectations on some builders.
        """
        return [
            self._filesystem.join(self.web_tests_dir(), 'ASANExpectations'),
            self._filesystem.join(self.web_tests_dir(), 'LeakExpectations'),
            self._filesystem.join(self.web_tests_dir(), 'MSANExpectations'),
        ]

    @memoized
    def path_to_generic_test_expectations_file(self):
        return self._filesystem.join(self.web_tests_dir(), 'TestExpectations')

    def path_to_flag_specific_expectations_file(self, flag_specific):
        return self._filesystem.join(self.web_tests_dir(),
                                     self.FLAG_EXPECTATIONS_PREFIX,
                                     flag_specific)

    def repository_path(self):
        """Returns the repository path for the chromium code base."""
        return self._path_from_chromium_base('build')

    def default_configuration(self):
        return 'Release'

    def _delete_dirs(self, dir_list):
        for dir_path in dir_list:
            self._filesystem.rmtree(dir_path)

    def rename_results_folder(self):
        try:
            timestamp = time.strftime(
                "%Y-%m-%d-%H-%M-%S",
                time.localtime(
                    self._filesystem.mtime(
                        self._filesystem.join(self.artifacts_directory(),
                                              'results.html'))))
        except OSError as error:
            # It might be possible that results.html was not generated in previous run, because the test
            # run was interrupted even before testing started. In those cases, don't archive the folder.
            # Simply override the current folder contents with new results.
            import errno
            if error.errno in (errno.EEXIST, errno.ENOENT):
                _log.info(
                    'No results.html file found in previous run, skipping it.')
            return None
        archived_name = ''.join(
            (self._filesystem.basename(self.artifacts_directory()), '_',
             timestamp))
        archived_path = self._filesystem.join(
            self._filesystem.dirname(self.artifacts_directory()),
            archived_name)
        self._filesystem.move(self.artifacts_directory(), archived_path)

    def _get_artifact_directories(self, artifacts_directory_path):
        results_directory_path = self._filesystem.dirname(
            artifacts_directory_path)
        file_list = self._filesystem.listdir(results_directory_path)
        results_directories = []
        for name in file_list:
            file_path = self._filesystem.join(results_directory_path, name)
            if (artifacts_directory_path in file_path
                    and self._filesystem.isdir(file_path)):
                results_directories.append(file_path)
        results_directories.sort(key=self._filesystem.mtime)
        return results_directories

    def limit_archived_results_count(self):
        _log.info('Clobbering excess archived results in %s' %
                  self._filesystem.dirname(self.artifacts_directory()))
        results_directories = self._get_artifact_directories(
            self.artifacts_directory())
        self._delete_dirs(results_directories[:-ARCHIVED_RESULTS_LIMIT])

    def clobber_old_results(self):
        dir_above_results_path = self._filesystem.dirname(
            self.artifacts_directory())
        _log.info('Clobbering old results in %s.' % dir_above_results_path)
        if not self._filesystem.exists(dir_above_results_path):
            return
        results_directories = self._get_artifact_directories(
            self.artifacts_directory())
        self._delete_dirs(results_directories)

        # Port specific clean-up.
        self.clobber_old_port_specific_results()

    def clobber_old_port_specific_results(self):
        pass

    def use_system_httpd(self):
        # We use system httpd on linux-arm64 and BSD
        return False

    # FIXME: This does not belong on the port object.
    @memoized
    def path_to_apache(self):
        """Returns the full path to the apache binary.

        This is needed only by ports that use the apache_http_server module.
        """
        raise NotImplementedError('Port.path_to_apache')

    def apache_server_root(self):
        """Returns the root that the apache binary is installed to.

        This is used for the ServerRoot directive.
        """
        executable = self.path_to_apache()
        return self._filesystem.dirname(self._filesystem.dirname(executable))

    def path_to_apache_config_file(self):
        """Returns the full path to the apache configuration file.

        If the WEBKIT_HTTP_SERVER_CONF_PATH environment variable is set, its
        contents will be used instead.

        This is needed only by ports that use the apache_http_server module.
        """
        config_file_from_env = self.host.environ.get(
            'WEBKIT_HTTP_SERVER_CONF_PATH')
        if config_file_from_env:
            if not self._filesystem.exists(config_file_from_env):
                raise IOError(
                    '%s was not found on the system' % config_file_from_env)
            return config_file_from_env

        config_file_name = self._apache_config_file_name_for_platform()
        return self._filesystem.join(self.apache_config_directory(),
                                     config_file_name)

    def _apache_version(self):
        env = self.setup_environ_for_server()
        config = self._executive.run_command([self.path_to_apache(), '-v'], env=env)
        # Log version including patch level.
        _log.debug(
            'Found apache version %s',
            re.sub(
                r'(?:.|\n)*Server version: Apache/(\d+\.\d+(?:\.\d+)?)(?:.|\n)*',
                r'\1', config))
        return re.sub(r'(?:.|\n)*Server version: Apache/(\d+\.\d+)(?:.|\n)*',
                      r'\1', config)

    def _apache_config_file_name_for_platform(self):
        # Keep the logic to use apache version even though we only have
        # configuration file for 2.4 now, in case we will have newer version in
        # future.
        return 'apache2-httpd-' + self._apache_version() + '-php7.conf'

    def path_to_driver(self, target=None):
        """Returns the full path to the test driver."""
        return self.build_path(self.driver_name(), target=target)

    def _path_to_image_diff(self):
        """Returns the full path to the image_diff binary, or None if it is not available.

        This is likely used only by diff_image()
        """
        return self.build_path('image_diff')

    def _absolute_baseline_path(self, platform_dir):
        """Return the absolute path to the top of the baseline tree for a
        given platform directory.
        """
        return self._filesystem.join(self.web_tests_dir(), 'platform',
                                     platform_dir)

    def _driver_class(self):
        """Returns the port's driver implementation."""
        return driver.Driver

    def output_contains_sanitizer_messages(self, output):
        if not output:
            return None
        if (b'AddressSanitizer' in output) or (b'MemorySanitizer' in output):
            return True
        return False

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than):
        if self.output_contains_sanitizer_messages(stderr):
            # Running the symbolizer script can take a lot of memory, so we need to
            # serialize access to it across all the concurrently running drivers.

            llvm_symbolizer_path = self._path_from_chromium_base(
                'third_party', 'llvm-build', 'Release+Asserts', 'bin',
                'llvm-symbolizer')
            if self._filesystem.exists(llvm_symbolizer_path):
                env = self.host.environ.copy()
                env['LLVM_SYMBOLIZER_PATH'] = llvm_symbolizer_path
            else:
                env = None
            sanitizer_filter_path = self._path_from_chromium_base(
                'tools', 'valgrind', 'asan', 'asan_symbolize.py')
            sanitizer_strip_path_prefix = 'Release/../../'
            if self._filesystem.exists(sanitizer_filter_path):
                stderr = self._executive.run_command([
                    'flock', sys.executable, sanitizer_filter_path,
                    sanitizer_strip_path_prefix
                ],
                                                     input=stderr,
                                                     decode_output=False,
                                                     env=env)

        name_str = name or '<unknown process name>'
        pid_str = str(pid or '<unknown>')

        # We require stdout and stderr to be bytestrings, not character strings.
        if stdout:
            stdout_lines = stdout.decode('utf8', 'replace').splitlines()
        else:
            stdout_lines = [u'<empty>']

        if stderr:
            stderr_lines = stderr.decode('utf8', 'replace').splitlines()
        else:
            stderr_lines = [u'<empty>']

        return (stderr,
                ('crash log for %s (pid %s):\n%s\n%s\n' %
                 (name_str, pid_str, '\n'.join(
                     ('STDOUT: ' + l) for l in stdout_lines), '\n'.join(
                         ('STDERR: ' + l)
                         for l in stderr_lines))).encode('utf8', 'replace'),
                self._get_crash_site(stderr_lines))

    def _get_crash_site(self, stderr_lines):
        # [blah:blah:blah:FATAL:
        prefix_re = r'\[[\w:/.]*FATAL:'
        # crash_file.ext(line)
        site_re = r'(?P<site>[\w_]*\.[\w_]*\(\d*\))'
        # ] blah failed
        suffix_re = r'\]\s*(Check failed|Security DCHECK failed)'
        pattern = re.compile(prefix_re + site_re + suffix_re)
        for line in stderr_lines:
            match = pattern.search(line)
            if match:
                return match.group('site')
        return None

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        pass

    def look_for_new_samples(self, unresponsive_processes, start_time):
        pass

    def sample_process(self, name, pid):
        pass

    @memoized
    def virtual_test_suites(self):
        path_to_virtual_test_suites = self._filesystem.join(
            self.web_tests_dir(), 'VirtualTestSuites')
        assert self._filesystem.exists(path_to_virtual_test_suites), \
            path_to_virtual_test_suites + ' not found'
        virtual_test_suites = []
        try:
            test_suite_json = json.loads(
                self._filesystem.read_text_file(path_to_virtual_test_suites))
            for json_config in test_suite_json:
                # Strings are treated as comments.
                if isinstance(json_config, str):
                    continue
                vts = VirtualTestSuite(**json_config)
                if any(vts.full_prefix == s.full_prefix
                       for s in virtual_test_suites):
                    raise ValueError(
                        '{} contains entries with the same prefix: {!r}. Please combine them'
                        .format(path_to_virtual_test_suites, json_config))
                virtual_test_suites.append(vts)
                if self.operating_system() in vts.platforms:
                    for entry in vts.skip_base_tests:
                        normalized_base = self.normalize_test_name(entry)
                        # Wpt js file can expand to multiple tests. Remove the "js"
                        # suffix so that the startswith test can pass. This could
                        # be inaccurate but is computationally cheap.
                        if (self.is_wpt_test(normalized_base)
                                and normalized_base.endswith(".js")):
                            normalized_base = normalized_base[:-2]
                        # Update _skip_base_test_map with tests from the current suite's list
                        test_dir, _ = self.split_test(normalized_base)
                        self._skip_base_test_map[test_dir].append(
                            normalized_base)

        except ValueError as error:
            raise ValueError('{} is not a valid JSON file: {}'.format(
                path_to_virtual_test_suites, error))
        return virtual_test_suites

    def _all_virtual_tests(self, tests_by_dir):
        tests = []

        for suite in self.virtual_test_suites():
            if suite.bases:
                tests.extend(map(lambda x: suite.full_prefix + x,
                             self.real_tests_from_dict(suite.bases, tests_by_dir)))
        return tests

    def _get_bases_for_suite_with_paths(self, suite, paths):
        """Returns a set of bases of the virutual suite that are referenced by
        paths. E.g. given a virtual test suite `foo` with the following bases:
          bar/baz
          bar/quu
          qux
        and given paths of [virtual/foo/bar], this method would return
          [bar/baz, bar/quu]

        Given paths of [virtual/foo/bar/baz/test.html], the return would be
        [bar/baz]
        """

        real_paths = [p.replace(suite.full_prefix, '', 1) for p in paths \
            if p.startswith(suite.full_prefix)]
        # Test for paths that are under the suite's bases, so that we don't run
        # a non-existent test.
        bases = set()
        for real_path in real_paths:
            for base in suite.bases:
                if real_path.startswith(base) or base.startswith(real_path):
                    bases.add(base)
                if (self.is_wpt_file(base) and base.endswith('.js')
                        and real_path in self._wpt_test_urls_matching_paths(
                            [base])):
                    bases.add(base)

        return list(bases)

    def _virtual_tests_for_suite_with_paths(self, suite, paths):
        if not suite.bases:
            return []

        bases = self._get_bases_for_suite_with_paths(suite, paths)

        if not bases:
            return []

        tests = []
        tests.extend(
            map(lambda x: suite.full_prefix + x, self.real_tests(bases)))

        wpt_bases = []
        for base in bases:
            if any(base.startswith(wpt_dir) for wpt_dir in self.WPT_DIRS):
                wpt_bases.append(base)

        if wpt_bases:
            tests.extend(
                self._wpt_test_urls_matching_paths(
                    wpt_bases, [suite.full_prefix] * len(wpt_bases)))

        return tests

    def _virtual_tests_matching_paths(self, paths):
        tests = []
        normalized_paths = [self.normalize_test_name(p) for p in paths]
        # Remove the 'js' suffix so that the startswith test can pass
        modified_paths = [
            p[:-2] if self.is_wpt_test(p) and p.endswith('.js') else p
            for p in normalized_paths
        ]
        for suite in self.virtual_test_suites():
            virtual_paths = [
                p for p in normalized_paths if p.startswith(suite.full_prefix)
            ]
            if not virtual_paths:
                continue
            for test in self._virtual_tests_for_suite_with_paths(
                    suite, virtual_paths):
                if any(test.startswith(p) for p in modified_paths):
                    tests.append(test)

        if any(self._path_has_wildcard(path) for path in paths):
            _log.warning(
                'WARNING: Wildcards in paths are not supported for virtual test suites.'
            )

        return tests

    def _path_has_wildcard(self, path):
        return '*' in path

    def _wpt_test_urls_matching_paths(self, filter_paths, virtual_prefixes=[]):
        """Returns a set of paths that are tests to be run from the
        web-platform-test manifest files.

        filter_paths: A list of strings that are prefix matched against the
            list of tests in the WPT manifests. Only tests that match are returned.
        virtual_prefixes: A list of prefixes corresponding to paths in |filter_paths|.
            If present, each test path output should have its virtual prefix
            prepended to the resulting path to the test.
        """
        # Generate the manifest files if needed and then read them. Do this once
        # for this whole method as the file is large and generation/loading is
        # slow.
        wpts = [(wpt_path, self.wpt_manifest(wpt_path))
                for wpt_path in self.WPT_DIRS]

        tests = []
        # This walks through the set of paths where we should look for tests.
        # For each path, a map can be provided that we replace 'path' with in
        # the result.
        for filter_path, virtual_prefix in zip_longest(filter_paths,
                                                       virtual_prefixes):
            # This is to make sure "external[\\/]?" can also match to
            # external/wpt.
            # TODO(robertma): Remove this special case when external/wpt is
            # moved to wpt.
            if filter_path.rstrip('\\/').endswith('external'):
                filter_path = self._filesystem.join(filter_path, 'wpt')
            # '/' is used throughout this function instead of filesystem.sep as
            # the WPT manifest always uses '/' for paths (it is not OS
            # dependent).
            if self._filesystem.sep != '/':
                filter_path = filter_path.replace(self._filesystem.sep, '/')

            # Drop empty path components.
            filter_path = filter_path.replace('//', '/')

            # We now have in |filter_path| a path to an actual test directory or file
            # on disk, in unix format, relative to the root of the web_tests
            # directory.

            for wpt_path, wpt_manifest in wpts:
                # If the |filter_path| is not inside a WPT dir, then we will
                # match no tests in the manifest.
                if not filter_path.startswith(wpt_path):
                    continue
                # Drop the WPT prefix (including the joining '/') from |path|.
                filter_path_from_wpt = filter_path[len(wpt_path) + 1:]

                # An empty filter matches everything.
                if filter_path_from_wpt:
                    # If the filter is to a specific test file that ends with .js,
                    # we match that against tests with any extension by dropping
                    # the extension from the filter.
                    #
                    # Else, when matching a directory, ensure the filter ends in '/'
                    # to only match the exact directory name and not directories
                    # with the filter as a prefix.
                    if wpt_manifest.is_test_file(filter_path_from_wpt):
                        filter_path_from_wpt = re.sub(r'\.js$', '.',
                                                      filter_path_from_wpt)
                    elif not wpt_manifest.is_test_url(filter_path_from_wpt):
                        filter_path_from_wpt = filter_path_from_wpt.rstrip(
                            '/') + '/'

                # We now have a path to an actual test directory or file on
                # disk, in unix format, relative to the WPT directory.
                #
                # Look for all tests in the manifest that are under the relative
                # |filter_path_from_wpt|.
                for test_path_from_wpt in wpt_manifest.all_urls():
                    if test_path_from_wpt.startswith(filter_path_from_wpt):
                        # The result is a test path from the root web test
                        # directory. If a |virtual_prefix| was given, we prepend
                        # that to the result.
                        prefix = virtual_prefix if virtual_prefix else ''
                        tests.append(prefix + wpt_path + '/' +
                                     test_path_from_wpt)
        return tests

    def _lookup_virtual_suite(self, test_name):
        if not test_name.startswith('virtual/'):
            return None
        for suite in self.virtual_test_suites():
            if test_name.startswith(suite.full_prefix):
                return suite
        return None

    def get_suite_name_and_base_test(self, test_name):
        # This assumes test_name is a valid test, and returns suite name
        # and base test. For non virtual tests, returns empty string for
        # suite name
        if test_name.startswith('virtual/'):
            _, name, base_test = test_name.split('/', 2)
            return name, base_test

        return '', test_name

    @memoized
    def lookup_virtual_test_base(self, test_name):
        suite = self._lookup_virtual_suite(test_name)
        if not suite:
            return None
        assert test_name.startswith(suite.full_prefix)
        maybe_base = self.normalize_test_name(
            test_name[len(suite.full_prefix):])
        for base in suite.bases:
            normalized_base = self.normalize_test_name(base)
            # Wpt js file can expand to multiple tests. Remove the "js"
            # suffix so that the startswith test can pass. This could
            # be inaccurate but is computationally cheap.
            if (self.is_wpt_test(normalized_base)
                    and normalized_base.endswith(".js")):
                normalized_base = normalized_base[:-2]
            if normalized_base.startswith(maybe_base) or maybe_base.startswith(
                    normalized_base):
                return maybe_base
        return None

    def _lookup_virtual_test_args(self, test_name):
        normalized_test_name = self.normalize_test_name(test_name)
        for suite in self.virtual_test_suites():
            if normalized_test_name.startswith(suite.full_prefix):
                return suite.args.copy()
        return []

    def build_path(self, *comps: str, target: Optional[str] = None):
        """Returns a path from the build directory."""
        return self._filesystem.join(
            self._path_from_chromium_base(),
            self.get_option('build_directory') or 'out', target
            or self._options.target, *comps)

    def _check_driver_build_up_to_date(self, target):
        # FIXME: We should probably get rid of this check altogether as it has
        # outlived its usefulness in a GN-based world, but for the moment we
        # will just check things if they are using the standard Debug or Release
        # target directories.
        if target not in ('Debug', 'Release'):
            return True

        try:
            debug_path = self.path_to_driver('Debug')
            release_path = self.path_to_driver('Release')

            debug_mtime = self._filesystem.mtime(debug_path)
            release_mtime = self._filesystem.mtime(release_path)

            if (debug_mtime > release_mtime and target == 'Release'
                    or release_mtime > debug_mtime and target == 'Debug'):
                most_recent_binary = 'Release' if target == 'Debug' else 'Debug'
                _log.warning(
                    'You are running the %s binary. However the %s binary appears to be more recent. '
                    'Please pass --%s.', target, most_recent_binary,
                    most_recent_binary.lower())
                _log.warning('')
        # This will fail if we don't have both a debug and release binary.
        # That's fine because, in this case, we must already be running the
        # most up-to-date one.
        except OSError:
            pass
        return True

    def _get_font_files(self):
        """Returns list of font files that should be used by the test."""
        # TODO(sergeyu): Currently FONT_FILES is valid only on Linux. Make it
        # usable on other platforms if necessary.
        result = []
        for (font_dirs, font_file, package) in FONT_FILES:
            exists = False
            for font_dir in font_dirs:
                font_path = self._filesystem.join(font_dir, font_file)
                if not self._filesystem.isabs(font_path):
                    font_path = self.build_path(font_path)
                if self._check_file_exists(font_path, '', more_logging=False):
                    result.append(font_path)
                    exists = True
                    break
            if not exists:
                message = 'You are missing %s under %s.' % (font_file,
                                                            font_dirs)
                if package:
                    message += ' Try installing %s. See build instructions.' % package

                _log.error(message)
                raise TestRunException(exit_codes.SYS_DEPS_EXIT_STATUS,
                                       message)
        return result


class VirtualTestSuite(object):
    def __init__(self,
                 prefix=None,
                 platforms=None,
                 bases=None,
                 exclusive_tests=None,
                 skip_base_tests=None,
                 args=None,
                 owners=None,
                 expires=None,
                 disabled=False):
        assert VALID_FILE_NAME_REGEX.match(prefix), \
            "Virtual test suite prefix '{}' contains invalid characters".format(prefix)
        assert isinstance(platforms, list)
        assert isinstance(bases, list)
        assert args
        assert isinstance(args, list)

        if exclusive_tests == "ALL":
            exclusive_tests = bases
        elif exclusive_tests is None:
            exclusive_tests = []
        assert isinstance(exclusive_tests, list)

        if skip_base_tests == "ALL":
            skip_base_tests = bases
        elif skip_base_tests is None:
            skip_base_tests = []
        assert isinstance(skip_base_tests, list)
        self.prefix = prefix
        self.full_prefix = 'virtual/' + prefix + '/'
        self.platforms = [x.lower() for x in platforms]
        self.bases = bases
        self.exclusive_tests = exclusive_tests
        self.skip_base_tests = skip_base_tests
        self.expires = expires
        self.disabled = disabled
        self.args = sorted(args)
        self.owners = owners
        # always put --enable-threaded-compositing at the end of list, so that after appending
        # this parameter due to crrev.com/c/4599846, we do not need to restart content shell
        # if the parameter set is same.
        if ENABLE_THREADED_COMPOSITING_FLAG in self.args:
            self.args.remove(ENABLE_THREADED_COMPOSITING_FLAG)
            self.args.append(ENABLE_THREADED_COMPOSITING_FLAG)

    def __repr__(self):
        return "VirtualTestSuite('%s', %s, %s, %s)" % (self.full_prefix,
                                                       self.platforms,
                                                       self.bases, self.args)
