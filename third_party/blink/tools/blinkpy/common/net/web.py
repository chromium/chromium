# Copyright (C) 2011 Google Inc. All rights reserved.
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

import gzip

import six
from six.moves import urllib

from blinkpy.common.net.network_transaction import NetworkTransaction


class Web(object):
    class _HTTPRedirectHandler2(urllib.request.HTTPRedirectHandler):  # pylint:disable=no-init
        """A subclass of HTTPRedirectHandler to support 308 Permanent Redirect."""

        def http_error_308(self, req, fp, code, msg, headers):  # pylint:disable=unused-argument
            # We have to override the code to 301 (Moved Permanently);
            # otherwise, HTTPRedirectHandler will throw a HTTPError.
            return self.http_error_301(req, fp, 301, msg, headers)

    def get_binary(self, url, return_none_on_404=False):
        def make_request():
            response = self.request('GET',
                                    url,
                                    headers={'Accept-Encoding': 'gzip'})
            if response.headers.get('Content-Encoding') == 'gzip':
                # Wrap the HTTP response, which is not fully file-like.
                # Unfortunately, `six` does not provide `io.BufferedRandom`, so
                # we need to read the entire payload up-front (may pose a
                # performance issue).
                buf = six.BytesIO(response.read())
                gzip_decoder = gzip.GzipFile(fileobj=buf)
                return gzip_decoder.read()
            return response.read()

        return NetworkTransaction(
            return_none_on_404=return_none_on_404).run(make_request)

    def request(self, method, url, data=None, headers=None):
        opener = urllib.request.build_opener(Web._HTTPRedirectHandler2)
        request = urllib.request.Request(url=url, data=data)

        request.get_method = lambda: method

        if headers:
            for key, value in headers.items():
                request.add_header(key, value)

        return opener.open(request)
