# Copyright (C) 2021 Google Inc. All rights reserved.
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

import json
import six

from blinkpy.common.net.luci_auth import LuciAuth

# These characters always appear at the beginning of the RPC response.
SEARCHBUILDS_RESPONSE_PREFIX = b")]}'"


class Rpc(object):
    def __init__(self, host):
        self._host = host

    def luci_rpc(self, url, data):
        """Fetches json data through Luci RPCs

        Args:
            url: url for the rpc call
            data: the request body in json format

        Returns:
            On success: Returns the json representation of the response.
            Otherwise: None
        """
        luci_token = LuciAuth(self._host).get_access_token()
        headers = {
            'Authorization': 'Bearer ' + luci_token,
            'Accept': 'application/json',
            'Content-Type': 'application/json',
        }
        if six.PY3:
            body = json.dumps(data).encode("utf-8")
        else:
            body = json.dumps(data)
        response = self._host.web.request('POST', url, data=body, headers=headers)
        if response.getcode() == 200:
            response_body = response.read()
            if response_body.startswith(SEARCHBUILDS_RESPONSE_PREFIX):
                response_body = response_body[len(SEARCHBUILDS_RESPONSE_PREFIX
                                                  ):]
            return json.loads(response_body)

        _log.error(
            "RPC request failed. Status=%s, url=%s" %
            (response.status, url))
        _log.debug("Full RPC response: %s" % str(response))
        return None
