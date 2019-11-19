# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def web_socket_do_extra_handshake(_request):
  pass  # Always accept.


def web_socket_transfer_data(_request):
  pass  # Close immediately
