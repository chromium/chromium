#!/usr/bin/env vpython3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a simple HTTP/TCP/PROXY/BASIC_AUTH_PROXY/WEBSOCKET server used for
testing Chrome.

It supports several test URLs, as specified by the handlers in TestPageHandler.
By default, it listens on an ephemeral port and sends the port number back to
the originating process over a pipe. The originating process can specify an
explicit port if necessary.
"""

from __future__ import print_function

import base64
import logging
import os
import select
from six.moves import BaseHTTPServer, socketserver
import six.moves.urllib.parse as urlparse
import socket
import ssl
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(BASE_DIR)))

# Insert at the beginning of the path, we want to use our copies of the library
# unconditionally (since they contain modifications from anything that might be
# obtained from e.g. PyPi).
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party', 'pywebsocket3', 'src'))

import mod_pywebsocket.standalone
from mod_pywebsocket.standalone import WebSocketServer
# import manually
mod_pywebsocket.standalone.ssl = ssl

import testserver_base

SERVER_UNSET = 0
SERVER_BASIC_AUTH_PROXY = 1
SERVER_WEBSOCKET = 2
SERVER_PROXY = 3

# Default request queue size for WebSocketServer.
_DEFAULT_REQUEST_QUEUE_SIZE = 128

class WebSocketOptions:
  """Holds options for WebSocketServer."""

  def __init__(self, host, port, data_dir):
    self.request_queue_size = _DEFAULT_REQUEST_QUEUE_SIZE
    self.server_host = host
    self.port = port
    self.websock_handlers = data_dir
    self.scan_dir = None
    self.allow_handlers_outside_root_dir = False
    self.websock_handlers_map_file = None
    self.cgi_directories = []
    self.is_executable_method = None

    self.use_tls = False
    self.private_key = None
    self.certificate = None
    self.tls_client_auth = False
    self.tls_client_ca = None
    self.use_basic_auth = False
    self.basic_auth_credential = 'Basic ' + base64.b64encode(
        b'test:test').decode()


class ThreadingHTTPServer(socketserver.ThreadingMixIn,
                          testserver_base.ClientRestrictingServerMixIn,
                          testserver_base.BrokenPipeHandlerMixIn,
                          testserver_base.StoppableHTTPServer):
  """This is a specialization of StoppableHTTPServer that adds client
  verification and creates a new thread for every connection. It
  should only be used with handlers that are known to be threadsafe."""

  pass


class TestPageHandler(testserver_base.BasePageHandler):
  def __init__(self, request, client_address, socket_server):
    connect_handlers = [self.DefaultConnectResponseHandler]
    get_handlers = [self.DefaultResponseHandler]
    post_handlers = get_handlers
    put_handlers = get_handlers
    head_handlers = [self.DefaultResponseHandler]
    testserver_base.BasePageHandler.__init__(self, request, client_address,
                                             socket_server, connect_handlers,
                                             get_handlers, head_handlers,
                                             post_handlers, put_handlers)

  def DefaultResponseHandler(self):
    """This is the catch-all response handler for requests that aren't handled
    by one of the special handlers above.
    Note that we specify the content-length as without it the https connection
    is not closed properly (and the browser keeps expecting data)."""

    contents = "Default response given for path: " + self.path
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(contents))
    self.end_headers()
    if (self.command != 'HEAD'):
      self.wfile.write(contents.encode('utf8'))
    return True

  def DefaultConnectResponseHandler(self):
    """This is the catch-all response handler for CONNECT requests that aren't
    handled by one of the special handlers above.  Real Web servers respond
    with 400 to CONNECT requests."""

    contents = "Your client has issued a malformed or illegal request."
    self.send_response(400)  # bad request
    self.send_header('Content-Type', 'text/html')
    self.send_header('Content-Length', len(contents))
    self.end_headers()
    self.wfile.write(contents.encode('utf8'))
    return True


class ProxyRequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """A request handler that behaves as a proxy server. Only CONNECT, GET and
  HEAD methods are supported.
  """

  redirect_connect_to_localhost = False;

  def _start_read_write(self, sock):
    sock.setblocking(0)
    self.request.setblocking(0)
    rlist = [self.request, sock]
    while True:
      ready_sockets, _unused, errors = select.select(rlist, [], [])
      if errors:
        self.send_response(500)
        self.end_headers()
        return
      for s in ready_sockets:
        received = s.recv(1024)
        if len(received) == 0:
          return
        if s == self.request:
          other = sock
        else:
          other = self.request
        # This will lose data if the kernel write buffer fills up.
        # TODO(ricea): Correctly use the return value to track how much was
        # written and buffer the rest. Use select to determine when the socket
        # becomes writable again.
        other.send(received)

  def _do_common_method(self):
    url = urlparse.urlparse(self.path)
    port = url.port
    if not port:
      if url.scheme == 'http':
        port = 80
      elif url.scheme == 'https':
        port = 443
    if not url.hostname or not port:
      self.send_response(400)
      self.end_headers()
      return

    if len(url.path) == 0:
      path = '/'
    else:
      path = url.path
    if len(url.query) > 0:
      path = '%s?%s' % (url.path, url.query)

    sock = None
    try:
      sock = socket.create_connection((url.hostname, port))
      sock.send(('%s %s %s\r\n' %
                 (self.command, path, self.protocol_version)).encode('utf-8'))
      for name, value in self.headers.items():
        if (name.lower().startswith('connection')
            or name.lower().startswith('proxy')):
          continue
        # HTTP headers are encoded in Latin-1.
        sock.send(b'%s: %s\r\n' %
                  (name.encode('latin-1'), value.encode('latin-1')))
      sock.send(b'\r\n')
      # This is wrong: it will pass through connection-level headers and
      # misbehave on connection reuse. The only reason it works at all is that
      # our test servers have never supported connection reuse.
      # TODO(ricea): Use a proper HTTP client library instead.
      self._start_read_write(sock)
    except Exception:
      logging.exception('failure in common method: %s %s', self.command, path)
      self.send_response(500)
      self.end_headers()
    finally:
      if sock is not None:
        sock.close()

  def do_CONNECT(self):
    try:
      pos = self.path.rfind(':')
      host = self.path[:pos]
      port = int(self.path[pos+1:])
    except Exception:
      self.send_response(400)
      self.end_headers()

    if ProxyRequestHandler.redirect_connect_to_localhost:
      host = "127.0.0.1"

    sock = None
    try:
      sock = socket.create_connection((host, port))
      self.send_response(200, 'Connection established')
      self.end_headers()
      self._start_read_write(sock)
    except Exception:
      logging.exception('failure in CONNECT: %s', path)
      self.send_response(500)
      self.end_headers()
    finally:
      if sock is not None:
        sock.close()

  def do_GET(self):
    self._do_common_method()

  def do_HEAD(self):
    self._do_common_method()

class BasicAuthProxyRequestHandler(ProxyRequestHandler):
  """A request handler that behaves as a proxy server which requires
  basic authentication.
  """

  _AUTH_CREDENTIAL = 'Basic Zm9vOmJhcg==' # foo:bar

  def parse_request(self):
    """Overrides parse_request to check credential."""

    if not ProxyRequestHandler.parse_request(self):
      return False

    auth = self.headers.get('Proxy-Authorization', None)
    if auth != self._AUTH_CREDENTIAL:
      self.send_response(407)
      self.send_header('Proxy-Authenticate', 'Basic realm="MyRealm1"')
      self.end_headers()
      return False

    return True


class ServerRunner(testserver_base.TestServerRunner):
  """TestServerRunner for the net test servers."""

  def __init__(self):
    super(ServerRunner, self).__init__()

  def __make_data_dir(self):
    if self.options.data_dir:
      if not os.path.isdir(self.options.data_dir):
        raise testserver_base.OptionError('specified data dir not found: ' +
            self.options.data_dir + ' exiting...')
      my_data_dir = self.options.data_dir
    else:
      # Create the default path to our data dir, relative to the exe dir.
      my_data_dir = os.path.join(BASE_DIR, "..", "..", "data")

    return my_data_dir

  def create_server(self, server_data):
    port = self.options.port
    host = self.options.host

    logging.basicConfig()

    # Work around a bug in Mac OS 10.6. Spawning a WebSockets server
    # will result in a call to |getaddrinfo|, which fails with "nodename
    # nor servname provided" for localhost:0 on 10.6.
    # TODO(ricea): Remove this if no longer needed.
    if self.options.server_type == SERVER_WEBSOCKET and \
       host == "localhost" and \
       port == 0:
      host = "127.0.0.1"

    # Construct the subjectAltNames for any ad-hoc generated certificates.
    # As host can be either a DNS name or IP address, attempt to determine
    # which it is, so it can be placed in the appropriate SAN.
    dns_sans = None
    ip_sans = None
    ip = None
    try:
      ip = socket.inet_aton(host)
      ip_sans = [ip]
    except socket.error:
      pass
    if ip is None:
      dns_sans = [host]

    if self.options.server_type == SERVER_UNSET:
      raise testserver_base.OptionError('no server type specified')
    elif self.options.server_type == SERVER_WEBSOCKET:
      # TODO(toyoshim): Remove following os.chdir. Currently this operation
      # is required to work correctly. It should be fixed from pywebsocket side.
      os.chdir(self.__make_data_dir())
      websocket_options = WebSocketOptions(host, port, '.')
      scheme = "ws"
      if self.options.cert_and_key_file:
        scheme = "wss"
        websocket_options.use_tls = True
        key_path = os.path.join(ROOT_DIR, self.options.cert_and_key_file)
        if not os.path.isfile(key_path):
          raise testserver_base.OptionError(
              'specified server cert file not found: ' +
              self.options.cert_and_key_file + ' exiting...')
        websocket_options.private_key = key_path
        websocket_options.certificate = key_path

      if self.options.ssl_client_auth:
        websocket_options.tls_client_cert_optional = False
        websocket_options.tls_client_auth = True
        if len(self.options.ssl_client_ca) != 1:
          raise testserver_base.OptionError(
              'one trusted client CA file should be specified')
        if not os.path.isfile(self.options.ssl_client_ca[0]):
          raise testserver_base.OptionError(
              'specified trusted client CA file not found: ' +
              self.options.ssl_client_ca[0] + ' exiting...')
        websocket_options.tls_client_ca = self.options.ssl_client_ca[0]
      print('Trying to start websocket server on %s://%s:%d...' %
            (scheme, websocket_options.server_host, websocket_options.port))
      server = WebSocketServer(websocket_options)
      print('WebSocket server started on %s://%s:%d...' %
            (scheme, host, server.server_port))
      server_data['port'] = server.server_port
      websocket_options.use_basic_auth = self.options.ws_basic_auth
    elif self.options.server_type == SERVER_PROXY:
      ProxyRequestHandler.redirect_connect_to_localhost = \
          self.options.redirect_connect_to_localhost
      server = ThreadingHTTPServer((host, port), ProxyRequestHandler)
      print('Proxy server started on port %d...' % server.server_port)
      server_data['port'] = server.server_port
    elif self.options.server_type == SERVER_BASIC_AUTH_PROXY:
      ProxyRequestHandler.redirect_connect_to_localhost = \
          self.options.redirect_connect_to_localhost
      server = ThreadingHTTPServer((host, port), BasicAuthProxyRequestHandler)
      print('BasicAuthProxy server started on port %d...' % server.server_port)
      server_data['port'] = server.server_port
    else:
      raise testserver_base.OptionError('unknown server type' +
          self.options.server_type)

    return server

  def add_options(self):
    testserver_base.TestServerRunner.add_options(self)
    self.option_parser.add_option('--proxy',
                                  action='store_const',
                                  const=SERVER_PROXY,
                                  default=SERVER_UNSET,
                                  dest='server_type',
                                  help='start up a proxy server.')
    self.option_parser.add_option('--basic-auth-proxy',
                                  action='store_const',
                                  const=SERVER_BASIC_AUTH_PROXY,
                                  default=SERVER_UNSET,
                                  dest='server_type',
                                  help='start up a proxy server which requires '
                                  'basic authentication.')
    self.option_parser.add_option('--websocket',
                                  action='store_const',
                                  const=SERVER_WEBSOCKET,
                                  default=SERVER_UNSET,
                                  dest='server_type',
                                  help='start up a WebSocket server.')
    self.option_parser.add_option('--cert-and-key-file',
                                  dest='cert_and_key_file', help='specify the '
                                  'path to the file containing the certificate '
                                  'and private key for the server in PEM '
                                  'format')
    self.option_parser.add_option('--ssl-client-auth', action='store_true',
                                  help='Require SSL client auth on every '
                                  'connection.')
    self.option_parser.add_option('--ssl-client-ca', action='append',
                                  default=[], help='Specify that the client '
                                  'certificate request should include the CA '
                                  'named in the subject of the DER-encoded '
                                  'certificate contained in the specified '
                                  'file. This option may appear multiple '
                                  'times, indicating multiple CA names should '
                                  'be sent in the request.')
    self.option_parser.add_option('--file-root-url', default='/files/',
                                  help='Specify a root URL for files served.')
    # TODO(ricea): Generalize this to support basic auth for HTTP too.
    self.option_parser.add_option('--ws-basic-auth', action='store_true',
                                  dest='ws_basic_auth',
                                  help='Enable basic-auth for WebSocket')
    self.option_parser.add_option('--redirect-connect-to-localhost',
                                  dest='redirect_connect_to_localhost',
                                  default=False, action='store_true',
                                  help='If set, the Proxy server will connect '
                                  'to localhost instead of the requested URL '
                                  'on CONNECT requests')


if __name__ == '__main__':
  sys.exit(ServerRunner().main())
