#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a simple HTTP server for manually testing exponential
back-off functionality in Chrome.
"""


import BaseHTTPServer
import sys
import urlparse


AJAX_TEST_PAGE = '''
<html>
<head>
<script>

function reportResult(txt) {
  var element = document.createElement('p');
  element.innerHTML = txt;
  document.body.appendChild(element);
}

function fetch() {
  var response_code = document.getElementById('response_code');

  xmlhttp = new XMLHttpRequest();
  xmlhttp.open("GET",
               "http://%s:%d/%s?code=" + response_code.value,
               true);
  xmlhttp.onreadystatechange = function() {
    reportResult(
        'readyState=' + xmlhttp.readyState + ', status=' + xmlhttp.status);
  }
  try {
    xmlhttp.send(null);
  } catch (e) {
    reportResult('Exception: ' + e);
  }
}

</script>
</head>
<body>
<form action="javascript:fetch()">
  Response code to get: <input id="response_code" type="text" value="503">
  <input type="submit">
</form>
</body>
</html>'''


class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  keep_running = True
  local_ip = ''
  port = 0

  def do_GET(self):
    if self.path == '/quitquitquit':
      self.send_response(200)
      self.send_header('Content-Type', 'text/plain')
      self.end_headers()
      self.wfile.write('QUITTING')
      RequestHandler.keep_running = False
      return

    if self.path.startswith('/ajax/'):
      self.send_response(200)
      self.send_header('Content-Type', 'text/html')
      self.end_headers()
      self.wfile.write(AJAX_TEST_PAGE % (self.local_ip,
                                         self.port,
                                         self.path[6:]))
      return

    params = urlparse.parse_qs(urlparse.urlparse(self.path).query)

    if not params or not 'code' in params or params['code'][0] == '200':
      self.send_response(200)
      self.send_header('Content-Type', 'text/plain')
      self.end_headers()
      self.wfile.write('OK')
    else:
      status_code = int(params['code'][0])
      self.send_response(status_code)
      self.end_headers()
      self.wfile.write('Error %d' % int(status_code))


def main():
  if len(sys.argv) != 3:
    print "Usage: %s LOCAL_IP PORT" % sys.argv[0]
    sys.exit(1)
  RequestHandler.local_ip = sys.argv[1]
  port = int(sys.argv[2])
  RequestHandler.port = port
  print "To stop the server, go to http://localhost:%d/quitquitquit" % port
  httpd = BaseHTTPServer.HTTPServer(('', port), RequestHandler)
  while RequestHandler.keep_running:
    httpd.handle_request()


if __name__ == '__main__':
  main()
