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
import random
import time
import requests.exceptions

_log = logging.getLogger(__name__)


class NetworkTimeout(Exception):
    def __str__(self):
        return 'NetworkTimeout'


class NetworkTransaction(object):
    def __init__(self,
                 initial_backoff_seconds=10,
                 grown_factor=1.5,
                 timeout_seconds=60,
                 return_none_on_404=False):
        self._initial_backoff_seconds = initial_backoff_seconds
        self._grown_factor = grown_factor
        self._timeout_seconds = timeout_seconds
        self._return_none_on_404 = return_none_on_404
        self._total_sleep = 0
        self._backoff_seconds = 0

    def run(self, request):
        self._total_sleep = 0
        self._backoff_seconds = self._initial_backoff_seconds
        retry_index = 0
        while True:
            try:
                return request(retry_index=retry_index)
            except requests.exceptions.RequestException as error:
                response = getattr(error, 'response', None)
                if response is not None:
                    if self._return_none_on_404 and response.status_code == 404:
                        return None
                    _log.warning('Received HTTP status %s loading "%s": %s. ',
                                 response.status_code, response.url,
                                 response.reason)
                else:
                    _log.warning('Received RequestException: %s ...', error)
                _log.warning('Retrying in %.3f seconds...',
                             self._backoff_seconds)
                self._check_for_timeout()
                self._sleep()
            retry_index += 1

    def _check_for_timeout(self):
        if self._total_sleep + self._backoff_seconds > self._timeout_seconds:
            raise NetworkTimeout()

    def _sleep(self):
        # Add jitter to avoid resource contention:
        #   https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/
        time.sleep(random.uniform(0.5, 1) * self._backoff_seconds)
        self._total_sleep += self._backoff_seconds
        self._backoff_seconds *= self._grown_factor
