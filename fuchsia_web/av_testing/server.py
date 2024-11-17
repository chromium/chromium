# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Starts an http server locally and serves the files in the folder of this
    python file. It needs an integer parameter as the port."""

import http.server
import logging
import os
import socketserver
import sys


class ThreadedHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


Handler = http.server.SimpleHTTPRequestHandler

Handler.extensions_map = {
    '.html': 'text/html',
    '': 'application/octet-stream',
}

DIR = os.path.dirname(__file__)


def start(port: int) -> None:
    with ThreadedHTTPServer(("", port), Handler) as httpd:
        os.chdir(DIR)
        logging.warning('Http server is running on port %s at %s', port, DIR)
        httpd.serve_forever()
