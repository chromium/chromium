# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import six

_GOODBYE_MESSAGE = u'Goodbye'


def web_socket_do_extra_handshake(_request):
  pass  # Always accept.


def web_socket_transfer_data(request):
  while True:
    line = request.ws_stream.receive_message()
    if line is None:
      return
    if isinstance(line, six.text_type):
      request.ws_stream.send_message(line, binary=False)
      if line == _GOODBYE_MESSAGE:
        return
    else:
      request.ws_stream.send_message(line, binary=True)


def web_socket_passive_closing_handshake(request):
  return request.ws_close_code, request.ws_close_reason
