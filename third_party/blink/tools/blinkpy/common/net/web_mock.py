# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import json
from requests.exceptions import HTTPError
from requests import Response
from requests.structures import CaseInsensitiveDict

from blinkpy.common.net.rpc import RESPONSE_PREFIX


class MockWeb(object):
    def __init__(self, urls=None, responses=None):
        self.urls = urls or {}
        self.urls_fetched = []
        self.requests = []
        self.responses = responses or []

    def get_binary(self, url, return_none_on_404=False, trace=None):  # pylint: disable=unused-argument
        self.urls_fetched.append(url)
        if url in self.urls:
            return self.urls[url]
        if return_none_on_404:
            return None
        return b'MOCK Web result, 404 Not found'

    def request(self, method, url, data=None, headers=None, retry_index=None):  # pylint: disable=unused-argument
        self.requests.append((url, data))
        return MockResponse(self.responses.pop(0))

    def request_and_read(self, *args, **kwargs):
        return self.request(*args, **kwargs).body

    def append_prpc_response(self, payload, status_code=200, headers=None):
        headers = headers or {}
        headers.setdefault('Content-Type', 'application/json')
        self.responses.append({
            'status_code':
            200,
            'body':
            RESPONSE_PREFIX + json.dumps(payload).encode(),
            'headers':
            headers,
        })


class MockResponse(object):
    def __init__(self, values):
        self.status_code = values['status_code']
        self.url = ''
        self.body = values.get('body', '')
        # The name of the headers (keys) are case-insensitive, and values are stripped.
        headers_raw = values.get('headers', {})
        self.headers = CaseInsensitiveDict(headers_raw)

        if int(self.status_code) >= 400:
            response = Response()
            response.status_code = self.status_code
            response.reason = 'Received error status code: {}'.format(
                self.status_code)
            response.url = self.url
            raise HTTPError(response=response)

    def getcode(self):
        return self.status_code

    def json(self):
        return json.loads(self.body)
