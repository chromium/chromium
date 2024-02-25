# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import six
import time


def web_socket_do_extra_handshake(request):
  request.ws_extension_processors = []


def web_socket_transfer_data(request):
  while True:
    message = request.ws_stream.receive_message()

    if message is None:
      return

    # Send <message> count messages to the web socket, each a quarter second
    # apart.
    for i in range(int(message)):
      request.ws_stream.send_message('ping', binary=False)
      time.sleep(0.25)
