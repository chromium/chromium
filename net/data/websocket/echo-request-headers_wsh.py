# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This handler serializes the received headers into a JSON string and sends it
# back to the client. In |headers_in|, the keys are converted to lower-case,
# while the original case is retained for the values.

import json
from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
  pass


def web_socket_transfer_data(request):
  # Since python 3 does not lowercase the dictionary key, manually lower all
  # keys to maintain python 2/3 compatibility
  lowered_dict = {
      header.lower(): value for header, value in request.headers_in.items()
  }
  msgutil.send_message(request, json.dumps(lowered_dict))
  # Wait for closing handshake
  request.ws_stream.receive_message()
