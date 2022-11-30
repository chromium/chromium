# Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
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

import logging
import re
import six
import unittest

from blinkpy.web_tests.views.metered_stream import MeteredStream

from six import StringIO


class RegularTest(unittest.TestCase):
    verbose = False
    isatty = False

    def setUp(self):
        self.stream = StringIO()
        self.stream.isatty = lambda: self.isatty

        # configure a logger to test that log calls do normally get included.
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        self.logger.propagate = False

        # add a dummy time counter for a default behavior.
        self.times = list(range(10))

        self.meter = MeteredStream(self.stream, self.verbose, self.logger,
                                   self.time_fn, 8675)

    def tearDown(self):
        if self.meter:
            self.meter.cleanup()
            self.meter = None

    def time_fn(self):
        return self.times.pop(0)

    def test_logging_not_included(self):
        # This tests that if we don't hand a logger to the MeteredStream,
        # nothing is logged.
        logging_stream = StringIO()
        handler = logging.StreamHandler(logging_stream)
        root_logger = logging.getLogger()
        orig_level = root_logger.level
        root_logger.addHandler(handler)
        root_logger.setLevel(logging.DEBUG)
        try:
            self.meter = MeteredStream(self.stream, self.verbose, None,
                                       self.time_fn, 8675)
            self.meter.write_throttled_update('foo')
            self.meter.write_update('bar')
            self.meter.write('baz')
            self.assertEqual(logging_stream.getvalue().splitlines(), [])
        finally:
            root_logger.removeHandler(handler)
            root_logger.setLevel(orig_level)

    def _basic(self, times):
        self.times = times
        self.meter.write_update('foo')
        self.meter.write_update('bar')
        self.meter.write_throttled_update('baz')
        self.meter.write_throttled_update('baz 2')
        self.meter.writeln('done')
        self.assertEqual(self.times, [])
        return self.stream.getvalue().splitlines()

    def test_basic(self):
        buflist = self._basic([0, 1, 2, 13, 14])
        self.assertEqual(buflist, ['foo', 'bar', 'baz 2', 'done'])

    def _log_after_update(self):
        self.meter.write_update('foo')
        self.logger.info('bar')
        return self.stream.getvalue().splitlines()

    def test_log_after_update(self):
        buflist = self._log_after_update()
        self.assertEqual(self.stream.getvalue().splitlines(), ['foo', 'bar'])

    def test_log_args(self):
        self.logger.info('foo %s %d', 'bar', 2)
        self.assertEqual(self.stream.getvalue().splitlines(), ['foo bar 2'])


class TtyTest(RegularTest):
    verbose = False
    isatty = True

    def test_basic(self):
        buflist = self._basic([0, 1, 1.05, 1.1, 2])
        self.assertEqual(buflist, [
            'foo' + MeteredStream._erasure('foo') + 'bar' +
            MeteredStream._erasure('bar') + 'baz 2' +
            MeteredStream._erasure('baz 2') + 'done'
        ])

    def test_log_after_update(self):
        buflist = self._log_after_update()
        self.assertEqual(buflist,
                         ['foo' + MeteredStream._erasure('foo') + 'bar'])

    def test_bytestream(self):
        self.meter.write('German umlauts: \xe4\xf6\xfc')
        self.meter.write(u'German umlauts: \xe4\xf6\xfc')
        if six.PY2:
            # TODO(preethim) : self.stream.getvalue() was giving unicode error.
            # continued with buflist for now.
            self.assertEqual(self.stream.buflist, [
                'German umlauts: \xe4\xf6\xfc', u'German umlauts: \xe4\xf6\xfc'
            ])

        else:
            self.assertEqual(self.stream.getvalue().splitlines(), [
                'German umlauts: \xe4\xf6\xfc' + 'German umlauts: \xe4\xf6\xfc'
            ])


class VerboseTest(RegularTest):
    isatty = False
    verbose = True

    def test_basic(self):
        buflist = self._basic([0, 1, 2.1, 13, 14.1234])
        # We don't bother to match the hours and minutes of the timestamp since
        # the local timezone can vary and we can't set that portably and easily.
        self.assertTrue(re.match(r'\d\d:\d\d:00.000 8675 foo', buflist[0]))
        self.assertTrue(re.match(r'\d\d:\d\d:01.000 8675 bar', buflist[1]))
        self.assertTrue(re.match(r'\d\d:\d\d:13.000 8675 baz 2', buflist[2]))
        self.assertTrue(re.match(r'\d\d:\d\d:14.123 8675 done', buflist[3]))
        self.assertEqual(len(buflist), 4)

    def test_log_after_update(self):
        buflist = self._log_after_update()
        self.assertTrue(re.match(r'\d\d:\d\d:00.000 8675 foo', buflist[0]))

        # The second argument should have a real timestamp and pid, so we just check the format.
        self.assertTrue(re.match(r'\d\d:\d\d:\d\d.\d\d\d \d+ bar', buflist[1]))

        self.assertEqual(len(buflist), 2)

    def test_log_args(self):
        self.logger.info('foo %s %d', 'bar', 2)
        self.assertEqual(len(self.stream.getvalue().splitlines()), 1)
        self.assertTrue(self.stream.getvalue().endswith('foo bar 2\n'))
