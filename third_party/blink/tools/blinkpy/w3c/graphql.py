# Copyright (C) 2024 Google Inc. All rights reserved.
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

from typing import Any, List, Optional

import requests


class GraphQL:
    def __init__(self, token):
        super().__init__()
        self.token = token

    def headers(self):
        return {"Authorization": f"token {self.token}"}

    def run_mutation(self, mutation):
        response = requests.post('https://api.github.com/graphql',
                                 json={'query': mutation},
                                 headers=self.headers())
        if response.status_code == 200:
            return response.json()
        else:
            raise GraphQLError(
                "Query failed to run by returning code of {}. {}".format(
                    response.status_code, mutation))

    def mark_ready_for_review(self, pull_request_id):
        """Idempotently mark a PR as "ready for review" (non-draft state)."""
        mutation = """
           mutation {
              markPullRequestReadyForReview(input:{pullRequestId: "%s"}) {
                  pullRequest{id, isDraft}
              }
           }
        """ % pull_request_id
        # See https://spec.graphql.org/June2018/#sec-Response-Format for the
        # response payload format, which is unwrapped here.
        payload = self.run_mutation(mutation)
        errors = payload.get('errors', [])
        if errors:
            raise GraphQLError('failed to mark PR ready for review', errors)
        data = payload['data']
        pull_request = data['markPullRequestReadyForReview']['pullRequest']
        assert not pull_request['isDraft']
        return pull_request


class GraphQLError(Exception):

    def __init__(self, msg: str, errors: Optional[List[Any]] = None):
        super().__init__(msg, errors or [])

    @property
    def errors(self) -> List[Any]:
        return self.args[1]
