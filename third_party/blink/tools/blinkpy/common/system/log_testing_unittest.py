# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest

from blinkpy.common.system.log_testing import LoggingTestCase, LogTesting, TestLogStream


class TestLogStreamTest(unittest.TestCase):
    def test_passed_to_stream_handler(self):
        stream = TestLogStream(self)
        handler = logging.StreamHandler(stream)
        logger = logging.getLogger('test.logger')
        logger.addHandler(handler)
        logger.setLevel(logging.INFO)
        logger.info('bar')
        stream.assertMessages(['bar\n'])

    def test_direct_use(self):
        stream = TestLogStream(self)
        stream.write('foo')
        stream.flush()
        stream.assertMessages(['foo'])


class LogTestingTest(unittest.TestCase):
    def test_basic(self):
        log_testing_instance = LogTesting.setUp(self)
        logger = logging.getLogger('test.logger')
        logger.info('my message')
        log_testing_instance.assertMessages(['INFO: my message\n'])
        # The list of messages is cleared after being checked once.
        log_testing_instance.assertMessages([])

    def test_log_level_warning(self):
        log_testing_instance = LogTesting.setUp(
            self, logging_level=logging.WARNING)
        logger = logging.getLogger('test.logger')
        logger.info('my message')
        log_testing_instance.assertMessages([])


class LoggingTestCaseTest(LoggingTestCase):
    def test_basic(self):
        self.example_logging_code()
        self.assertLog([
            'INFO: Informative message\n',
            'WARNING: Warning message\n',
        ])

    @staticmethod
    def example_logging_code():
        logger = logging.getLogger('test.logger')
        logger.debug('Debugging message')
        logger.info('Informative message')
        logger.warning('Warning message')
