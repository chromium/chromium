# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Add a Strict-Transport-Security header to the response.


import json


def web_socket_do_extra_handshake(request):
  request.extra_headers.append(
      ('Strict-Transport-Security', 'max-age=3600'))
  pass


def web_socket_transfer_data(request):
  # Wait for closing handshake
  request.ws_stream.receive_message()
