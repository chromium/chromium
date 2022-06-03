# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from collections import OrderedDict

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.filesystem_mock import MockFileSystem
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.web_tests.update_expectations import ExpectationsRemover
from blinkpy.web_tests.builder_list import BuilderList
from blinkpy.web_tests.port.factory import PortFactory
from blinkpy.web_tests.port.test import WEB_TEST_DIR
from blinkpy.web_tests.update_expectations import main
from blinkpy.tool.commands.flaky_tests import FlakyTests


class FakeBotTestExpectations(object):
    def __init__(self, results_by_path):
        self._results = {}

        # Make the results distinct like the real BotTestExpectations.
        for path, results in results_by_path.iteritems():
            self._results[path] = list(set(results))

    def all_results_by_path(self):
        return self._results


class FakeBotTestExpectationsFactory(object):
    def __init__(self):
        """The distinct results seen in at least one run of the test.

        For example, if the bot results for mytest.html are:
            PASS PASS FAIL PASS TIMEOUT
        then |all_results_by_builder| would be:
            {
                'WebKit Linux Trusty' : {
                    'mytest.html': ['FAIL', 'PASS', 'TIMEOUT']
                }
            }
        """
        self.all_results_by_builder = {}

    def expectations_for_builder(self, builder):
        if builder not in self.all_results_by_builder:
            return None

        return FakeBotTestExpectations(self.all_results_by_builder[builder])


class FakePortFactory(PortFactory):
    def __init__(self, host, all_build_types=None, all_systems=None):
        super(FakePortFactory, self).__init__(host)
        self._all_build_types = all_build_types or ()
        self._all_systems = all_systems or ()
        self._configuration_specifier_macros = {
            'mac': ['mac10.10'],
            'win': ['win7'],
            'linux': ['trusty']
        }

    def get(self, port_name=None, options=None, **kwargs):
        """Returns an object implementing the Port interface.

        This fake object will always return the 'test' port.
        """
        port = super(FakePortFactory, self).get('test', None)
        port.all_build_types = self._all_build_types
        port.all_systems = self._all_systems
        port.configuration_specifier_macros_dict = self._configuration_specifier_macros
        return port


class MockWebBrowser(object):
    def __init__(self):
        self.opened_url = None

    def open(self, url):
        self.opened_url = url


def _strip_multiline_string_spaces(raw_string):
    return '\n'.join([s.strip() for s in raw_string.splitlines()])


