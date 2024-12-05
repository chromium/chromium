# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Starts an http server locally and serves the files in the folder of this
    python file. It needs an integer parameter as the port."""

import http.server
import logging
import os
import socketserver


VIDEO_DIR = '/usr/local/cipd/videostack_videos_30s'


class ThreadedHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        '.html': 'text/html',
        '': 'application/octet-stream',
    }

    def translate_path(self, path):
        prefix = '/videos/'
        if path.startswith(prefix):
            return os.path.join(VIDEO_DIR, path[len(prefix):])
        return http.server.SimpleHTTPRequestHandler.translate_path(self, path)


def start(port: int) -> None:
    with ThreadedHTTPServer(("", port), Handler) as httpd:
        root_dir = os.path.dirname(__file__)
        os.chdir(root_dir)
        logging.warning('Http server is running on port %s at %s', port,
                        root_dir)
        httpd.serve_forever()
