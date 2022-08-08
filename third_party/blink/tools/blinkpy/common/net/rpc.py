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

import logging
import json
from urllib.parse import urlunsplit

import six

from blinkpy.common.memoized import memoized

_log = logging.getLogger(__name__)

# These characters always appear at the beginning of the RPC response.
RESPONSE_PREFIX = b")]}'"


class BaseRPC:
    """pRPC client.

    A pRPC server handles HTTP POST requests at:
        /prpc/<service>/<method>

    See Also:
        go/prpc: Describes the provisional RPC protocol.
    """

    def __init__(self, web, luci_auth, hostname, service):
        self._web = web
        self._luci_auth = luci_auth
        self._hostname = hostname
        self._service = service

    @memoized
    def _make_url(self, method):
        return urlunsplit((
            'https',
            self._hostname,
            '/prpc/%s/%s' % (self._service, method),
            '',  # No query params
            '',  # No fragment
        ))

    def luci_rpc(self, method, data):
        """Fetches json data through Luci RPCs

        Args:
            method: Method for the RPC call.
            data: the request body in json format

        Returns:
            On success: Returns the json representation of the response.
            Otherwise: None
        """
        luci_token = self._luci_auth.get_access_token()
        headers = {
            'Authorization': 'Bearer ' + luci_token,
            'Accept': 'application/json',
            'Content-Type': 'application/json',
        }
        url = self._make_url(method)
        body = six.ensure_binary(json.dumps(data, separators=(',', ':')))
        response = self._web.request('POST', url, data=body, headers=headers)
        if response.getcode() == 200:
            response_body = response.read()
            if response_body.startswith(RESPONSE_PREFIX):
                response_body = response_body[len(RESPONSE_PREFIX):]
            return json.loads(response_body)

        _log.error("RPC request failed. Status=%s, url=%s", response.status,
                   url)
        _log.debug("Full RPC response: %s" % str(response))
        return None

    def luci_rpc_paginated(self, method, data, field, count=1000):
        """Retrieve entities from a pRPC endpoint with paginated results.

        Some methods receive a response token like:
            {..., "pageSize": ..., "pageToken": ...}

        and reply with a payload like:
            {<repeated field>: [<entity1>, ...], "nextPageToken": ...}

        This method automatically makes a sequence of requests needed to gather
        the requested number of entities. Generally, the payload should not
        change between requests.

        See Also:
            https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/buildbucket/proto/builds_service.proto
            https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/proto/v1/resultdb.proto
        """
        entities = []
        while data.get('pageToken', True) and count > 0:
            response = self.luci_rpc(method, data)
            if not isinstance(response, dict):
                break
            new_entities = response.get(field) or []
            entities.extend(new_entities)
            count -= len(new_entities)
            data['pageToken'] = response.get('nextPageToken')
        return entities[:count]


class ResultDBClient(BaseRPC):
    def __init__(self,
                 web,
                 luci_auth,
                 hostname='results.api.cr.dev',
                 service='luci.resultdb.v1.ResultDB'):
        super().__init__(web, luci_auth, hostname, service)

    def _get_invocations(self, build_ids):
        return ['invocations/build-%s' % build_id for build_id in build_ids]

    def query_artifacts(self, build_ids, predicate, count=1000):
        request = {
            'invocations': self._get_invocations(build_ids),
            'predicate': predicate,
        }
        return self.luci_rpc_paginated('QueryArtifacts',
                                       request,
                                       'artifacts',
                                       count=count)
