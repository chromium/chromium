# Copyright (c) 2010 Google Inc. All rights reserved.
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

from unittest import mock

from blinkpy.common.net.network_transaction import NetworkTransaction, NetworkTimeout
from blinkpy.common.system.log_testing import LoggingTestCase

from requests import Response
from requests.exceptions import HTTPError, Timeout


@mock.patch('time.sleep', new=mock.Mock())
class NetworkTransactionTest(LoggingTestCase):
    exception = Exception('Test exception')

    def setUp(self):
        super(NetworkTransactionTest, self).setUp()
        self._run_count = 0

    def test_success(self):
        transaction = NetworkTransaction()
        self.assertEqual(transaction.run(lambda retry_index: 42), 42)

    def _raise_exception(self, retry_index):
        raise self.exception

    def test_exception(self):
        transaction = NetworkTransaction()
        did_process_exception = False
        did_throw_exception = True
        try:
            transaction.run(self._raise_exception)
            did_throw_exception = False
        except Exception as error:  # pylint: disable=broad-except
            did_process_exception = True
            self.assertEqual(error, self.exception)
        self.assertTrue(did_throw_exception)
        self.assertTrue(did_process_exception)

    def _raise_500_error(self, retry_index):
        self._run_count += 1
        if retry_index < 2:
            response = Response()
            response.status_code = 500
            response.reason = 'internal server error'
            response.url = 'http://example.com/'
            raise HTTPError(response=response)
        return 42

    def _raise_404_error(self, retry_index):
        response = Response()
        response.status_code = 404
        response.reason = 'not found'
        response.url = 'http://foo.com/'
        raise HTTPError(response=response)

    def _raise_timeout(self, retry_index):
        raise Timeout()

    def test_retry(self):
        transaction = NetworkTransaction(initial_backoff_seconds=0)
        self.assertEqual(transaction.run(self._raise_500_error), 42)
        self.assertEqual(self._run_count, 3)
        self.assertLog([
            'WARNING: Received HTTP status 500 loading "http://example.com/": internal server error. \n',
            'WARNING: Retrying in 0.000 seconds...\n',
            'WARNING: Received HTTP status 500 loading "http://example.com/": internal server error. \n',
            'WARNING: Retrying in 0.000 seconds...\n'
        ])

    def test_convert_404_to_none(self):
        transaction = NetworkTransaction(return_none_on_404=True)
        self.assertIsNone(transaction.run(self._raise_404_error))

    def test_timeout(self):
        transaction = NetworkTransaction(
            initial_backoff_seconds=60 * 60, timeout_seconds=60)
        with self.assertRaises(NetworkTimeout):
            transaction.run(self._raise_timeout)
