# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mod_pywebsocket import msgutil
from six.moves.urllib import parse


def web_socket_do_extra_handshake(request):
  r = request.ws_resource.split('?', 1)
  if len(r) == 1:
    return
  param = parse.parse_qs(r[1])
  if 'protocol' in param:
    request.ws_protocol = param['protocol'][0]


def web_socket_transfer_data(request):
  msgutil.send_message(request, request.ws_protocol)
  # Wait for a close message.
  unused = request.ws_stream.receive_message()
