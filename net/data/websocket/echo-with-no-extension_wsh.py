# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import six

_GOODBYE_MESSAGE = u'Goodbye'


def web_socket_do_extra_handshake(request):
  request.ws_extension_processors = []


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
