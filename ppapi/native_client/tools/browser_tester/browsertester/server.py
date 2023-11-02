# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import BaseHTTPServer
import cgi
import mimetypes
import os
import os.path
import posixpath
import SimpleHTTPServer
import SocketServer
import threading
import time
import urllib
import urlparse

class RequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

  def NormalizePath(self, path):
    path = path.split('?', 1)[0]
    path = path.split('#', 1)[0]
    path = posixpath.normpath(urllib.unquote(path))
    words = path.split('/')

    bad = set((os.curdir, os.pardir, ''))
    words = [word for word in words if word not in bad]
    # The path of the request should always use POSIX-style path separators, so
    # that the filename input of --map_file can be a POSIX-style path and still
    # match correctly in translate_path().
    return '/'.join(words)

  def translate_path(self, path):
    path = self.NormalizePath(path)
    if path in self.server.file_mapping:
      return self.server.file_mapping[path]
    for extra_dir in self.server.serving_dirs:
      # TODO(halyavin): set allowed paths in another parameter?
      full_path = os.path.join(extra_dir, os.path.basename(path))
      if os.path.isfile(full_path):
        return full_path

      # Try the complete relative path, not just a basename. This allows the
      # user to serve everything recursively under extra_dir, not just one
      # level deep.
      #
      # One use case for this is the Native Client SDK examples. The examples
      # expect to be able to access files as relative paths from the root of
      # the example directory.
      # Sometimes two subdirectories contain files with the same name, so
      # including all subdirectories in self.server.serving_dirs will not do
      # the correct thing; (i.e. the wrong file will be chosen, even though the
      # correct path was given).
      full_path = os.path.join(extra_dir, path)
      if os.path.isfile(full_path):
        return full_path
    if not path.endswith('favicon.ico') and not self.server.allow_404:
      self.server.listener.ServerError('Cannot find file \'%s\'' % path)
    return path

  def guess_type(self, path):
    # We store the extension -> MIME type mapping in the server instead of the
    # request handler so we that can add additional mapping entries via the
    # command line.
    _, ext = posixpath.splitext(path)
    if ext in self.server.extensions_mapping:
      return self.server.extensions_mapping[ext]
    ext = ext.lower()
    if ext in self.server.extensions_mapping:
      return self.server.extensions_mapping[ext]
    else:
      return self.server.extensions_mapping['']

  def SendRPCResponse(self, response):
    self.send_response(200)
    self.send_header("Content-type", "text/plain")
    self.send_header("Content-length", str(len(response)))
    self.end_headers()
    self.wfile.write(response)

    # shut down the connection
    self.wfile.flush()
    self.connection.shutdown(1)

  def HandleRPC(self, name, query):
    kargs = {}
    for k, v in query.items():
      assert len(v) == 1, k
      kargs[k] = v[0]

    l = self.server.listener
    try:
      response = getattr(l, name)(**kargs)
    except Exception as e:
      self.SendRPCResponse('%r' % (e,))
      raise
    else:
      self.SendRPCResponse(response)

  # For Last-Modified-based caching, the timestamp needs to be old enough
  # for the browser cache to be used (at least 60 seconds).
  # http://www.w3.org/Protocols/rfc2616/rfc2616-sec13.html
  # Often we clobber and regenerate files for testing, so this is needed
  # to actually use the browser cache.
  def send_header(self, keyword, value):
    if keyword == 'Last-Modified':
      last_mod_format = '%a, %d %b %Y %H:%M:%S GMT'
      old_value_as_t = time.strptime(value, last_mod_format)
      old_value_in_secs = time.mktime(old_value_as_t)
      new_value_in_secs = old_value_in_secs - 360
      value = time.strftime(last_mod_format,
                            time.localtime(new_value_in_secs))
    SimpleHTTPServer.SimpleHTTPRequestHandler.send_header(self,
                                                          keyword,
                                                          value)

  def do_POST(self):
    # Backwards compatible - treat result as tuple without named fields.
    _, _, path, _, query, _ = urlparse.urlparse(self.path)

    self.server.listener.Log('POST %s (%s)' % (self.path, path))
    if path == '/echo':
      self.send_response(200)
      self.end_headers()
      data = self.rfile.read(int(self.headers.getheader('content-length')))
      self.wfile.write(data)
    elif self.server.output_dir is not None:
      # Try to write the file to disk.
      path = self.NormalizePath(path)
      output_path = os.path.join(self.server.output_dir, path)
      try:
        outfile = open(output_path, 'w')
      except IOError:
        error_message = 'File not found: %r' % output_path
        self.server.listener.ServerError(error_message)
        self.send_error(404, error_message)
        return

      try:
        data = self.rfile.read(int(self.headers.getheader('content-length')))
        outfile.write(data)
      except IOError as e:
        outfile.close()
        try:
          os.remove(output_path)
        except OSError:
          # Oh, well.
          pass
        error_message = 'Can\'t write file: %r\n' % output_path
        error_message += 'Exception:\n%s' % str(e)
        self.server.listener.ServerError(error_message)
        self.send_error(500, error_message)
        return

      outfile.close()

      # Send a success response.
      self.send_response(200)
      self.end_headers()
    else:
      error_message = 'File not found: %r' % path
      self.server.listener.ServerError(error_message)
      self.send_error(404, error_message)

    self.server.ResetTimeout()

  def do_GET(self):
    # Backwards compatible - treat result as tuple without named fields.
    _, _, path, _, query, _ = urlparse.urlparse(self.path)

    tester = '/TESTER/'
    if path.startswith(tester):
      # If the path starts with '/TESTER/', the GET is an RPC call.
      name = path[len(tester):]
      # Supporting Python 2.5 prevents us from using urlparse.parse_qs
      query = cgi.parse_qs(query, True)

      self.server.rpc_lock.acquire()
      try:
        self.HandleRPC(name, query)
      finally:
        self.server.rpc_lock.release()

      # Don't reset the timeout.  This is not "part of the test", rather it's
      # used to tell us if the renderer process is still alive.
      if name == 'JavaScriptIsAlive':
        self.server.JavaScriptIsAlive()
        return

    elif path in self.server.redirect_mapping:
      dest = self.server.redirect_mapping[path]
      self.send_response(301, 'Moved')
      self.send_header('Location', dest)
      self.end_headers()
      self.wfile.write(self.error_message_format %
                       {'code': 301,
                        'message': 'Moved',
                        'explain': 'Object moved permanently'})
      self.server.listener.Log('REDIRECT %s (%s -> %s)' %
                                (self.path, path, dest))
    else:
      self.server.listener.Log('GET %s (%s)' % (self.path, path))
      # A normal GET request for transferring files, etc.
      f = self.send_head()
      if f:
        self.copyfile(f, self.wfile)
        f.close()

    self.server.ResetTimeout()

  def copyfile(self, source, outputfile):
    # Bandwidth values <= 0.0 are considered infinite
    if self.server.bandwidth <= 0.0:
      return SimpleHTTPServer.SimpleHTTPRequestHandler.copyfile(
          self, source, outputfile)

    self.server.listener.Log('Simulating %f mbps server BW' %
                             self.server.bandwidth)
    chunk_size = 1500 # What size to use?
    bits_per_sec = self.server.bandwidth * 1000000
    start_time = time.time()
    data_sent = 0
    while True:
      chunk = source.read(chunk_size)
      if len(chunk) == 0:
        break
      cur_elapsed = time.time() - start_time
      target_elapsed = (data_sent + len(chunk)) * 8 / bits_per_sec
      if (cur_elapsed < target_elapsed):
        time.sleep(target_elapsed - cur_elapsed)
      outputfile.write(chunk)
      data_sent += len(chunk)
    self.server.listener.Log('Streamed %d bytes in %f s' %
                             (data_sent, time.time() - start_time))

  # Disable the built-in logging
  def log_message(self, format, *args):
    pass


