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

import enum
import functools
import logging
import json
from typing import List, Literal, NamedTuple, Optional
from urllib.parse import urlunsplit

import six

from blinkpy.common.memoized import memoized
from blinkpy.common.net.luci_auth import LuciAuth
from blinkpy.common.net.network_transaction import (
    NetworkTransaction,
    NetworkTimeout,
)

_log = logging.getLogger(__name__)

# These characters always appear at the beginning of the RPC response.
RESPONSE_PREFIX = b")]}'"


class Build(NamedTuple):
    """A combination of builder and build number.

    If the build number is absent, this represents the latest build for a given
    builder.
    """
    builder_name: str
    build_number: Optional[int] = None
    build_id: Optional[str] = None
    bucket: Literal['ci', 'try'] = 'try'

    def __str__(self) -> str:
        builder = f'"{self.builder_name}"'
        if self.build_number:
            return f'{builder} build {self.build_number}'
        return builder


class BuildStatus(enum.Flag):
    """Buildbucket statuses [0]. The names should match where applicable.

    [0]: https://chromium.googlesource.com/infra/luci/luci-go/+/main/buildbucket/proto/common.proto
    """
    SCHEDULED = enum.auto()
    STARTED = enum.auto()
    SUCCESS = enum.auto()
    # `*_FAILURE` are Chromium-specific reasons why a build appears red:
    # https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:recipes/recipe_modules/tryserver/api.py;l=295-309;drc=6ba67afd6fb7718743af91b847ddf1907f3ee9a6;bpv=0;bpt=0
    #
    # These reasons are opaque to Buildbucket, which treats all of them as just
    # `FAILURE`.
    TEST_FAILURE = enum.auto()
    COMPILE_FAILURE = enum.auto()
    PATCH_FAILURE = enum.auto()
    OTHER_FAILURE = enum.auto()
    FAILURE = TEST_FAILURE | COMPILE_FAILURE | PATCH_FAILURE | OTHER_FAILURE
    INFRA_FAILURE = enum.auto()
    CANCELED = enum.auto()
    COMPLETED = SUCCESS | FAILURE | INFRA_FAILURE | CANCELED
    # Pseudo-status more specific than `SCHEDULED` to indicate a build that was
    # triggered by this run.
    TRIGGERED = enum.auto()
    # Pseudo-status to indicate a missing try build.
    MISSING = enum.auto()


class RPCError(Exception):
    """Base type for all pRPC errors."""

    def __init__(self, message, method, request_body=None, code=None):
        message = '%s: %s' % (method, message)
        if code:
            message += ' (code: %d)' % code
        super().__init__(message)
        self.method = method
        self.code = code
        self.request_body = request_body


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

    @classmethod
    def from_host(cls, host, *args, **kwargs):
        return cls(host.web, LuciAuth(host), *args, **kwargs)

    @memoized
    def _make_url(self, method):
        return urlunsplit((
            'https',
            self._hostname,
            '/prpc/%s/%s' % (self._service, method),
            '',  # No query params
            '',  # No fragment
        ))

    def _luci_rpc(self, method, data):
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
            'Accept-Encoding': 'gzip',
            'Content-Type': 'application/json',
        }
        url = self._make_url(method)
        body = six.ensure_binary(json.dumps(data, separators=(',', ':')))
        make_request = functools.partial(self._web.request_and_read,
                                         'POST',
                                         url,
                                         data=body,
                                         headers=headers)
        try:
            response_body = NetworkTransaction().run(make_request)
            if response_body.startswith(RESPONSE_PREFIX):
                response_body = response_body[len(RESPONSE_PREFIX):]
            return json.loads(response_body)
        except NetworkTimeout:
            _log.error('RPC request timed out. URL: %s', url)
            _log.debug('Full RPC request: %s', json.dumps(data, indent=2))
            return None

    def _luci_rpc_paginated(self,
                            method,
                            data,
                            field,
                            count=1000):
        """Retrieve entities from a pRPC method with paginated results.

        Some methods receive a request like:
            {..., "pageSize": ..., "pageToken": ...}

        and reply with a payload like:
            {<repeated field>: [<entity1>, ...], "nextPageToken": ...}

        This method automatically makes a sequence of requests to gather the
        requested number of entities. Generally, the method parameters should
        not change between requests except for the "pageToken" field.

        Arguments:
            method: The RPC method name (conventionally Pascal case).
            data: JSON-encodable parameters to send to the RPC endpoint.
            field: Name of the repeated field that should be extracted from each
                response body.
            count: Total number of entities to attempt to retrieve. The actual
                number returned may be fewer, depending on how many entities
                exist, 0 means get all.

        Returns:
            A list of up to `count` entities. The shape of each entry depends
            on the method.

        See Also:
            https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/buildbucket/proto/builds_service.proto
            https://source.chromium.org/chromium/infra/infra/+/master:go/src/go.chromium.org/luci/resultdb/proto/v1/resultdb.proto
        """
        entities = []
        data['pageSize'] = count if count > 0 else 1000
        while data.get('pageToken', True) and (count == 0
                                               or count - len(entities) > 0):
            response = self._luci_rpc(method, data)
            if not isinstance(response, dict):
                break
            entities.extend(response.get(field) or [])
            data['pageToken'] = response.get('nextPageToken')
        return entities[:count] if count > 0 else entities


