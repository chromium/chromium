# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import struct

from mod_pywebsocket import handshake
from mod_pywebsocket import stream


def web_socket_do_extra_handshake(_request):
  pass


def web_socket_transfer_data(request):
  # Just waiting...
  request.ws_stream.receive_message()


def web_socket_passive_closing_handshake(request):
  code = struct.pack('!H', 3004)
  packet = stream.create_close_frame(code + 'split test'.encode('utf-8'))
  request.connection.write(packet[:1])
  request.connection.write(packet[1:])
  raise handshake.AbortedByUserException('Abort the connection')
