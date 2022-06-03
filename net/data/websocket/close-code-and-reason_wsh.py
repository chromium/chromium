# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import struct

from mod_pywebsocket import stream


def web_socket_do_extra_handshake(_request):
  pass


def web_socket_transfer_data(request):
  line = request.ws_stream.receive_message()
  if line is None:
    return
  if line == '-':
    data = b''
  elif line == '--':
    data = b'X'
  else:
    code, reason = line.split(' ', 1)
    data = struct.pack('!H', int(code)) + reason.encode('utf-8')
  request.connection.write(stream.create_close_frame(data))
  request.server_terminated = True
  # Wait for Close frame from client.
  request.ws_stream.receive_message()
