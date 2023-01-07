# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing
import os
import SimpleHTTPServer


class LocalHTTPServer(object):
  """Class to start a local HTTP server as a child process."""

  def __init__(self, serve_dir):
    parent_conn, child_conn = multiprocessing.Pipe()
    self.process = multiprocessing.Process(target=_HTTPServerProcess,
                                           args=(child_conn, serve_dir))
    self.process.start()
    if parent_conn.poll(10):  # wait 10 seconds
      self.port = parent_conn.recv()
    else:
      raise Exception('Unable to launch HTTP server.')

    self.conn = parent_conn

  def Shutdown(self):
    """Send a message to the child HTTP server process and wait for it to
        finish."""
    self.conn.send(False)
    self.process.join()

  def GetURL(self, rel_url):
    """Get the full url for a file on the local HTTP server.

    Args:
      rel_url: A URL fragment to convert to a full URL. For example,
          GetURL('foobar.baz') -> 'http://localhost:1234/foobar.baz'
    """
    return 'http://localhost:%d/%s' % (self.port, rel_url)


class QuietHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  def log_message(self, msg_format, *args):
    pass


def _HTTPServerProcess(conn, serve_dir):
  """Run a local httpserver with a randomly-chosen port.

  This function assumes it is run as a child process using multiprocessing.

  Args:
    conn: A connection to the parent process. The child process sends
        the local port, and waits for a message from the parent to
        stop serving.
    serve_dir: The directory to serve. All files are accessible through
       http://localhost:<port>/path/to/filename.
  """
  import BaseHTTPServer

  os.chdir(serve_dir)
  httpd = BaseHTTPServer.HTTPServer(('', 0), QuietHTTPRequestHandler)
  conn.send(httpd.server_address[1])  # the chosen port number
  httpd.timeout = 0.5  # seconds
  running = True
  while running:
    httpd.handle_request()
    if conn.poll():
      running = conn.recv()
  conn.close()