# The ThreadingMixIn allows the server to handle multiple requests
# concurently (or at least as concurently as Python allows).  This is desirable
# because server sockets only allow a limited "backlog" of pending connections
# and in the worst case the browser could make multiple connections and exceed
# this backlog - causing the server to drop requests.  Using ThreadingMixIn
# helps reduce the chance this will happen.
# There were apparently some problems using this Mixin with Python 2.5, but we
# are no longer using anything older than 2.6.
class Server(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):

  def Configure(
      self, file_mapping, redirect_mapping, extensions_mapping, allow_404,
      bandwidth, listener, serving_dirs=[], output_dir=None):
    self.file_mapping = file_mapping
    self.redirect_mapping = redirect_mapping
    self.extensions_mapping.update(extensions_mapping)
    self.allow_404 = allow_404
    self.bandwidth = bandwidth
    self.listener = listener
    self.rpc_lock = threading.Lock()
    self.serving_dirs = serving_dirs
    self.output_dir = output_dir

  def TestingBegun(self, timeout):
    self.test_in_progress = True
    # self.timeout does not affect Python 2.5.
    self.timeout = timeout
    self.ResetTimeout()
    self.JavaScriptIsAlive()
    # Have we seen any requests from the browser?
    self.received_request = False

  def ResetTimeout(self):
    self.last_activity = time.time()
    self.received_request = True

  def JavaScriptIsAlive(self):
    self.last_js_activity = time.time()

  def TimeSinceJSHeartbeat(self):
    return time.time() - self.last_js_activity

  def TestingEnded(self):
    self.test_in_progress = False

  def TimedOut(self, total_time):
    return (total_time >= 0.0 and
            (time.time() - self.last_activity) >= total_time)


def Create(host, port):
  server = Server((host, port), RequestHandler)
  server.extensions_mapping = mimetypes.types_map.copy()
  server.extensions_mapping.update({
    '': 'application/octet-stream' # Default
  })
  return server
