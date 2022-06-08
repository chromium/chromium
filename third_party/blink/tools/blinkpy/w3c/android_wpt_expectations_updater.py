# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates expectations for Android WPT bots.

Specifically, this class fetches results from try bots for the current CL, then
(1) updates browser specific expectation files for Androids browsers like Weblayer.

We needed an Android flavour of the WPTExpectationsUpdater class because
(1) Android bots don't currently produce output that can be baselined, therefore
    we only update expectations in the class below.
(2) For Android we write test expectations to browser specific expectation files,
    not the regular web test's TestExpectation and NeverFixTests file.
(3) WPTExpectationsUpdater can only update expectations for the blink_web_tests
    and webdriver_test_suite steps. Android bots may run several WPT steps, so
    the script needs to be able to update expectations for multiple steps.
"""

import logging
from collections import defaultdict, namedtuple
import six

if six.PY3:
    from functools import reduce

from blinkpy.common.host import Host
from blinkpy.common.memoized import memoized
from blinkpy.common.system.executive import ScriptError
from blinkpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater
from blinkpy.web_tests.models.test_expectations import TestExpectations
from blinkpy.web_tests.models.typ_types import Expectation, ResultType
from blinkpy.web_tests.port.android import (
    PRODUCTS, PRODUCTS_TO_STEPNAMES, PRODUCTS_TO_BROWSER_TAGS,
    PRODUCTS_TO_EXPECTATION_FILE_PATHS, ANDROID_DISABLED_TESTS,
    ANDROID_WEBLAYER, ANDROID_WEBVIEW, CHROME_ANDROID)

_log = logging.getLogger(__name__)

AndroidConfig = namedtuple('AndroidConfig', ['port_name', 'browser'])


class AndroidWPTExpectationsUpdater(WPTExpectationsUpdater):
    MARKER_COMMENT = '# Add untriaged failures in this block'
    NEVER_FIX_MARKER_COMMENT = '# Add untriaged disabled tests in this block'
    UMBRELLA_BUG = 'crbug.com/1050754'

    def __init__(self, host, args=None, wpt_manifests=None):
        super(AndroidWPTExpectationsUpdater, self).__init__(host, args, wpt_manifests)
        self._never_fix_expectations = TestExpectations(
            self.port, {
                ANDROID_DISABLED_TESTS:
                host.filesystem.read_text_file(ANDROID_DISABLED_TESTS)})
        self._baseline_expectations = TestExpectations(self.port)
        assert(len(self.options.android_product) == 1)
        product = self.options.android_product[0]
        if product == ANDROID_WEBLAYER:
            self.testid_prefix = "ninja://weblayer/shell/android:weblayer_shell_wpt/"
            self.test_suite = "weblayer_shell_wpt"
        elif product == ANDROID_WEBVIEW:
            self.testid_prefix = "ninja://android_webview/test:system_webview_wpt/"
            self.test_suite = "system_webview_wpt"
        elif product == CHROME_ANDROID:
            self.testid_prefix = "ninja://chrome/android:chrome_public_wpt/"
            self.test_suite = "chrome_public_wpt"

    def expectations_files(self):
        # We need to put all the Android expectation files in
        # the _test_expectations member variable so that the
        # files get cleaned in cleanup_test_expectations_files()
        return (list(PRODUCTS_TO_EXPECTATION_FILE_PATHS.values()) +
                [ANDROID_DISABLED_TESTS])

    def _get_web_test_results(self, build):
        """Gets web tests results for Android builders. We need to
        bypass the step which gets the step names for the builder
        since it does not currently work for Android.

        Args:
            build: Named tuple containing builder name and number

        Returns:
            returns: List of web tests results for each web test step
            in build.
        """
        results_sets = []
        build_specifiers = self._get_build_specifiers(build)
        for product in self.options.android_product:
            if product in build_specifiers:
                step_name = PRODUCTS_TO_STEPNAMES[product]
                results_sets.append(self.host.results_fetcher.fetch_results(
                    build, True, '%s (with patch)' % step_name))
        return filter(None, results_sets)

    def get_builder_configs(self, build, results_set=None):
        """Gets step name from WebTestResults instance and uses
        that to create AndroidConfig instances. It also only
        returns valid configs for the android products passed
        through the --android-product command line argument.

        Args:
            build: Build object that contains the builder name and
                build number.
            results_set: WebTestResults instance. If this variable
                then it will return a list of android configs for
                each product passed through the --android-product
                command line argument.

        Returns:
            List of valid android configs for products passed through
            the --android-product command line argument."""
        configs = []
        if not results_set:
            build_specifiers = self._get_build_specifiers(build)
            products = build_specifiers & {
                s.lower() for s in self.options.android_product}
        else:
            step_name = results_set.step_name()
            step_name = step_name[: step_name.index(' (with patch)')]
            product = {s: p for p, s in PRODUCTS_TO_STEPNAMES.items()}[step_name]
            products = {product}

        for product in products:
            browser = PRODUCTS_TO_BROWSER_TAGS[product]
            configs.append(
                AndroidConfig(port_name=self.port_name(build), browser=browser))
        return configs

    @memoized
    def _get_build_specifiers(self, build):
        return {s.lower() for s in
                self.host.builders.specifiers_for_builder(build.builder_name)}

    def can_rebaseline(self, *_):
        """Return False since we cannot rebaseline tests for
        Android at the moment."""
        return False

    @staticmethod
    @memoized
    def _get_marker_line_number(test_expectations, path, marker_comment):
        for line in test_expectations.get_updated_lines(path):
            if line.to_string() == marker_comment:
                return line.lineno
        raise ScriptError('Marker comment does not exist in %s' % path)

    def _get_untriaged_test_expectations(
            self, test_expectations, paths, marker_comment):
        untriaged_exps = defaultdict(dict)
        for path in paths:
            marker_lineno = self._get_marker_line_number(
                test_expectations, path, marker_comment)
            exp_lines = test_expectations.get_updated_lines(path)
            for i in range(marker_lineno, len(exp_lines)):
                if (not exp_lines[i].to_string().strip() or
                        exp_lines[i].to_string().startswith('#')):
                    break
                untriaged_exps[path].setdefault(
                    exp_lines[i].test, []).append(exp_lines[i])
        return untriaged_exps

    def _maybe_create_never_fix_expectation(
            self, path, test, test_skipped, tags):
        if test_skipped:
            exps = self._test_expectations.get_expectations_from_file(
                path, test)
            wontfix = self._never_fix_expectations.matches_an_expected_result(
                test, ResultType.Skip)
            temporary_skip = any(ResultType.Skip in exp.results for exp in exps)
            if not (wontfix or temporary_skip):
                return Expectation(
                    test=test, reason=self.UMBRELLA_BUG,
                    results={ResultType.Skip}, tags=tags, raw_tags=tags)

    def _get_expectations_from_baseline(self, exps_test_name):
        expectation_line = self._baseline_expectations.get_expectations(
            exps_test_name)
        results_from_expectation = expectation_line.results
        if expectation_line.is_default_pass:
            # Expectation is default pass, also check baseline expectation
            if self.port.expected_subtest_failure(exps_test_name):
                results_from_expectation = set([ResultType.Failure])
        return results_from_expectation

    def write_to_test_expectations(self, test_to_results):
        """Each expectations file is browser specific, and currently only
        runs on pie. Therefore we do not need any configuration specifiers
        to anotate expectations for certain builds.

        Args:
            test_to_results: A dictionary that maps test names to another
            dictionary which maps a tuple of build configurations and to
            a test result.
        Returns:
            Dictionary mapping test names to lists of expectation strings.
        """
        browser_to_product = {
            browser: product
            for product, browser in PRODUCTS_TO_BROWSER_TAGS.items()}
        browser_to_exp_path = {
            browser: PRODUCTS_TO_EXPECTATION_FILE_PATHS[product]
            for product, browser in PRODUCTS_TO_BROWSER_TAGS.items()}
        product_exp_paths = {PRODUCTS_TO_EXPECTATION_FILE_PATHS[prod]
                             for prod in self.options.android_product}
        untriaged_exps = self._get_untriaged_test_expectations(
            self._test_expectations, product_exp_paths, self.MARKER_COMMENT)
        neverfix_tests = self._get_untriaged_test_expectations(
            self._never_fix_expectations, [ANDROID_DISABLED_TESTS],
            self.NEVER_FIX_MARKER_COMMENT)[ANDROID_DISABLED_TESTS]

        for path, test_exps in untriaged_exps.items():
            self._test_expectations.remove_expectations(
                path, reduce(lambda x, y: x + y, list(test_exps.values())))

        if neverfix_tests:
            self._never_fix_expectations.remove_expectations(
                ANDROID_DISABLED_TESTS,
                reduce(lambda x, y: x + y, list(neverfix_tests.values())))

        exp_lines_dict_by_product = defaultdict(dict)
        for results_test_name, platform_results in test_to_results.items():
            exps_test_name = results_test_name
            for configs, test_results in platform_results.items():
                for config in configs:
                    path = browser_to_exp_path[config.browser]
                    neverfix_exp = self._maybe_create_never_fix_expectation(
                        path, exps_test_name,
                        ResultType.Skip in test_results.actual,
                        {config.browser.lower()})
                    if neverfix_exp:
                        neverfix_tests.setdefault(exps_test_name, []).append(
                            neverfix_exp)
                    else:
                        # no system specifiers are necessary because we are
                        # writing to browser specific expectations files for
                        # only one Android version.
                        unexpected_results = {
                            r for r in test_results.actual.split()
                            if r not in test_results.expected.split()}

                        # as we are using override expectations for Android
                        # side, do not create override expectations if it is a
                        # subset of default expectations or baseline
                        default_expectation = \
                            self._get_expectations_from_baseline(results_test_name)
                        if unexpected_results.issubset(default_expectation):
                            continue

                        # Test expectations for modified test cases are already
                        # deleted, so all tests should be new test
                        expectation = Expectation(
                            test=exps_test_name, reason=self.UMBRELLA_BUG,
                            results=unexpected_results)
                        product = browser_to_product[config.browser]
                        exp_lines_dict_by_product[product][exps_test_name] = \
                            expectation.to_string()
                        untriaged_exps[path].setdefault(
                            exps_test_name, []).append(expectation)

        for path in untriaged_exps:
            marker_lineno = self._get_marker_line_number(
                self._test_expectations, path, self.MARKER_COMMENT)
            self._test_expectations.add_expectations(
                path,
                sorted([exps[0] for exps in untriaged_exps[path].values()],
                       key=lambda e: e.test),
                marker_lineno)

        disabled_tests_marker_lineno = self._get_marker_line_number(
            self._never_fix_expectations,
            ANDROID_DISABLED_TESTS,
            self.NEVER_FIX_MARKER_COMMENT)

        if neverfix_tests:
            self._never_fix_expectations.add_expectations(
                ANDROID_DISABLED_TESTS,
                sorted(reduce(lambda x, y: x + y, list(neverfix_tests.values())),
                       key=lambda e: e.test),
                disabled_tests_marker_lineno)

        self._test_expectations.commit_changes()
        self._never_fix_expectations.commit_changes()

        # returns dictionary mapping product to dictionary that maps test names
        # to test expectation strings.
        return exp_lines_dict_by_product

    def _is_wpt_test(self, _):
        """On Android we use the wpt executable. The test results do not include
        the external/wpt prefix. Therefore we need to override this function for
        Android and return True for all tests since we only run WPT on Android.
        """
        return True

    @memoized
    def _get_try_bots(self):
        return self.host.builders.filter_builders(
            is_try=True, include_specifiers=self.options.android_product)
