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

import logging

_log = logging.getLogger(__name__)


class MockUser(object):

    DEFAULT_YES = 'y'
    DEFAULT_NO = 'n'

    @classmethod
    def prompt_with_list(cls,
                         list_title,
                         list_items,
                         can_choose_multiple=False,
                         input_func=input):
        pass

    def __init__(self):
        self.opened_urls = []
        self._canned_responses = ['Mock user response']

    def prompt(self, message, repeat=1, input_func=input):
        return self._canned_responses.pop(0)

    def set_canned_responses(self, responses):
        self._canned_responses = responses

    def confirm(self, message=None, default='y'):
        _log.info(message or 'Continue?')
        response = default
        if self._canned_responses:
            response = self._canned_responses.pop(0)
        return response == 'y'

    def can_open_url(self):
        return True

    def open_url(self, url):
        self.opened_urls.append(url)
        if url.startswith('file://'):
            _log.info('MOCK: user.open_url: file://...')
            return
        _log.info('MOCK: user.open_url: %s', url)
