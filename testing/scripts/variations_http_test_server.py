# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A http server implementation based on SimpleHTTPServer or http.server and
serves content from a base_path.
"""

import os
try:
  # Python 2
  from SimpleHTTPServer import SimpleHTTPRequestHandler
  from BaseHTTPServer import HTTPServer as BaseHTTPServer
except ImportError:
  # Python 3
  from http.server import SimpleHTTPRequestHandler
  from http.server import HTTPServer as BaseHTTPServer

class HTTPHandler(SimpleHTTPRequestHandler):
  """This handler allows to specify a bath_path. """
  def translate_path(self, path):
    """Uses server.base_path to combine full path."""
    path = SimpleHTTPRequestHandler.translate_path(self, path)
    real_path = os.path.relpath(path, os.getcwd())
    return os.path.join(self.server.base_path, real_path)

class HTTPServer(BaseHTTPServer):
  """The main server, which you couild override base_path."""
  def __init__(self, base_path, server_address,
               RequestHandlerClass=HTTPHandler):
    self.base_path = base_path
    self.stop = False
    BaseHTTPServer.__init__(self, server_address, RequestHandlerClass)

  #pylint: disable=unused-argument
  def serve_forever(self, poll_interval=0.1):
    self.stop = False
    while not self.stop:
      self.handle_request()

  def shutdown(self):
    self.stop = True
