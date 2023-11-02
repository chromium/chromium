# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mod_pywebsocket import msgutil


def web_socket_do_extra_handshake(request):
  pass  # Always accept.


def web_socket_transfer_data(request):
  msgutil.send_message(request, request.ws_origin)