class BuildbucketClient(BaseRPC):
    def __init__(self,
                 web,
                 luci_auth,
                 hostname='cr-buildbucket.appspot.com',
                 service='buildbucket.v2.Builds'):
        super().__init__(web, luci_auth, hostname, service)
        self._batch_requests = []

    def _make_get_build_body(self, build=None, build_fields=None):
        request = {}
        if build.build_id:
            request['id'] = str(build.build_id)
        elif build.builder_name and build.build_number:
            request['builder'] = {
                'project': 'chromium',
                'bucket': build.bucket,
                'builder': build.builder_name
            }
            request['buildNumber'] = build.build_number
        else:
            raise ValueError('bad GetBuild request: must provide either '
                             'build ID or (builder and build number)')
        if build_fields:
            # The `builds.*` prefix is not needed for retrieving an individual
            # build.
            request['fields'] = ','.join(build_fields)
        return request

    def _make_search_builds_body(self, predicate, build_fields=None):
        request = {'predicate': predicate}
        if build_fields:
            request['fields'] = ','.join('builds.*.%s' % field
                                         for field in build_fields)
        return request

    def get_build(self, build=None, build_fields=None):
        return self._luci_rpc('GetBuild',
                              self._make_get_build_body(build, build_fields))

    def search_builds(self, predicate, build_fields=None, count=0):
        return self._luci_rpc_paginated('SearchBuilds',
                                        self._make_search_builds_body(
                                            predicate, build_fields),
                                        'builds',
                                        count=count)

    def add_get_build_req(self, build=None, build_fields=None):
        self._batch_requests.append(
            ('getBuild', self._make_get_build_body(build,
                                                   build_fields), None, None))

    def add_search_builds_req(self, predicate, build_fields=None, count=1000):
        # Just try to extract the repeated field and truncate it to `count`
        # items, at most.
        self._batch_requests.append(
            ('searchBuilds',
             self._make_search_builds_body(predicate,
                                           build_fields), 'builds', count))

    def execute_batch(self):
        """Execute the current batch request and yield the results.

        Once called, the client will clear its internal request buffer.

        Raises:
            RPCError: If the server returns an error object for any individual
                response.
        """
        if not self._batch_requests:
            return
        batch_requests, self._batch_requests = self._batch_requests, []
        batch_request_body = {
            'requests': [{
                method: body
            } for method, body, _, _ in batch_requests]
        }
        batch_response = self._luci_rpc('Batch', batch_request_body) or {}
        responses = batch_response.get('responses') or []
        for request, response_body in zip(batch_requests, responses):
            method, request_body, field, count = request
            error = response_body.get('error')
            if error:
                message = error.get('message', 'unknown error')
                # Avoid the built-in `str.capitalize`, since it lowercases the
                # remaining letters.
                raise RPCError(message, method[0].upper() + method[1:],
                               request_body, error.get('code'))
            unwrapped_response = response_body[method]
            if field:
                yield from unwrapped_response[
                    field][:count] if count > 0 else unwrapped_response[field]
            else:
                yield unwrapped_response

    def clear_batch(self):
        """Clear the current batch request."""
        self._batch_requests.clear()


class ResultDBClient(BaseRPC):
    def __init__(self,
                 web,
                 luci_auth,
                 hostname='results.api.cr.dev',
                 service='luci.resultdb.v1.ResultDB'):
        super().__init__(web, luci_auth, hostname, service)

    def _get_invocations(self, build_ids):
        return ['invocations/build-%s' % build_id for build_id in build_ids]

    def query_test_results(self,
                           build_ids,
                           predicate,
                           read_mask: Optional[List[str]] = None,
                           count=0):
        request = {
            'invocations': self._get_invocations(build_ids),
            'predicate': predicate,
        }
        if read_mask:
            request['readMask'] = ','.join(read_mask)
        return self._luci_rpc_paginated('QueryTestResults',
                                        request,
                                        'testResults',
                                        count=count)

    def query_artifacts(self, build_ids, predicate, count=0):
        request = {
            'invocations': self._get_invocations(build_ids),
            'predicate': predicate,
        }
        return self._luci_rpc_paginated('QueryArtifacts',
                                        request,
                                        'artifacts',
                                        count=count)
