# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from blinkpy.tool.commands.rebaseline import AbstractRebaseliningCommand

_log = logging.getLogger(__name__)


class RebaselineTest(AbstractRebaseliningCommand):
    name = 'rebaseline-test-internal'
    help_text = 'Rebaseline a single test from a single builder.'

    def __init__(self):
        super(RebaselineTest, self).__init__(options=[
            self.test_option,
            self.suffixes_option,
            self.port_name_option,
            self.builder_option,
            self.build_number_option,
            self.step_name_option,
            self.results_directory_option,
            self.flag_specific_option,
            self.resultDB_option,
            self.fetch_url_option,
        ])

    def execute(self, options, args, tool):
        self._tool = tool
        self._rebaseline_test_and_update_expectations(options)

    def _rebaseline_test_and_update_expectations(self, options):
        self._baseline_suffix_list = options.suffixes.split(',')
        results_url = ''
        suffix = ''
        if options.results_directory:
            results_url = 'file://' + options.results_directory
        elif not options.resultDB:
            results_url = self._tool.results_fetcher.results_url(
                options.builder,
                build_number=options.build_number,
                step_name=options.step_name)

        port_name = options.port_name or self._tool.builders.port_name_for_builder_name(
            options.builder)
        test_name = options.test
        if not options.resultDB:
            for suffix in self._baseline_suffix_list:
                self._rebaseline_test(port_name,
                                      test_name,
                                      suffix,
                                      results_url,
                                      options=options)
        else:
            self._baseline_fetch_url_list = options.fetch_url.split(',')
            if not self._baseline_fetch_url_list:
                _log.warning('No baseline fetch url found for test %s',
                             test_name)
            for artifact_fetch_url in self._baseline_fetch_url_list:
                if 'actual_image' in artifact_fetch_url:
                    suffix = 'png'
                if 'actual_text' in artifact_fetch_url:
                    suffix = 'txt'
                if 'actual_audio' in artifact_fetch_url:
                    suffix = 'wav'
                self._rebaseline_test(port_name,
                                      test_name,
                                      suffix,
                                      results_url,
                                      artifact_fetch_url,
                                      options=options)

    def _rebaseline_test(self,
                         port_name,
                         test_name,
                         suffix,
                         results_url,
                         fetch_url_resultdb='',
                         options=None):
        """Downloads a baseline file and saves it to the filesystem.

        Args:
            port: The port that the baseline is for. This determines
                the directory that the baseline is saved to.
            test_name: The name of the test being rebaselined.
            suffix: The baseline file extension (e.g. png); together with the
                test name and results_url this determines what file to download.
            results_url: Base URL to download the actual result from.
            options: (Optional, default to None) An object with the command line options.
        """
        port = self._tool.port_factory.get(port_name, options)

        # TODO(crbug.com/1154085): Undo this special case when we have WPT bots
        # on more ports.
        if options and options.flag_specific:
            baseline_directory = port.baseline_flag_specific_dir()
        else:
            baseline_directory = port.baseline_version_dir()

        if options and options.resultDB:
            source_baseline = fetch_url_resultdb
        else:
            source_baseline = '%s/%s' % (results_url,
                                         self._file_name_for_actual_result(
                                             test_name, suffix))
        target_baseline = self._tool.filesystem.join(
            baseline_directory,
            self._file_name_for_expected_result(test_name, suffix))

        if suffix == 'png' and port.reference_files(test_name):
            _log.warning('Cannot rebaseline image result for reftest: %s',
                         test_name)
            data = b''
            # Still continue in case we can remove extra -expected.png.
        else:
            _log.debug('Retrieving source %s for target %s.', source_baseline,
                       target_baseline)
            data = self._tool.web.get_binary(source_baseline,
                                             return_none_on_404=True)

        if not data:
            # We don't just remove the file because the test may create empty
            # result on this platform but non-empty on other platforms.
            # Create an empty file, and let optimization deal with it.
            _log.debug(
                'Writing empty result %s which may be removed during optimization.',
                target_baseline)
            data = b''

        filesystem = self._tool.filesystem
        filesystem.maybe_make_directory(filesystem.dirname(target_baseline))
        filesystem.write_binary_file(target_baseline, data)
