# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The purpose of this test is to verify that the WebSocket handshake correctly
# ignores trailing whitespace on response headers.
# It is used by test case WebSocketEndToEndTest.TrailingWhitespace.

from mod_pywebsocket import handshake
from mod_pywebsocket.handshake.hybi import compute_accept_from_unicode


def web_socket_do_extra_handshake(request):
  accept = compute_accept_from_unicode(request.headers_in['Sec-WebSocket-Key'])
  message = (b'HTTP/1.1 101 Switching Protocols\r\n'
             b'Upgrade: websocket\r\n'
             b'Connection: Upgrade\r\n'
             b'Sec-WebSocket-Accept: %s\r\n'
             b'Sec-WebSocket-Protocol: sip \r\n'
             b'\r\n' % accept)
  request.connection.write(message)
  # Prevent pywebsocket from sending its own handshake message.
  raise handshake.AbortedByUserException('Close the connection')


def web_socket_transfer_data(request):
  pass
