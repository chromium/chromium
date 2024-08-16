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

import functools
import logging
import threading

import requests

from blinkpy.common.net.network_transaction import NetworkTransaction

_log = logging.getLogger(__name__)


class Web:
    def __init__(self):
        # Map each thread ID to a per-thread session object so that `Web` is
        # thread-safe. This should automatically make the rest of the RPC stack
        # built on `Web` thread-safe.
        #
        # Persisting the sessions keeps the underlying connections alive, which
        # is critical for performance. Too many connections can cause network
        # throttling. See crbug.com/1394451#c8 for an example.
        self._sessions = {}
        # Guard access to the session map.
        # From https://google.github.io/styleguide/pyguide.html#218-threading:
        #   > Do not rely on the atomicity of built-in types.
        self._session_access = threading.Lock()

    @property
    def session(self) -> requests.Session:
        thread_id = threading.current_thread().ident
        with self._session_access:
            session = self._sessions.get(thread_id)
            if not session:
                session = self._sessions[thread_id] = requests.Session()
            return session

    def get_binary(self, url, return_none_on_404=False, trace=None):
        make_request = functools.partial(self.request_and_read,
                                         'GET',
                                         url,
                                         trace=trace,
                                         headers={'Accept-Encoding': 'gzip'})
        return NetworkTransaction(
            return_none_on_404=return_none_on_404).run(make_request)

    def request(self,
                method,
                url,
                data=None,
                headers=None,
                trace=None,
                retry_index=None):
        if trace is not None and retry_index > 2:
            url += f'&{trace=!s}'
        response = self.session.request(method.lower(),
                                        url,
                                        data=data,
                                        headers=headers,
                                        stream=True)
        response.raise_for_status()
        return response

    def request_and_read(self, *args, **kwargs):
        response = self.request(*args, **kwargs)
        buf = bytearray()
        for section in response.iter_content(chunk_size=None):
            buf.extend(section)
        return buf
