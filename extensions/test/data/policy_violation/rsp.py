#!/usr/bin/env python3
#
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import http.server

RESPONSE_BODY = b""")]}'
{"response":
  {
    "server":"prod",
    "protocol":"3.1",
    "daystart":{"elapsed_seconds":45861,"elapsed_days":5205},
    "app":[
      {
        "appid":"ebfcakiglfalfoplflllgbnmalfhaeio",
        "cohort":"1::",
        "status":"ok",
        "cohortname":"",
        "updatecheck":{"_esbAllowlist":"false","status":"noupdate"}
      }
    ]
  }
}
"""

class Handler(http.server.BaseHTTPRequestHandler):
  def do_POST(self):
    self.send_response(200)
    self.end_headers()
    self.wfile.write(RESPONSE_BODY)

http.server.HTTPServer(('', 8080), Handler).serve_forever()