class UpdateTestExpectationsTest(LoggingTestCase):
    FLAKE_TYPE = 'flake'
    FAIL_TYPE = 'fail'

    def setUp(self):
        super(UpdateTestExpectationsTest, self).setUp()
        self._mock_web_browser = MockWebBrowser()
        self._host = MockHost()
        self._port = self._host.port_factory.get('test', None)
        self._expectation_factory = FakeBotTestExpectationsFactory()
        filesystem = self._host.filesystem
        self._write_tests_into_filesystem(filesystem)

    def tearDown(self):
        super(UpdateTestExpectationsTest, self).tearDown()

    def _write_tests_into_filesystem(self, filesystem):
        test_list = [
            'test/a.html', 'test/b.html', 'test/c.html', 'test/d.html',
            'test/e.html', 'test/f.html', 'test/g.html'
        ]
        for test in test_list:
            path = filesystem.join(WEB_TEST_DIR, test)
            filesystem.write_binary_file(path, '')

    def _create_expectations_remover(self,
                                     type_flag='all',
                                     remove_missing=False,
                                     include_cq_results=False):
        return ExpectationsRemover(
            self._host, self._port, self._expectation_factory,
            self._mock_web_browser, type_flag, remove_missing,
            include_cq_results)

    def _parse_expectations(self, expectations):
        path = self._port.path_to_generic_test_expectations_file()
        self._host.filesystem.write_text_file(path, expectations)

    def _define_builders(self, builders_dict):
        """Defines the available builders for the test.

        Args:
            builders_dict: A dictionary containing builder names to their
            attributes, see BuilderList.__init__ for the format.
        """
        self._host.builders = BuilderList(builders_dict)

    def test_flake_mode_doesnt_remove_non_flakes(self):
        """Tests that lines that aren't flaky are not touched.

        Lines are flaky if they contain a PASS as well as at least one other
        failing result.
        """
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Pass Timeout Failure ]
            # Even though the results show all passing, none of the
            # expectations are flaky so we shouldn't remove any.
            test/a.html [ Pass ]
            test/b.html [ Timeout ]
            test/c.html [ Failure Timeout ]""")

        self._expectations_remover = (self._create_expectations_remover(
            self.FLAKE_TYPE))
        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS'],
            }
        }
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, test_expectations_before)

    def test_fail_mode_doesnt_remove_non_fails(self):
        """Tests that lines that aren't failing are not touched.

        Lines are failing if they contain only 'Failure', 'Timeout', or
        'Crash' results.
        """
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Pass Failure Timeout ]
            # Even though the results show all passing, none of the
            # expectations are failing so we shouldn't remove any.
            test/a.html [ Pass ]
            test/b.html [ Failure Pass ]
            test/c.html [ Failure Pass Timeout ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS'],
            }
        }
        self._expectations_remover = (self._create_expectations_remover(
            self.FAIL_TYPE))
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, test_expectations_before)

    def test_dont_remove_directory_flake(self):
        """Tests that flake lines with directories are untouched."""
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Failure Pass ]
            # This expectation is for a whole directory.
            test/* [ Failure Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS'],
            }
        }
        self._expectations_remover = (self._create_expectations_remover(
            self.FLAKE_TYPE))
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, test_expectations_before)

    def test_dont_remove_directory_fail(self):
        """Tests that fail lines with directories are untouched."""
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Failure ]
            # This expectation is for a whole directory.
            test/* [ Failure ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS'],
            }
        }
        self._expectations_remover = (self._create_expectations_remover(
            self.FAIL_TYPE))
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, test_expectations_before)

    def test_dont_remove_skip(self):
        """Tests that lines with Skip are untouched.

        If a line is marked as Skip, it will eventually contain no results,
        which is indistinguishable from "All Passing" so don't remove since we
        don't know what the results actually are.
        """
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Skip ]
            # Skip expectations should never be removed.
            test/a.html [ Skip ]
            test/b.html [ Skip ]
            test/c.html [ Skip ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS'],
                'test/b.html': ['PASS', 'FAIL'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, test_expectations_before)

    def test_all_failure_result_types(self):
        """Tests that all failure types are treated as failure."""
        test_expectations_before = _strip_multiline_string_spaces(
            """# results: [ Failure Pass ]
            test/a.html [ Failure Pass ]
            test/b.html [ Failure Pass ]
            test/c.html [ Failure Pass ]
            test/d.html [ Failure Pass ]
            # Remove these two since CRASH and TIMEOUT aren't considered
            # Failure.
            test/e.html [ Failure Pass ]
            test/f.html [ Failure Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'FAIL'],
                'test/b.html': ['PASS', 'FAIL'],
                'test/c.html': ['PASS', 'FAIL'],
                'test/d.html': ['PASS', 'FAIL'],
                'test/e.html': ['PASS', 'CRASH'],
                'test/f.html': ['PASS', 'TIMEOUT'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces("""# results: [ Failure Pass ]
            test/a.html [ Failure Pass ]
            test/b.html [ Failure Pass ]
            test/c.html [ Failure Pass ]
            test/d.html [ Failure Pass ]"""))

    def test_fail_mode_all_fail_types_removed(self):
        """Tests that all types of fail expectation are removed in fail mode.

        Fail expectation types include Failure, Timeout, and Crash.
        """
        test_expectations_before = _strip_multiline_string_spaces(
            """# results: [ Timeout Crash Failure ]
            test/a.html [ Failure ]
            test/b.html [ Timeout ]
            test/c.html [ Crash ]
            test/d.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS'],
            }
        }
        self._expectations_remover = (self._create_expectations_remover(
            self.FAIL_TYPE))
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        # The line with test/d.html is not removed since
        # --remove-missing is false by default; lines for
        # tests with no actual results are kept.
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces(
                """# results: [ Timeout Crash Failure ]
            test/d.html [ Failure ]"""))

    def test_basic_one_builder(self):
        """Tests basic functionality with a single builder.

        Test that expectations with results from a single bot showing the
        expected failure isn't occurring should be removed. Results with failures
        of the expected type shouldn't be removed but other kinds of failures
        allow removal.
        """
        test_expectations_before = _strip_multiline_string_spaces(
            """# results: [ Failure Pass Crash Timeout ]
            # Remove these two since they're passing all runs.
            test/a.html [ Failure Pass ]
            test/b.html [ Failure ]
            # Remove these two since the failure is not a Timeout
            test/c.html [ Pass Timeout ]
            test/d.html [ Timeout ]
            # Keep since we have both crashes and passes.
            test/e.html [ Crash Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'FAIL', 'PASS'],
                'test/d.html': ['PASS', 'FAIL', 'PASS'],
                'test/e.html': ['PASS', 'CRASH', 'PASS'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces(
                """# results: [ Failure Pass Crash Timeout ]
            # Keep since we have both crashes and passes.
            test/e.html [ Crash Pass ]"""))

    def test_flake_mode_all_failure_case(self):
        """Tests that results with all failures are not treated as non-flaky."""
        test_expectations_before = _strip_multiline_string_spaces(
            """# results: [ Failure Pass ]
            # Keep since it's all failures.
            test/a.html [ Failure Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['FAIL', 'FAIL', 'FAIL'],
            }
        }
        self._expectations_remover = (self._create_expectations_remover(
            self.FLAKE_TYPE))
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces("""# results: [ Failure Pass ]
            # Keep since it's all failures.
            test/a.html [ Failure Pass ]"""))

    def test_remove_none_met(self):
        """Tests that expectations with no matching result are removed.

        Expectations that are failing in a different way than specified should
        be removed, even if there is no passing result.
        """
        test_expectations_before = ("""# results: [ Failure Pass ]
            # Remove all since CRASH and TIMEOUT aren't considered Failure.
            test/a.html [ Failure Pass ]
            test/b.html [ Failure Pass ]
            test/c.html [ Failure ]
            test/d.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['CRASH'],
                'test/b.html': ['TIMEOUT'],
                'test/c.html': ['CRASH'],
                'test/d.html': ['TIMEOUT'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations,
                          ('# results: [ Failure Pass ]'))

    def test_empty_test_expectations(self):
        """Running on an empty TestExpectations file outputs an empty file."""
        test_expectations_before = ''

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, '')

    def test_basic_multiple_builders(self):
        """Tests basic functionality with multiple builders."""
        test_expectations_before = _strip_multiline_string_spaces(
            """# results: [ Failure Pass ]
            # Remove these two since they're passing on both builders.
            test/a.html [ Failure Pass ]
            test/b.html [ Failure ]
            # Keep these two since they're failing on the Mac builder.
            test/c.html [ Failure Pass ]
            test/d.html [ Failure ]
            # Keep these two since they're failing on the Linux builder.
            test/e.html [ Failure Pass ]
            test/f.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'WebKit Mac10.10': {
                'port_name': 'mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release']
            },
        })

        self._port.all_build_types = ('release', )
        self._port.all_systems = (('mac10.10', 'x86'), ('trusty', 'x86_64'))

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
                'test/e.html': ['FAIL', 'FAIL', 'FAIL'],
                'test/f.html': ['FAIL', 'FAIL', 'FAIL'],
            },
            'WebKit Mac10.10': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'FAIL'],
                'test/d.html': ['PASS', 'PASS', 'FAIL'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS', 'PASS'],
            },
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces("""# results: [ Failure Pass ]
            # Keep these two since they're failing on the Mac builder.
            test/c.html [ Failure Pass ]
            test/d.html [ Failure ]
            # Keep these two since they're failing on the Linux builder.
            test/e.html [ Failure Pass ]
            test/f.html [ Failure ]"""))

    def test_multiple_builders_and_platform_specifiers(self):
        """Tests correct operation with platform specifiers."""
        test_expectations_before = _strip_multiline_string_spaces("""
            # tags: [ Linux Mac Win Mac ]
            # results: [ Failure Pass ]
            # Keep these two since they're failing in the Mac10.10 results.
            [ Mac ] test/a.html [ Failure Pass ]
            [ Mac ] test/b.html [ Failure ]
            # Keep these two since they're failing on the Windows builder.
            [ Linux ] test/c.html [ Failure Pass ]
            [ Win ] test/c.html [ Failure Pass ]
            [ Linux ] test/d.html [ Failure ]
            [ Win ] test/d.html [ Failure ]
            # Remove these two since they're passing on both Linux and Windows builders.
            [ Linux ] test/e.html [ Failure Pass ]
            [ Win ] test/e.html [ Failure Pass ]
            [ Linux ] test/f.html [ Failure ]
            [ Win ] test/f.html [ Failure ]
            # Remove these two since they're passing on Mac results
            [ Mac ] test/g.html [ Failure Pass ]
            [ Mac ] test/h.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'WebKit Mac10.10': {
                'port_name': 'mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release']
            },
            'WebKit Mac10.11': {
                'port_name': 'mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release']
            },
            'WebKit Win7': {
                'port_name': 'win-win7',
                'specifiers': ['Win7', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (
            ('mac10.10', 'x86'),
            ('mac10.11', 'x86'),
            ('trusty', 'x86_64'),
            ('win7', 'x86'),
        )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS', 'PASS'],
                'test/g.html': ['FAIL', 'PASS', 'PASS'],
                'test/h.html': ['FAIL', 'PASS', 'PASS'],
            },
            'WebKit Mac10.10': {
                'test/a.html': ['PASS', 'PASS', 'FAIL'],
                'test/b.html': ['PASS', 'PASS', 'FAIL'],
                'test/c.html': ['PASS', 'FAIL', 'PASS'],
                'test/d.html': ['PASS', 'FAIL', 'PASS'],
                'test/e.html': ['PASS', 'FAIL', 'PASS'],
                'test/f.html': ['PASS', 'FAIL', 'PASS'],
                'test/g.html': ['PASS', 'PASS', 'PASS'],
                'test/h.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Mac10.11': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS', 'PASS'],
                'test/g.html': ['PASS', 'PASS', 'PASS'],
                'test/h.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Win7': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['FAIL', 'PASS', 'PASS'],
                'test/d.html': ['FAIL', 'PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS', 'PASS'],
                'test/g.html': ['FAIL', 'PASS', 'PASS'],
                'test/h.html': ['FAIL', 'PASS', 'PASS'],
            },
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces("""
            # tags: [ Linux Mac Win Mac ]
            # results: [ Failure Pass ]
            # Keep these two since they're failing in the Mac10.10 results.
            [ Mac ] test/a.html [ Failure Pass ]
            [ Mac ] test/b.html [ Failure ]
            # Keep these two since they're failing on the Windows builder.
            [ Win ] test/c.html [ Failure Pass ]
            [ Win ] test/d.html [ Failure ]"""))

    def test_debug_release_specifiers(self):
        """Tests correct operation of Debug/Release specifiers."""
        test_expectations_before = _strip_multiline_string_spaces(
            """# Keep these two since they fail in debug.
            # tags: [ Linux ]
            # tags: [ Debug Release ]
            # results: [ Failure Pass ]
            [ Linux ] test/a.html [ Failure Pass ]
            [ Linux ] test/b.html [ Failure ]
            # Remove these two since failure is in Release, Debug is all PASS.
            [ Debug ] test/c.html [ Failure Pass ]
            [ Debug ] test/d.html [ Failure ]
            # Keep these two since they fail in Linux Release.
            [ Release ] test/e.html [ Failure Pass ]
            [ Release ] test/f.html [ Failure ]
            # Remove these two since the Release Linux builder is all passing.
            [ Release Linux ] test/g.html [ Failure Pass ]
            [ Release Linux ] test/h.html [ Failure ]
            # Remove these two since all the Linux builders PASS.
            [ Linux ] test/i.html [ Failure Pass ]
            [ Linux ] test/j.html [ Failure ]""")

        self._define_builders({
            'WebKit Win7': {
                'port_name': 'win-win7',
                'specifiers': ['Win7', 'Release']
            },
            'WebKit Win7 (dbg)': {
                'port_name': 'win-win7',
                'specifiers': ['Win7', 'Debug']
            },
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'WebKit Linux Trusty (dbg)': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Debug']
            },
        })
        self._port.all_build_types = ('release', 'debug')
        self._port.all_systems = (('win7', 'x86'), ('trusty', 'x86_64'))

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'FAIL', 'PASS'],
                'test/d.html': ['PASS', 'FAIL', 'PASS'],
                'test/e.html': ['PASS', 'FAIL', 'PASS'],
                'test/f.html': ['PASS', 'FAIL', 'PASS'],
                'test/g.html': ['PASS', 'PASS', 'PASS'],
                'test/h.html': ['PASS', 'PASS', 'PASS'],
                'test/i.html': ['PASS', 'PASS', 'PASS'],
                'test/j.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Linux Trusty (dbg)': {
                'test/a.html': ['PASS', 'FAIL', 'PASS'],
                'test/b.html': ['PASS', 'FAIL', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS', 'PASS'],
                'test/g.html': ['FAIL', 'PASS', 'PASS'],
                'test/h.html': ['FAIL', 'PASS', 'PASS'],
                'test/i.html': ['PASS', 'PASS', 'PASS'],
                'test/j.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Win7 (dbg)': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS', 'PASS'],
                'test/g.html': ['PASS', 'FAIL', 'PASS'],
                'test/h.html': ['PASS', 'FAIL', 'PASS'],
                'test/i.html': ['PASS', 'PASS', 'PASS'],
                'test/j.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Win7': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'FAIL'],
                'test/d.html': ['PASS', 'PASS', 'FAIL'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
                'test/f.html': ['PASS', 'PASS', 'PASS'],
                'test/g.html': ['PASS', 'FAIL', 'PASS'],
                'test/h.html': ['PASS', 'FAIL', 'PASS'],
                'test/i.html': ['PASS', 'PASS', 'PASS'],
                'test/j.html': ['PASS', 'PASS', 'PASS'],
            },
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces(
                """# Keep these two since they fail in debug.
            # tags: [ Linux ]
            # tags: [ Debug Release ]
            # results: [ Failure Pass ]
            [ Linux ] test/a.html [ Failure Pass ]
            [ Linux ] test/b.html [ Failure ]
            # Keep these two since they fail in Linux Release.
            [ Release ] test/e.html [ Failure Pass ]
            [ Release ] test/f.html [ Failure ]"""))

    def test_preserve_comments_and_whitespace(self):
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Failure Pass ]
            # Comment A - Keep since these aren't part of any test.
            # Comment B - Keep since these aren't part of any test.

            # Comment C - Remove since it's a block belonging to a
            # Comment D - and a is removed.
            test/a.html [ Failure Pass ]
            # Comment E - Keep since it's below a.


            # Comment F - Keep since only b is removed
            test/b.html [ Failure Pass ]
            test/c.html [ Failure Pass ]

            # Comment G - Should be removed since both d and e will be removed.
            test/d.html [ Failure Pass ]
            test/e.html [ Failure Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'FAIL', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
                'test/e.html': ['PASS', 'PASS', 'PASS'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations,
                          (_strip_multiline_string_spaces("""
            # results: [ Failure Pass ]
            # Comment A - Keep since these aren't part of any test.
            # Comment B - Keep since these aren't part of any test.
            # Comment E - Keep since it's below a.


            # Comment F - Keep since only b is removed
            test/c.html [ Failure Pass ]""")))

    def test_lines_with_no_results_on_builders_kept_by_default(self):
        """Tests the case where there are lines with no results on the builders.

        A test that has no results returned from the builders means that all
        runs passed or were skipped. This might be because the test is skipped
        because it's not in the SmokeTests file (e.g. on Android); or it might
        potentially be that the test is legitimately no longer run anywhere.

        In the former case, we may want to keep the line; but it may also be
        useful to be able to remove it.
        """
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Failure Skip Timeout Pass Crash ]
            # A Skip expectation probably won't have any results but we
            # shouldn't consider those passing so this line should remain.
            test/a.html [ Skip ]
            # The lines below should be kept since the flag for removing
            # such results (--remove-missing) is not passed.
            test/b.html [ Failure Timeout ]
            test/c.html [ Failure Pass ]
            test/d.html [ Pass Timeout ]
            test/e.html [ Crash Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {}
        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, test_expectations_before)

    def test_lines_with_no_results_on_builders_can_be_removed(self):
        """Tests that we remove a line that has no results on the builders.

        In this test, we simulate what would happen when --remove-missing
        is passed.
        """
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Failure Timeout Pass Crash Skip ]
            # A Skip expectation probably won't have any results but we
            # shouldn't consider those passing so this line should remain.
            test/a.html [ Skip ]
            # The lines below should be removed since the flag for removing
            # such results (--remove-missing) is passed.
            test/b.html [ Failure Timeout ]
            test/e.html [ Crash Pass ]
            test/c.html [ Failure Pass ]
            test/d.html [ Pass Timeout ]""")
        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {}
        }
        self._expectations_remover = self._create_expectations_remover(
            remove_missing=True)
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces("""
            # results: [ Failure Timeout Pass Crash Skip ]
            # A Skip expectation probably won't have any results but we
            # shouldn't consider those passing so this line should remain.
            test/a.html [ Skip ]"""))

    def test_include_cq_results(self):
        """By default, cq results are ignored."""
        test_expectations_before = _strip_multiline_string_spaces("""
            # results: [ Failure Pass ]
            # Remove this if cq results are ignored.
            crbug.com/1111 test/a.html [ Failure Pass ]""")

        self._define_builders({
            'WebKit Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'WebKit Linux try': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True
            },
        })

        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux': {'test/a.html': ['PASS', 'PASS', 'PASS'],},
            'WebKit Linux try': {'test/a.html': ['PASS', 'FAIL', 'PASS'],}

        }
        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces("""
                # results: [ Failure Pass ]"""))

        self._expectations_remover = (
            self._create_expectations_remover(include_cq_results=True))
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(updated_expectations, test_expectations_before)

    def test_missing_builders_for_some_configurations(self):
        """Tests the behavior when there are no builders for some configurations.

        We don't necessarily expect to have builders for all configurations,
        so as long as a test appears to not match the expectation on all
        matching configurations that have builders, then it can be removed,
        even if there are extra configurations with no existing builders.
        """
        # Set the logging level used for assertLog to allow us to check
        # messages with a "debug" severity level.
        self.set_logging_level(logging.DEBUG)

        test_expectations_before = _strip_multiline_string_spaces("""
            # tags: [ Win Linux ]
            # tags: [ Release ]
            # results: [ Failure Pass ]

            # There are no builders that match this configuration at all.
            [ Win ] test/a.html [ Failure Pass ]

            # This matches the existing linux release builder and
            # also linux debug, which has no builder.
            [ Linux ] test/b.html [ Failure Pass ]

            # This one is marked as Failing and there are some matching
            # configurations with no builders, but for all configurations
            # with existing builders it is passing.
            test/c.html [ Failure ]

            # This one is marked as flaky and there are some matching
            # configurations with no builders, but for all configurations
            # with existing builders, it is non-flaky.
            test/d.html [ Failure Pass ]

            # This one only matches the existing linux release builder,
            # and it's still flaky, so it shouldn't be removed.
            [ Linux Release ] test/e.html [ Failure Pass ]

            # No message should be emitted for this one because it's not
            # marked as flaky or failing, so we don't need to check builder
            # results.
            test/f.html [ Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })

        self._port.all_build_types = ('release', 'debug')
        self._port.all_systems = (
            ('win7', 'x86'),
            ('trusty', 'x86_64'),
        )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
                'test/e.html': ['PASS', 'FAIL', 'PASS'],
                'test/f.html': ['PASS', 'FAIL', 'PASS'],
            }
        }

        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertEquals(
            updated_expectations,
            _strip_multiline_string_spaces("""
            # tags: [ Win Linux ]
            # tags: [ Release ]
            # results: [ Failure Pass ]

            # This one only matches the existing linux release builder,
            # and it's still flaky, so it shouldn't be removed.
            [ Linux Release ] test/e.html [ Failure Pass ]

            # No message should be emitted for this one because it's not
            # marked as flaky or failing, so we don't need to check builder
            # results.
            test/f.html [ Pass ]"""))

    def test_log_missing_results(self):
        """Tests that we emit the appropriate error for missing results.

        If the results dictionary we download from the builders is missing the
        results from one of the builders we matched we should have logged an
        error.
        """
        test_expectations_before = _strip_multiline_string_spaces("""
            # tags: [ Linux ]
            # tags: [ Release ]
            # results: [ Failure Pass ]
            [ Linux ] test/a.html [ Failure Pass ]
            # This line won't emit an error since the Linux Release results
            # exist.
            [ Linux Release ] test/b.html [ Failure Pass ]
            [ Release ] test/c.html [ Failure ]
            # This line is not flaky or failing so we shouldn't even check the
            # results.
            [ Linux ] test/d.html [ Pass ]""")

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'WebKit Linux Trusty (dbg)': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Debug']
            },
            'WebKit Win7': {
                'port_name': 'win-win7',
                'specifiers': ['Win7', 'Release']
            },
            'WebKit Win7 (dbg)': {
                'port_name': 'win-win7',
                'specifiers': ['Win7', 'Debug']
            },
        })

        # Two warnings and two errors should be emitted:
        # (1) A warning since the results don't contain anything for the Linux
        #     (dbg) builder
        # (2) A warning since the results don't contain anything for the Win
        #     release builder
        # (3) The first line needs and is missing results for Linux (dbg).
        # (4) The third line needs and is missing results for Win Release.
        self._port.all_build_types = ('release', 'debug')
        self._port.all_systems = (('win7', 'x86'), ('trusty', 'x86_64'))

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'FAIL', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Win7 (dbg)': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
            },
        }

        self._expectations_remover = self._create_expectations_remover()
        updated_expectations = (
            self._expectations_remover.get_updated_test_expectations())
        self.assertLog([
            'WARNING: Downloaded results are missing results for builder "WebKit Linux Trusty (dbg)"\n',
            'WARNING: Downloaded results are missing results for builder "WebKit Win7"\n',
            'ERROR: Failed to find results for builder "WebKit Linux Trusty (dbg)"\n',
            'ERROR: Failed to find results for builder "WebKit Win7"\n',
        ])

        # Also make sure we didn't remove any lines if some builders were
        # missing.
        self.assertEqual(updated_expectations, test_expectations_before)

    def test_harness_updates_file(self):
        """Tests that the call harness updates the TestExpectations file."""

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'WebKit Linux Trusty (dbg)': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Debug']
            },
        })

        # Setup the mock host and port.
        host = self._host
        host.port_factory = FakePortFactory(
            host,
            all_build_types=('release', 'debug'),
            all_systems=(('trusty', 'x86_64'), ))

        # Write out a fake TestExpectations file.
        test_expectation_path = (
            host.port_factory.get().path_to_generic_test_expectations_file())
        test_expectations = _strip_multiline_string_spaces("""
            # tags: [ Linux ]
            # tags: [ Release ]
            # results: [ Failure Pass ]
            # Remove since passing on both bots.
            [ Linux ] test/a.html [ Failure Pass ]
            # Keep since there's a failure on release bot.
            [ Linux Release ] test/b.html [ Failure Pass ]
            # Remove since it's passing on both builders.
            test/c.html [ Failure ]
            # Keep since there's a failure on debug bot.
            [ Linux ] test/d.html [ Failure ]""")
        files = {test_expectation_path: test_expectations}
        host.filesystem = MockFileSystem(files)
        self._write_tests_into_filesystem(host.filesystem)

        # Write out the fake builder bot results.
        expectation_factory = FakeBotTestExpectationsFactory()
        expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'FAIL', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Linux Trusty (dbg)': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
                'test/d.html': ['FAIL', 'PASS', 'PASS'],
            },
        }

        main(host, expectation_factory, [])
        self.assertEqual(
            host.filesystem.files[test_expectation_path],
            _strip_multiline_string_spaces("""
            # tags: [ Linux ]
            # tags: [ Release ]
            # results: [ Failure Pass ]
            # Keep since there's a failure on release bot.
            [ Linux Release ] test/b.html [ Failure Pass ]
            # Keep since there's a failure on debug bot.
            [ Linux ] test/d.html [ Failure ]"""))

    def test_harness_no_expectations(self):
        """Tests behavior when TestExpectations file doesn't exist.

        Tests that a warning is outputted if the TestExpectations file
        doesn't exist.
        """

        # Set up the mock host and port.
        host = MockHost()
        host.port_factory = FakePortFactory(host)

        # Write the test file but not the TestExpectations file.
        test_expectation_path = (
            host.port_factory.get().path_to_generic_test_expectations_file())
        host.filesystem = MockFileSystem()
        self._write_tests_into_filesystem(host.filesystem)

        # Write out the fake builder bot results.
        expectation_factory = FakeBotTestExpectationsFactory()
        expectation_factory.all_results_by_builder = {}

        self.assertFalse(host.filesystem.isfile(test_expectation_path))

        return_code = main(host, expectation_factory, [])

        self.assertEqual(return_code, 1)

        self.assertLog([
            "WARNING: Didn't find generic expectations file at: %s\n" %
            test_expectation_path
        ])
        self.assertFalse(host.filesystem.isfile(test_expectation_path))

    def test_harness_remove_all(self):
        """Tests that removing all expectations doesn't delete the file.

        Make sure we're prepared for the day when we exterminated flakes.
        """

        self._define_builders({
            'WebKit Linux Trusty': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
            'WebKit Linux Trusty (dbg)': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Debug']
            },
        })

        # Set up the mock host and port.
        host = self._host
        host.port_factory = FakePortFactory(
            host,
            all_build_types=('release', 'debug'),
            all_systems=(('trusty', 'x86_64'), ))

        # Write out a fake TestExpectations file.
        test_expectation_path = (
            host.port_factory.get().path_to_generic_test_expectations_file())
        test_expectations = """
            # Remove since passing on both bots.
            # tags: [ Linux ]
            # results: [ Failure Pass ]
            [ Linux ] test/a.html [ Failure Pass ]"""

        files = {test_expectation_path: test_expectations}
        host.filesystem = MockFileSystem(files)
        self._write_tests_into_filesystem(host.filesystem)

        # Write out the fake builder bot results.
        expectation_factory = FakeBotTestExpectationsFactory()
        expectation_factory.all_results_by_builder = {
            'WebKit Linux Trusty': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
            },
            'WebKit Linux Trusty (dbg)': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
            },
        }

        main(host, expectation_factory, [])

        self.assertTrue(host.filesystem.isfile(test_expectation_path))
        self.assertEqual(
            host.filesystem.files[test_expectation_path], """
            # Remove since passing on both bots.
            # tags: [ Linux ]
            # results: [ Failure Pass ]""")

    def test_show_results(self):
        """Tests that passing --show-results shows the removed results.

        --show-results opens the removed tests in the layout dashboard using
        the default browser. This tests mocks the webbrowser.open function and
        checks that it was called with the correct URL.
        """
        test_expectations_before = (
            """# Remove this since it's passing all runs.
            # results: [ Failure Pass Crash Timeout ]
            test/a.html [ Failure Pass ]
            # Remove this since, although there's a failure, it's not a timeout.
            test/b.html [ Pass Timeout ]
            # Keep since we have both crashes and passes.
            test/c.html [ Crash Pass ]
            # Remove since it's passing all runs.
            test/d.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'FAIL', 'PASS'],
                'test/c.html': ['PASS', 'CRASH', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        self._expectations_remover.get_updated_test_expectations()
        self._expectations_remover.show_removed_results()
        self.assertEqual(
            FlakyTests.FLAKINESS_DASHBOARD_URL %
            'test/a.html,test/b.html,test/d.html',
            self._mock_web_browser.opened_url)

    def test_flake_mode_suggested_commit_description(self):
        """Tests display of the suggested commit message.
        """
        test_expectations_before = (
            """# Remove this since it's passing all runs.
            # results: [ Failure Pass Timeout ]
            crbug.com/1111 test/a.html [ Failure Pass ]
            # Remove this since, although there's a failure, it's not a timeout.
            crbug.com/2222 test/b.html [ Pass Timeout ]
            # Keep since it's not a flake
            crbug.com/3333 test/c.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'FAIL', 'PASS'],
                'test/c.html': ['PASS', 'PASS', 'PASS'],
            }
        }
        self._expectations_remover = (self._create_expectations_remover(
            self.FLAKE_TYPE))
        self._expectations_remover.get_updated_test_expectations()
        self._expectations_remover.print_suggested_commit_description()
        self.assertLog([
            'INFO: Deleting line "crbug.com/1111 test/a.html [ Failure Pass ]"\n',
            'INFO: Deleting line "crbug.com/2222 test/b.html [ Pass Timeout ]"\n',
            'INFO: Suggested commit description:\n'
            'Remove flake TestExpectations which are not failing in the specified way.\n\n'
            'This change was made by the update_expectations.py script.\n\n'
            'Recent test results history:\n'
            'https://test-results.appspot.com/dashboards/flakiness_dashboard.html'
            '#testType=blink_web_tests&tests=test/a.html,test/b.html\n\n'
            'Bug: 1111, 2222\n'
        ])

    def test_fail_mode_suggested_commit_description(self):
        """Tests display of the suggested commit message.
        """
        test_expectations_before = ("""# Keep since it's not a fail.
            # results: [ Failure Pass ]
            crbug.com/1111 test/a.html [ Failure Pass ]
            # Remove since it's passing all runs.
            crbug.com/2222 test/b.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })
        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'PASS', 'PASS'],
            }
        }
        self._expectations_remover = (self._create_expectations_remover(
            self.FAIL_TYPE))
        self._expectations_remover.get_updated_test_expectations()
        self._expectations_remover.print_suggested_commit_description()
        self.assertLog([
            'INFO: Deleting line "crbug.com/2222 test/b.html [ Failure ]"\n',
            'INFO: Suggested commit description:\n'
            'Remove fail TestExpectations which are not failing in the specified way.\n\n'
            'This change was made by the update_expectations.py script.\n\n'
            'Recent test results history:\n'
            'https://test-results.appspot.com/dashboards/flakiness_dashboard.html'
            '#testType=blink_web_tests&tests=test/b.html\n\n'
            'Bug: 2222\n'
        ])

    def test_suggested_commit_description(self):
        """Tests display of the suggested commit message.
        """
        test_expectations_before = ("""
            # results: [ Failure Pass Crash Timeout ]
            # Remove this since it's passing all runs.
            crbug.com/1111 test/a.html [ Failure Pass ]
            # Remove this since, although there's a failure, it's not a timeout.
            crbug.com/1111 test/b.html [ Pass Timeout ]
            # Keep since we have both crashes and passes.
            crbug.com/2222 test/c.html [ Crash Pass ]
            # Remove since it's passing all runs.
            crbug.com/3333 test/d.html [ Failure ]""")

        self._define_builders({
            'WebKit Linux': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release']
            },
        })

        self._port.all_build_types = ('release', )
        self._port.all_systems = (('trusty', 'x86_64'), )

        self._parse_expectations(test_expectations_before)
        self._expectation_factory.all_results_by_builder = {
            'WebKit Linux': {
                'test/a.html': ['PASS', 'PASS', 'PASS'],
                'test/b.html': ['PASS', 'FAIL', 'PASS'],
                'test/c.html': ['PASS', 'CRASH', 'PASS'],
                'test/d.html': ['PASS', 'PASS', 'PASS'],
            }
        }
        self._expectations_remover = self._create_expectations_remover()
        self._expectations_remover.get_updated_test_expectations()
        self._expectations_remover.print_suggested_commit_description()
        self.assertLog([
            'INFO: Deleting line "crbug.com/1111 test/a.html [ Failure Pass ]"\n',
            'INFO: Deleting line "crbug.com/1111 test/b.html [ Pass Timeout ]"\n',
            'INFO: Deleting line "crbug.com/3333 test/d.html [ Failure ]"\n',
            'INFO: Suggested commit description:\n'
            'Remove TestExpectations which are not failing in the specified way.\n\n'
            'This change was made by the update_expectations.py script.\n\n'
            'Recent test results history:\n'
            'https://test-results.appspot.com/dashboards/flakiness_dashboard.html'
            '#testType=blink_web_tests&tests=test/a.html,test/b.html,test/d.html\n\n'
            'Bug: 1111, 3333\n'
        ])
