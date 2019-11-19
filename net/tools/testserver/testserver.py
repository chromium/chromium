#!/usr/bin/env vpython
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is a simple HTTP/FTP/TCP/UDP/PROXY/BASIC_AUTH_PROXY/WEBSOCKET server
used for testing Chrome.

It supports several test URLs, as specified by the handlers in TestPageHandler.
By default, it listens on an ephemeral port and sends the port number back to
the originating process over a pipe. The originating process can specify an
explicit port if necessary.
It can use https if you specify the flag --https=CERT where CERT is the path
to a pem file containing the certificate and private key that should be used.
"""

import base64
import BaseHTTPServer
import cgi
import hashlib
import logging
import minica
import os
import json
import random
import re
import select
import socket
import SocketServer
import ssl
import struct
import sys
import threading
import time
import urllib
import urlparse
import zlib

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(BASE_DIR)))

# Insert at the beginning of the path, we want to use our copies of the library
# unconditionally (since they contain modifications from anything that might be
# obtained from e.g. PyPi).
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party', 'pywebsocket', 'src'))
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party', 'tlslite'))

import mod_pywebsocket.standalone
from mod_pywebsocket.standalone import WebSocketServer
# import manually
mod_pywebsocket.standalone.ssl = ssl

import pyftpdlib.ftpserver

import tlslite
import tlslite.api

import echo_message
import testserver_base

SERVER_HTTP = 0
SERVER_FTP = 1
SERVER_TCP_ECHO = 2
SERVER_UDP_ECHO = 3
SERVER_BASIC_AUTH_PROXY = 4
SERVER_WEBSOCKET = 5
SERVER_PROXY = 6

# Default request queue size for WebSocketServer.
_DEFAULT_REQUEST_QUEUE_SIZE = 128

OCSP_STATES_NO_SINGLE_RESPONSE = {
  minica.OCSP_STATE_INVALID_RESPONSE,
  minica.OCSP_STATE_UNAUTHORIZED,
  minica.OCSP_STATE_TRY_LATER,
  minica.OCSP_STATE_INVALID_RESPONSE_DATA,
}

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
    self.allow_draft75 = False
    self.strict = True

    self.use_tls = False
    self.private_key = None
    self.certificate = None
    self.tls_client_auth = False
    self.tls_client_ca = None
    self.tls_module = 'ssl'
    self.use_basic_auth = False
    self.basic_auth_credential = 'Basic ' + base64.b64encode('test:test')


class RecordingSSLSessionCache(object):
  """RecordingSSLSessionCache acts as a TLS session cache and maintains a log of
  lookups and inserts in order to test session cache behaviours."""

  def __init__(self):
    self.log = []

  def __getitem__(self, sessionID):
    self.log.append(('lookup', sessionID))
    raise KeyError()

  def __setitem__(self, sessionID, session):
    self.log.append(('insert', sessionID))


class HTTPServer(testserver_base.ClientRestrictingServerMixIn,
                 testserver_base.BrokenPipeHandlerMixIn,
                 testserver_base.StoppableHTTPServer):
  """This is a specialization of StoppableHTTPServer that adds client
  verification."""

  pass

class ThreadingHTTPServer(SocketServer.ThreadingMixIn,
                          HTTPServer):
  """This variant of HTTPServer creates a new thread for every connection. It
  should only be used with handlers that are known to be threadsafe."""

  pass

class OCSPServer(testserver_base.ClientRestrictingServerMixIn,
                 testserver_base.BrokenPipeHandlerMixIn,
                 BaseHTTPServer.HTTPServer):
  """This is a specialization of HTTPServer that serves an
  OCSP response"""

  def serve_forever_on_thread(self):
    self.thread = threading.Thread(target = self.serve_forever,
                                   name = "OCSPServerThread")
    self.thread.start()

  def stop_serving(self):
    self.shutdown()
    self.thread.join()


class HTTPSServer(tlslite.api.TLSSocketServerMixIn,
                  testserver_base.ClientRestrictingServerMixIn,
                  testserver_base.BrokenPipeHandlerMixIn,
                  testserver_base.StoppableHTTPServer):
  """This is a specialization of StoppableHTTPServer that add https support and
  client verification."""

  def __init__(self, server_address, request_hander_class, pem_cert_and_key,
               ssl_client_auth, ssl_client_cas, ssl_client_cert_types,
               ssl_bulk_ciphers, ssl_key_exchanges, alpn_protocols,
               npn_protocols, record_resume_info, tls_intolerant,
               tls_intolerance_type, signed_cert_timestamps,
               fallback_scsv_enabled, ocsp_response,
               alert_after_handshake, disable_channel_id, disable_ems,
               simulate_tls13_downgrade, simulate_tls12_downgrade,
               tls_max_version):
    self.cert_chain = tlslite.api.X509CertChain()
    self.cert_chain.parsePemList(pem_cert_and_key)
    # Force using only python implementation - otherwise behavior is different
    # depending on whether m2crypto Python module is present (error is thrown
    # when it is). m2crypto uses a C (based on OpenSSL) implementation under
    # the hood.
    self.private_key = tlslite.api.parsePEMKey(pem_cert_and_key,
                                               private=True,
                                               implementations=['python'])
    self.ssl_client_auth = ssl_client_auth
    self.ssl_client_cas = []
    self.ssl_client_cert_types = []
    self.npn_protocols = npn_protocols
    self.signed_cert_timestamps = signed_cert_timestamps
    self.fallback_scsv_enabled = fallback_scsv_enabled
    self.ocsp_response = ocsp_response

    if ssl_client_auth:
      for ca_file in ssl_client_cas:
        s = open(ca_file).read()
        x509 = tlslite.api.X509()
        x509.parse(s)
        self.ssl_client_cas.append(x509.subject)

      for cert_type in ssl_client_cert_types:
        self.ssl_client_cert_types.append({
            "rsa_sign": tlslite.api.ClientCertificateType.rsa_sign,
            "ecdsa_sign": tlslite.api.ClientCertificateType.ecdsa_sign,
            }[cert_type])

    self.ssl_handshake_settings = tlslite.api.HandshakeSettings()
    # Enable SSLv3 for testing purposes.
    self.ssl_handshake_settings.minVersion = (3, 0)
    if ssl_bulk_ciphers is not None:
      self.ssl_handshake_settings.cipherNames = ssl_bulk_ciphers
    if ssl_key_exchanges is not None:
      self.ssl_handshake_settings.keyExchangeNames = ssl_key_exchanges
    if tls_intolerant != 0:
      self.ssl_handshake_settings.tlsIntolerant = (3, tls_intolerant)
      self.ssl_handshake_settings.tlsIntoleranceType = tls_intolerance_type
    if alert_after_handshake:
      self.ssl_handshake_settings.alertAfterHandshake = True
    if disable_channel_id:
      self.ssl_handshake_settings.enableChannelID = False
    if disable_ems:
      self.ssl_handshake_settings.enableExtendedMasterSecret = False
    if simulate_tls13_downgrade:
      self.ssl_handshake_settings.simulateTLS13Downgrade = True
    if simulate_tls12_downgrade:
      self.ssl_handshake_settings.simulateTLS12Downgrade = True
    if tls_max_version != 0:
      self.ssl_handshake_settings.maxVersion = (3, tls_max_version)
    self.ssl_handshake_settings.alpnProtos=alpn_protocols;

    if record_resume_info:
      # If record_resume_info is true then we'll replace the session cache with
      # an object that records the lookups and inserts that it sees.
      self.session_cache = RecordingSSLSessionCache()
    else:
      self.session_cache = tlslite.api.SessionCache()
    testserver_base.StoppableHTTPServer.__init__(self,
                                                 server_address,
                                                 request_hander_class)

  def handshake(self, tlsConnection):
    """Creates the SSL connection."""

    try:
      self.tlsConnection = tlsConnection
      tlsConnection.handshakeServer(certChain=self.cert_chain,
                                    privateKey=self.private_key,
                                    sessionCache=self.session_cache,
                                    reqCert=self.ssl_client_auth,
                                    settings=self.ssl_handshake_settings,
                                    reqCAs=self.ssl_client_cas,
                                    reqCertTypes=self.ssl_client_cert_types,
                                    nextProtos=self.npn_protocols,
                                    signedCertTimestamps=
                                    self.signed_cert_timestamps,
                                    fallbackSCSV=self.fallback_scsv_enabled,
                                    ocspResponse = self.ocsp_response)
      tlsConnection.ignoreAbruptClose = True
      return True
    except tlslite.api.TLSAbruptCloseError:
      # Ignore abrupt close.
      return True
    except tlslite.api.TLSError, error:
      print "Handshake failure:", str(error)
      return False


class FTPServer(testserver_base.ClientRestrictingServerMixIn,
                pyftpdlib.ftpserver.FTPServer):
  """This is a specialization of FTPServer that adds client verification."""

  pass


class TCPEchoServer(testserver_base.ClientRestrictingServerMixIn,
                    SocketServer.TCPServer):
  """A TCP echo server that echoes back what it has received."""

  def server_bind(self):
    """Override server_bind to store the server name."""

    SocketServer.TCPServer.server_bind(self)
    host, port = self.socket.getsockname()[:2]
    self.server_name = socket.getfqdn(host)
    self.server_port = port

  def serve_forever(self):
    self.stop = False
    self.nonce_time = None
    while not self.stop:
      self.handle_request()
    self.socket.close()


class UDPEchoServer(testserver_base.ClientRestrictingServerMixIn,
                    SocketServer.UDPServer):
  """A UDP echo server that echoes back what it has received."""

  def server_bind(self):
    """Override server_bind to store the server name."""

    SocketServer.UDPServer.server_bind(self)
    host, port = self.socket.getsockname()[:2]
    self.server_name = socket.getfqdn(host)
    self.server_port = port

  def serve_forever(self):
    self.stop = False
    self.nonce_time = None
    while not self.stop:
      self.handle_request()
    self.socket.close()


class TestPageHandler(testserver_base.BasePageHandler):
  # Class variables to allow for persistence state between page handler
  # invocations
  rst_limits = {}
  fail_precondition = {}

  def __init__(self, request, client_address, socket_server):
    connect_handlers = [
      self.RedirectConnectHandler,
      self.ServerAuthConnectHandler,
      self.DefaultConnectResponseHandler]
    get_handlers = [
      self.NoCacheMaxAgeTimeHandler,
      self.NoCacheTimeHandler,
      self.CacheTimeHandler,
      self.CacheExpiresHandler,
      self.CacheProxyRevalidateHandler,
      self.CachePrivateHandler,
      self.CachePublicHandler,
      self.CacheSMaxAgeHandler,
      self.CacheMustRevalidateHandler,
      self.CacheMustRevalidateMaxAgeHandler,
      self.CacheNoStoreHandler,
      self.CacheNoStoreMaxAgeHandler,
      self.CacheNoTransformHandler,
      self.DownloadHandler,
      self.DownloadFinishHandler,
      self.EchoHeader,
      self.EchoHeaderCache,
      self.EchoAllHandler,
      self.ZipFileHandler,
      self.FileHandler,
      self.SetCookieHandler,
      self.SetManyCookiesHandler,
      self.ExpectAndSetCookieHandler,
      self.SetHeaderHandler,
      self.AuthBasicHandler,
      self.AuthDigestHandler,
      self.SlowServerHandler,
      self.ChunkedServerHandler,
      self.NoContentHandler,
      self.ServerRedirectHandler,
      self.CrossSiteRedirectHandler,
      self.ClientRedirectHandler,
      self.GetSSLSessionCacheHandler,
      self.SSLManySmallRecords,
      self.GetChannelID,
      self.GetClientCert,
      self.ClientCipherListHandler,
      self.CloseSocketHandler,
      self.DefaultResponseHandler]
    post_handlers = [
      self.EchoTitleHandler,
      self.EchoHandler,
      self.PostOnlyFileHandler,
      self.EchoMultipartPostHandler] + get_handlers
    put_handlers = [
      self.EchoTitleHandler,
      self.EchoHandler] + get_handlers
    head_handlers = [
      self.FileHandler,
      self.DefaultResponseHandler]

    self._mime_types = {
      'crx' : 'application/x-chrome-extension',
      'exe' : 'application/octet-stream',
      'gif': 'image/gif',
      'jpeg' : 'image/jpeg',
      'jpg' : 'image/jpeg',
      'js' : 'application/javascript',
      'json': 'application/json',
      'pdf' : 'application/pdf',
      'txt' : 'text/plain',
      'wav' : 'audio/wav',
      'xml' : 'text/xml'
    }
    self._default_mime_type = 'text/html'

    testserver_base.BasePageHandler.__init__(self, request, client_address,
                                             socket_server, connect_handlers,
                                             get_handlers, head_handlers,
                                             post_handlers, put_handlers)

  def GetMIMETypeFromName(self, file_name):
    """Returns the mime type for the specified file_name. So far it only looks
    at the file extension."""

    (_shortname, extension) = os.path.splitext(file_name.split("?")[0])
    if len(extension) == 0:
      # no extension.
      return self._default_mime_type

    # extension starts with a dot, so we need to remove it
    return self._mime_types.get(extension[1:], self._default_mime_type)

  def NoCacheMaxAgeTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and no caching requested."""

    if not self._ShouldHandleRequest("/nocachetime/maxage"):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'max-age=0')
    self.send_header('Content-Type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def NoCacheTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and no caching requested."""

    if not self._ShouldHandleRequest("/nocachetime"):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'no-cache')
    self.send_header('Content-Type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheTimeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for one minute."""

    if not self._ShouldHandleRequest("/cachetime"):
      return False

    self.send_response(200)
    self.send_header('Cache-Control', 'max-age=60')
    self.send_header('Content-Type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheExpiresHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and set the page to expire on 1 Jan 2099."""

    if not self._ShouldHandleRequest("/cache/expires"):
      return False

    self.send_response(200)
    self.send_header('Expires', 'Thu, 1 Jan 2099 00:00:00 GMT')
    self.send_header('Content-Type', 'text/html')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheProxyRevalidateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 60 seconds"""

    if not self._ShouldHandleRequest("/cache/proxy-revalidate"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, proxy-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CachePrivateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 3 seconds."""

    if not self._ShouldHandleRequest("/cache/private"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'max-age=3, private')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CachePublicHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and allows caching for 3 seconds."""

    if not self._ShouldHandleRequest("/cache/public"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'max-age=3, public')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheSMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow for caching."""

    if not self._ShouldHandleRequest("/cache/s-maxage"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'public, s-maxage = 60, max-age = 0')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheMustRevalidateHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow caching."""

    if not self._ShouldHandleRequest("/cache/must-revalidate"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'must-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheMustRevalidateMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow caching event though max-age of 60
    seconds is specified."""

    if not self._ShouldHandleRequest("/cache/must-revalidate/max-age"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, must-revalidate')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheNoStoreHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the page to be stored."""

    if not self._ShouldHandleRequest("/cache/no-store"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'no-store')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def CacheNoStoreMaxAgeHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the page to be stored even though max-age
    of 60 seconds is specified."""

    if not self._ShouldHandleRequest("/cache/no-store/max-age"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'max-age=60, no-store')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True


  def CacheNoTransformHandler(self):
    """This request handler yields a page with the title set to the current
    system time, and does not allow the content to transformed during
    user-agent caching"""

    if not self._ShouldHandleRequest("/cache/no-transform"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'no-transform')
    self.end_headers()

    self.wfile.write('<html><head><title>%s</title></head></html>' %
                     time.time())

    return True

  def EchoHeader(self):
    """This handler echoes back the value of a specific request header."""

    return self.EchoHeaderHelper("/echoheader")

  def EchoHeaderCache(self):
    """This function echoes back the value of a specific request header while
    allowing caching for 10 hours."""

    return self.EchoHeaderHelper("/echoheadercache")

  def EchoHeaderHelper(self, echo_header):
    """This function echoes back the value of the request header passed in."""

    if not self._ShouldHandleRequest(echo_header):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      header_name = self.path[query_char+1:]

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    if echo_header == '/echoheadercache':
      self.send_header('Cache-control', 'max-age=60000')
    else:
      self.send_header('Cache-control', 'no-cache')
    # insert a vary header to properly indicate that the cachability of this
    # request is subject to value of the request header being echoed.
    if len(header_name) > 0:
      self.send_header('Vary', header_name)
    self.end_headers()

    if len(header_name) > 0:
      self.wfile.write(self.headers.getheader(header_name))

    return True

  def ReadRequestBody(self):
    """This function reads the body of the current HTTP request, handling
    both plain and chunked transfer encoded requests."""

    if self.headers.getheader('transfer-encoding') != 'chunked':
      length = int(self.headers.getheader('content-length'))
      return self.rfile.read(length)

    # Read the request body as chunks.
    body = ""
    while True:
      line = self.rfile.readline()
      length = int(line, 16)
      if length == 0:
        self.rfile.readline()
        break
      body += self.rfile.read(length)
      self.rfile.read(2)
    return body

  def EchoHandler(self):
    """This handler just echoes back the payload of the request, for testing
    form submission."""

    if not self._ShouldHandleRequest("/echo"):
      return False

    _, _, _, _, query, _ = urlparse.urlparse(self.path)
    query_params = cgi.parse_qs(query, True)
    if 'status' in query_params:
      self.send_response(int(query_params['status'][0]))
    else:
      self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write(self.ReadRequestBody())
    return True

  def EchoTitleHandler(self):
    """This handler is like Echo, but sets the page title to the request."""

    if not self._ShouldHandleRequest("/echotitle"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    request = self.ReadRequestBody()
    self.wfile.write('<html><head><title>')
    self.wfile.write(request)
    self.wfile.write('</title></head></html>')
    return True

  def EchoAllHandler(self):
    """This handler yields a (more) human-readable page listing information
    about the request header & contents."""

    if not self._ShouldHandleRequest("/echoall"):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head><style>'
      'pre { border: 1px solid black; margin: 5px; padding: 5px }'
      '</style></head><body>'
      '<div style="float: right">'
      '<a href="/echo">back to referring page</a></div>'
      '<h1>Request Body:</h1><pre>')

    if self.command == 'POST' or self.command == 'PUT':
      qs = self.ReadRequestBody()
      params = cgi.parse_qs(qs, keep_blank_values=1)

      for param in params:
        self.wfile.write('%s=%s\n' % (param, params[param][0]))

    self.wfile.write('</pre>')

    self.wfile.write('<h1>Request Headers:</h1><pre>%s</pre>' % self.headers)

    self.wfile.write('</body></html>')
    return True

  def EchoMultipartPostHandler(self):
    """This handler echoes received multipart post data as json format."""

    if not (self._ShouldHandleRequest("/echomultipartpost") or
            self._ShouldHandleRequest("/searchbyimage")):
      return False

    content_type, parameters = cgi.parse_header(
        self.headers.getheader('content-type'))
    if content_type == 'multipart/form-data':
      post_multipart = cgi.parse_multipart(self.rfile, parameters)
    elif content_type == 'application/x-www-form-urlencoded':
      raise Exception('POST by application/x-www-form-urlencoded is '
                      'not implemented.')
    else:
      post_multipart = {}

    # Since the data can be binary, we encode them by base64.
    post_multipart_base64_encoded = {}
    for field, values in post_multipart.items():
      post_multipart_base64_encoded[field] = [base64.b64encode(value)
                                              for value in values]

    result = {'POST_multipart' : post_multipart_base64_encoded}

    self.send_response(200)
    self.send_header("Content-type", "text/plain")
    self.end_headers()
    self.wfile.write(json.dumps(result, indent=2, sort_keys=False))
    return True

  def DownloadHandler(self):
    """This handler sends a downloadable file with or without reporting
    the size (6K)."""

    if self.path.startswith("/download-unknown-size"):
      send_length = False
    elif self.path.startswith("/download-known-size"):
      send_length = True
    else:
      return False

    #
    # The test which uses this functionality is attempting to send
    # small chunks of data to the client.  Use a fairly large buffer
    # so that we'll fill chrome's IO buffer enough to force it to
    # actually write the data.
    # See also the comments in the client-side of this test in
    # download_uitest.cc
    #
    size_chunk1 = 35*1024
    size_chunk2 = 10*1024

    self.send_response(200)
    self.send_header('Content-Type', 'application/octet-stream')
    self.send_header('Cache-Control', 'max-age=0')
    if send_length:
      self.send_header('Content-Length', size_chunk1 + size_chunk2)
    self.end_headers()

    # First chunk of data:
    self.wfile.write("*" * size_chunk1)
    self.wfile.flush()

    # handle requests until one of them clears this flag.
    self.server.wait_for_download = True
    while self.server.wait_for_download:
      self.server.handle_request()

    # Second chunk of data:
    self.wfile.write("*" * size_chunk2)
    return True

  def DownloadFinishHandler(self):
    """This handler just tells the server to finish the current download."""

    if not self._ShouldHandleRequest("/download-finish"):
      return False

    self.server.wait_for_download = False
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.send_header('Cache-Control', 'max-age=0')
    self.end_headers()
    return True

  def _ReplaceFileData(self, data, query_parameters):
    """Replaces matching substrings in a file.

    If the 'replace_text' URL query parameter is present, it is expected to be
    of the form old_text:new_text, which indicates that any old_text strings in
    the file are replaced with new_text. Multiple 'replace_text' parameters may
    be specified.

    If the parameters are not present, |data| is returned.
    """

    query_dict = cgi.parse_qs(query_parameters)
    replace_text_values = query_dict.get('replace_text', [])
    for replace_text_value in replace_text_values:
      replace_text_args = replace_text_value.split(':')
      if len(replace_text_args) != 2:
        raise ValueError(
          'replace_text must be of form old_text:new_text. Actual value: %s' %
          replace_text_value)
      old_text_b64, new_text_b64 = replace_text_args
      old_text = base64.urlsafe_b64decode(old_text_b64)
      new_text = base64.urlsafe_b64decode(new_text_b64)
      data = data.replace(old_text, new_text)
    return data

  def ZipFileHandler(self):
    """This handler sends the contents of the requested file in compressed form.
    Can pass in a parameter that specifies that the content length be
    C - the compressed size (OK),
    U - the uncompressed size (Non-standard, but handled),
    S - less than compressed (OK because we keep going),
    M - larger than compressed but less than uncompressed (an error),
    L - larger than uncompressed (an error)
    Example: compressedfiles/Picture_1.doc?C
    """

    prefix = "/compressedfiles/"
    if not self.path.startswith(prefix):
      return False

    # Consume a request body if present.
    if self.command == 'POST' or self.command == 'PUT' :
      self.ReadRequestBody()

    _, _, url_path, _, query, _ = urlparse.urlparse(self.path)

    if not query in ('C', 'U', 'S', 'M', 'L'):
      return False

    sub_path = url_path[len(prefix):]
    entries = sub_path.split('/')
    file_path = os.path.join(self.server.data_dir, *entries)
    if os.path.isdir(file_path):
      file_path = os.path.join(file_path, 'index.html')

    if not os.path.isfile(file_path):
      print "File not found " + sub_path + " full path:" + file_path
      self.send_error(404)
      return True

    f = open(file_path, "rb")
    data = f.read()
    uncompressed_len = len(data)
    f.close()

    # Compress the data.
    data = zlib.compress(data)
    compressed_len = len(data)

    content_length = compressed_len
    if query == 'U':
      content_length = uncompressed_len
    elif query == 'S':
      content_length = compressed_len / 2
    elif query == 'M':
      content_length = (compressed_len + uncompressed_len) / 2
    elif query == 'L':
      content_length = compressed_len + uncompressed_len

    self.send_response(200)
    self.send_header('Content-Type', 'application/msword')
    self.send_header('Content-encoding', 'deflate')
    self.send_header('Connection', 'close')
    self.send_header('Content-Length', content_length)
    self.send_header('ETag', '\'' + file_path + '\'')
    self.end_headers()

    self.wfile.write(data)

    return True

  def FileHandler(self):
    """This handler sends the contents of the requested file.  Wow, it's like
    a real webserver!"""

    prefix = self.server.file_root_url
    if not self.path.startswith(prefix):
      return False
    return self._FileHandlerHelper(prefix)

  def PostOnlyFileHandler(self):
    """This handler sends the contents of the requested file on a POST."""

    prefix = urlparse.urljoin(self.server.file_root_url, 'post/')
    if not self.path.startswith(prefix):
      return False
    return self._FileHandlerHelper(prefix)

  def _FileHandlerHelper(self, prefix):
    request_body = ''
    if self.command == 'POST' or self.command == 'PUT':
      # Consume a request body if present.
      request_body = self.ReadRequestBody()

    _, _, url_path, _, query, _ = urlparse.urlparse(self.path)
    query_dict = cgi.parse_qs(query)

    expected_body = query_dict.get('expected_body', [])
    if expected_body and request_body not in expected_body:
      self.send_response(404)
      self.end_headers()
      self.wfile.write('')
      return True

    expected_headers = query_dict.get('expected_headers', [])
    for expected_header in expected_headers:
      header_name, expected_value = expected_header.split(':')
      if self.headers.getheader(header_name) != expected_value:
        self.send_response(404)
        self.end_headers()
        self.wfile.write('')
        return True

    sub_path = url_path[len(prefix):]
    entries = sub_path.split('/')
    file_path = os.path.join(self.server.data_dir, *entries)
    if os.path.isdir(file_path):
      file_path = os.path.join(file_path, 'index.html')

    if not os.path.isfile(file_path):
      print "File not found " + sub_path + " full path:" + file_path
      self.send_error(404)
      return True

    f = open(file_path, "rb")
    data = f.read()
    f.close()

    data = self._ReplaceFileData(data, query)

    old_protocol_version = self.protocol_version

    # If file.mock-http-headers exists, it contains the headers we
    # should send.  Read them in and parse them.
    headers_path = file_path + '.mock-http-headers'
    if os.path.isfile(headers_path):
      f = open(headers_path, "r")

      # "HTTP/1.1 200 OK"
      response = f.readline()
      http_major, http_minor, status_code = re.findall(
          'HTTP/(\d+).(\d+) (\d+)', response)[0]
      self.protocol_version = "HTTP/%s.%s" % (http_major, http_minor)
      self.send_response(int(status_code))

      for line in f:
        header_values = re.findall('(\S+):\s*(.*)', line)
        if len(header_values) > 0:
          # "name: value"
          name, value = header_values[0]
          self.send_header(name, value)
      f.close()
    else:
      # Could be more generic once we support mime-type sniffing, but for
      # now we need to set it explicitly.

      range_header = self.headers.get('Range')
      if range_header and range_header.startswith('bytes='):
        # Note this doesn't handle all valid byte range_header values (i.e.
        # left open ended ones), just enough for what we needed so far.
        range_header = range_header[6:].split('-')
        start = int(range_header[0])
        if range_header[1]:
          end = int(range_header[1])
        else:
          end = len(data) - 1

        self.send_response(206)
        content_range = ('bytes ' + str(start) + '-' + str(end) + '/' +
                         str(len(data)))
        self.send_header('Content-Range', content_range)
        data = data[start: end + 1]
      else:
        self.send_response(200)

      self.send_header('Content-Type', self.GetMIMETypeFromName(file_path))
      self.send_header('Accept-Ranges', 'bytes')
      self.send_header('Content-Length', len(data))
      self.send_header('ETag', '\'' + file_path + '\'')
    self.end_headers()

    if (self.command != 'HEAD'):
      self.wfile.write(data)

    self.protocol_version = old_protocol_version
    return True

  def SetCookieHandler(self):
    """This handler just sets a cookie, for testing cookie handling."""

    if not self._ShouldHandleRequest("/set-cookie"):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      cookie_values = self.path[query_char + 1:].split('&')
    else:
      cookie_values = ("",)
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    for cookie_value in cookie_values:
      self.send_header('Set-Cookie', '%s' % cookie_value)
    self.end_headers()
    for cookie_value in cookie_values:
      self.wfile.write('%s' % cookie_value)
    return True

  def SetManyCookiesHandler(self):
    """This handler just sets a given number of cookies, for testing handling
       of large numbers of cookies."""

    if not self._ShouldHandleRequest("/set-many-cookies"):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      num_cookies = int(self.path[query_char + 1:])
    else:
      num_cookies = 0
    self.send_response(200)
    self.send_header('', 'text/html')
    for _i in range(0, num_cookies):
      self.send_header('Set-Cookie', 'a=')
    self.end_headers()
    self.wfile.write('%d cookies were sent' % num_cookies)
    return True

  def ExpectAndSetCookieHandler(self):
    """Expects some cookies to be sent, and if they are, sets more cookies.

    The expect parameter specifies a required cookie.  May be specified multiple
    times.
    The set parameter specifies a cookie to set if all required cookies are
    preset.  May be specified multiple times.
    The data parameter specifies the response body data to be returned."""

    if not self._ShouldHandleRequest("/expect-and-set-cookie"):
      return False

    _, _, _, _, query, _ = urlparse.urlparse(self.path)
    query_dict = cgi.parse_qs(query)
    cookies = set()
    if 'Cookie' in self.headers:
      cookie_header = self.headers.getheader('Cookie')
      cookies.update([s.strip() for s in cookie_header.split(';')])
    got_all_expected_cookies = True
    for expected_cookie in query_dict.get('expect', []):
      if expected_cookie not in cookies:
        got_all_expected_cookies = False
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    if got_all_expected_cookies:
      for cookie_value in query_dict.get('set', []):
        self.send_header('Set-Cookie', '%s' % cookie_value)
    self.end_headers()
    for data_value in query_dict.get('data', []):
      self.wfile.write(data_value)
    return True

  def SetHeaderHandler(self):
    """This handler sets a response header. Parameters are in the
    key%3A%20value&key2%3A%20value2 format."""

    if not self._ShouldHandleRequest("/set-header"):
      return False

    query_char = self.path.find('?')
    if query_char != -1:
      headers_values = self.path[query_char + 1:].split('&')
    else:
      headers_values = ("",)
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    for header_value in headers_values:
      header_value = urllib.unquote(header_value)
      (key, value) = header_value.split(': ', 1)
      self.send_header(key, value)
    self.end_headers()
    for header_value in headers_values:
      self.wfile.write('%s' % header_value)
    return True

  def AuthBasicHandler(self):
    """This handler tests 'Basic' authentication.  It just sends a page with
    title 'user/pass' if you succeed."""

    if not self._ShouldHandleRequest("/auth-basic"):
      return False

    username = userpass = password = b64str = ""
    expected_password = 'secret'
    realm = 'testrealm'
    set_cookie_if_challenged = False

    _, _, url_path, _, query, _ = urlparse.urlparse(self.path)
    query_params = cgi.parse_qs(query, True)
    if 'set-cookie-if-challenged' in query_params:
      set_cookie_if_challenged = True
    if 'password' in query_params:
      expected_password = query_params['password'][0]
    if 'realm' in query_params:
      realm = query_params['realm'][0]

    auth = self.headers.getheader('authorization')
    try:
      if not auth:
        raise Exception('no auth')
      b64str = re.findall(r'Basic (\S+)', auth)[0]
      userpass = base64.b64decode(b64str)
      username, password = re.findall(r'([^:]+):(\S+)', userpass)[0]
      if password != expected_password:
        raise Exception('wrong password')
    except Exception, e:
      # Authentication failed.
      self.send_response(401)
      self.send_header('WWW-Authenticate', 'Basic realm="%s"' % realm)
      self.send_header('Content-Type', 'text/html')
      if set_cookie_if_challenged:
        self.send_header('Set-Cookie', 'got_challenged=true')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>Denied: %s</title>' % e)
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('b64str=%s<p>' % b64str)
      self.wfile.write('username: %s<p>' % username)
      self.wfile.write('userpass: %s<p>' % userpass)
      self.wfile.write('password: %s<p>' % password)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('</body></html>')
      return True

    # Authentication successful.  (Return a cachable response to allow for
    # testing cached pages that require authentication.)
    old_protocol_version = self.protocol_version
    self.protocol_version = "HTTP/1.1"

    if_none_match = self.headers.getheader('if-none-match')
    if if_none_match == "abc":
      self.send_response(304)
      self.end_headers()
    elif url_path.endswith(".gif"):
      # Using chrome/test/data/google/logo.gif as the test image
      test_image_path = ['google', 'logo.gif']
      gif_path = os.path.join(self.server.data_dir, *test_image_path)
      if not os.path.isfile(gif_path):
        self.send_error(404)
        self.protocol_version = old_protocol_version
        return True

      f = open(gif_path, "rb")
      data = f.read()
      f.close()

      self.send_response(200)
      self.send_header('Content-Type', 'image/gif')
      self.send_header('Cache-control', 'max-age=60000')
      self.send_header('Etag', 'abc')
      self.end_headers()
      self.wfile.write(data)
    else:
      self.send_response(200)
      self.send_header('Content-Type', 'text/html')
      self.send_header('Cache-control', 'max-age=60000')
      self.send_header('Etag', 'abc')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>%s/%s</title>' % (username, password))
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('</body></html>')

    self.protocol_version = old_protocol_version
    return True

  def GetNonce(self, force_reset=False):
    """Returns a nonce that's stable per request path for the server's lifetime.
    This is a fake implementation. A real implementation would only use a given
    nonce a single time (hence the name n-once). However, for the purposes of
    unittesting, we don't care about the security of the nonce.

    Args:
      force_reset: Iff set, the nonce will be changed. Useful for testing the
          "stale" response.
    """

    if force_reset or not self.server.nonce_time:
      self.server.nonce_time = time.time()
    return hashlib.md5('privatekey%s%d' %
                       (self.path, self.server.nonce_time)).hexdigest()

  def AuthDigestHandler(self):
    """This handler tests 'Digest' authentication.

    It just sends a page with title 'user/pass' if you succeed.

    A stale response is sent iff "stale" is present in the request path.
    """

    if not self._ShouldHandleRequest("/auth-digest"):
      return False

    stale = 'stale' in self.path
    nonce = self.GetNonce(force_reset=stale)
    opaque = hashlib.md5('opaque').hexdigest()
    password = 'secret'
    realm = 'testrealm'

    auth = self.headers.getheader('authorization')
    pairs = {}
    try:
      if not auth:
        raise Exception('no auth')
      if not auth.startswith('Digest'):
        raise Exception('not digest')
      # Pull out all the name="value" pairs as a dictionary.
      pairs = dict(re.findall(r'(\b[^ ,=]+)="?([^",]+)"?', auth))

      # Make sure it's all valid.
      if pairs['nonce'] != nonce:
        raise Exception('wrong nonce')
      if pairs['opaque'] != opaque:
        raise Exception('wrong opaque')

      # Check the 'response' value and make sure it matches our magic hash.
      # See http://www.ietf.org/rfc/rfc2617.txt
      hash_a1 = hashlib.md5(
          ':'.join([pairs['username'], realm, password])).hexdigest()
      hash_a2 = hashlib.md5(':'.join([self.command, pairs['uri']])).hexdigest()
      if 'qop' in pairs and 'nc' in pairs and 'cnonce' in pairs:
        response = hashlib.md5(':'.join([hash_a1, nonce, pairs['nc'],
            pairs['cnonce'], pairs['qop'], hash_a2])).hexdigest()
      else:
        response = hashlib.md5(':'.join([hash_a1, nonce, hash_a2])).hexdigest()

      if pairs['response'] != response:
        raise Exception('wrong password')
    except Exception, e:
      # Authentication failed.
      self.send_response(401)
      hdr = ('Digest '
             'realm="%s", '
             'domain="/", '
             'qop="auth", '
             'algorithm=MD5, '
             'nonce="%s", '
             'opaque="%s"') % (realm, nonce, opaque)
      if stale:
        hdr += ', stale="TRUE"'
      self.send_header('WWW-Authenticate', hdr)
      self.send_header('Content-Type', 'text/html')
      self.end_headers()
      self.wfile.write('<html><head>')
      self.wfile.write('<title>Denied: %s</title>' % e)
      self.wfile.write('</head><body>')
      self.wfile.write('auth=%s<p>' % auth)
      self.wfile.write('pairs=%s<p>' % pairs)
      self.wfile.write('You sent:<br>%s<p>' % self.headers)
      self.wfile.write('We are replying:<br>%s<p>' % hdr)
      self.wfile.write('</body></html>')
      return True

    # Authentication successful.
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('<title>%s/%s</title>' % (pairs['username'], password))
    self.wfile.write('</head><body>')
    self.wfile.write('auth=%s<p>' % auth)
    self.wfile.write('pairs=%s<p>' % pairs)
    self.wfile.write('</body></html>')

    return True

  def SlowServerHandler(self):
    """Wait for the user suggested time before responding. The syntax is
    /slow?0.5 to wait for half a second."""

    if not self._ShouldHandleRequest("/slow"):
      return False
    query_char = self.path.find('?')
    wait_sec = 1.0
    if query_char >= 0:
      try:
        wait_sec = float(self.path[query_char + 1:])
      except ValueError:
        pass
    time.sleep(wait_sec)
    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()
    self.wfile.write("waited %.1f seconds" % wait_sec)
    return True

  def ChunkedServerHandler(self):
    """Send chunked response. Allows to specify chunks parameters:
     - waitBeforeHeaders - ms to wait before sending headers
     - waitBetweenChunks - ms to wait between chunks
     - chunkSize - size of each chunk in bytes
     - chunksNumber - number of chunks
    Example: /chunked?waitBeforeHeaders=1000&chunkSize=5&chunksNumber=5
    waits one second, then sends headers and five chunks five bytes each."""

    if not self._ShouldHandleRequest("/chunked"):
      return False
    query_char = self.path.find('?')
    chunkedSettings = {'waitBeforeHeaders' : 0,
                       'waitBetweenChunks' : 0,
                       'chunkSize' : 5,
                       'chunksNumber' : 5}
    if query_char >= 0:
      params = self.path[query_char + 1:].split('&')
      for param in params:
        keyValue = param.split('=')
        if len(keyValue) == 2:
          try:
            chunkedSettings[keyValue[0]] = int(keyValue[1])
          except ValueError:
            pass
    time.sleep(0.001 * chunkedSettings['waitBeforeHeaders'])
    self.protocol_version = 'HTTP/1.1' # Needed for chunked encoding
    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.send_header('Connection', 'close')
    self.send_header('Transfer-Encoding', 'chunked')
    self.end_headers()
    # Chunked encoding: sending all chunks, then final zero-length chunk and
    # then final CRLF.
    for i in range(0, chunkedSettings['chunksNumber']):
      if i > 0:
        time.sleep(0.001 * chunkedSettings['waitBetweenChunks'])
      self.sendChunkHelp('*' * chunkedSettings['chunkSize'])
      self.wfile.flush() # Keep in mind that we start flushing only after 1kb.
    self.sendChunkHelp('')
    return True

  def NoContentHandler(self):
    """Returns a 204 No Content response."""

    if not self._ShouldHandleRequest("/nocontent"):
      return False
    self.send_response(204)
    self.end_headers()
    return True

  def ServerRedirectHandler(self):
    """Sends a server redirect to the given URL. The syntax is
    '/server-redirect?http://foo.bar/asdf' to redirect to
    'http://foo.bar/asdf'"""

    test_name = "/server-redirect"
    if not self._ShouldHandleRequest(test_name):
      return False

    query_char = self.path.find('?')
    if query_char < 0 or len(self.path) <= query_char + 1:
      self.sendRedirectHelp(test_name)
      return True
    dest = urllib.unquote(self.path[query_char + 1:])

    self.send_response(301)  # moved permanently
    self.send_header('Location', dest)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True

  def CrossSiteRedirectHandler(self):
    """Sends a server redirect to the given site. The syntax is
    '/cross-site/hostname/...' to redirect to //hostname/...
    It is used to navigate between different Sites, causing
    cross-site/cross-process navigations in the browser."""

    test_name = "/cross-site"
    if not self._ShouldHandleRequest(test_name):
      return False

    params = urllib.unquote(self.path[(len(test_name) + 1):])
    slash = params.find('/')
    if slash < 0:
      self.sendRedirectHelp(test_name)
      return True

    host = params[:slash]
    path = params[(slash+1):]
    dest = "//%s:%s/%s" % (host, str(self.server.server_port), path)

    self.send_response(301)  # moved permanently
    self.send_header('Location', dest)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True

  def ClientRedirectHandler(self):
    """Sends a client redirect to the given URL. The syntax is
    '/client-redirect?http://foo.bar/asdf' to redirect to
    'http://foo.bar/asdf'"""

    test_name = "/client-redirect"
    if not self._ShouldHandleRequest(test_name):
      return False

    query_char = self.path.find('?')
    if query_char < 0 or len(self.path) <= query_char + 1:
      self.sendRedirectHelp(test_name)
      return True
    dest = urllib.unquote(self.path[query_char + 1:])

    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><head>')
    self.wfile.write('<meta http-equiv="refresh" content="0;url=%s">' % dest)
    self.wfile.write('</head><body>Redirecting to %s</body></html>' % dest)

    return True

  def GetSSLSessionCacheHandler(self):
    """Send a reply containing a log of the session cache operations."""

    if not self._ShouldHandleRequest('/ssl-session-cache'):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()
    try:
      log = self.server.session_cache.log
    except AttributeError:
      self.wfile.write('Pass --https-record-resume in order to use' +
                       ' this request')
      return True

    for (action, sessionID) in log:
      self.wfile.write('%s\t%s\n' % (action, bytes(sessionID).encode('hex')))
    return True

  def SSLManySmallRecords(self):
    """Sends a reply consisting of a variety of small writes. These will be
    translated into a series of small SSL records when used over an HTTPS
    server."""

    if not self._ShouldHandleRequest('/ssl-many-small-records'):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()

    # Write ~26K of data, in 1350 byte chunks
    for i in xrange(20):
      self.wfile.write('*' * 1350)
      self.wfile.flush()
    return True

  def GetChannelID(self):
    """Send a reply containing the hashed ChannelID that the client provided."""

    if not self._ShouldHandleRequest('/channel-id'):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()
    channel_id = bytes(self.server.tlsConnection.channel_id)
    self.wfile.write(hashlib.sha256(channel_id).digest().encode('base64'))
    return True

  def GetClientCert(self):
    """Send a reply whether a client certificate was provided."""

    if not self._ShouldHandleRequest('/client-cert'):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()

    cert_chain = self.server.tlsConnection.session.clientCertChain
    if cert_chain != None:
      self.wfile.write('got client cert with fingerprint: ' +
                       cert_chain.getFingerprint())
    else:
      self.wfile.write('got no client cert')
    return True

  def ClientCipherListHandler(self):
    """Send a reply containing the cipher suite list that the client
    provided. Each cipher suite value is serialized in decimal, followed by a
    newline."""

    if not self._ShouldHandleRequest('/client-cipher-list'):
      return False

    self.send_response(200)
    self.send_header('Content-Type', 'text/plain')
    self.end_headers()

    cipher_suites = self.server.tlsConnection.clientHello.cipher_suites
    self.wfile.write('\n'.join(str(c) for c in cipher_suites))
    return True

  def CloseSocketHandler(self):
    """Closes the socket without sending anything."""

    if not self._ShouldHandleRequest('/close-socket'):
      return False

    self.wfile.close()
    return True

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
      self.wfile.write(contents)
    return True

  def RedirectConnectHandler(self):
    """Sends a redirect to the CONNECT request for www.redirect.com. This
    response is not specified by the RFC, so the browser should not follow
    the redirect."""

    if (self.path.find("www.redirect.com") < 0):
      return False

    dest = "http://www.destination.com/foo.js"

    self.send_response(302)  # moved temporarily
    self.send_header('Location', dest)
    self.send_header('Connection', 'close')
    self.end_headers()
    return True

  def ServerAuthConnectHandler(self):
    """Sends a 401 to the CONNECT request for www.server-auth.com. This
    response doesn't make sense because the proxy server cannot request
    server authentication."""

    if (self.path.find("www.server-auth.com") < 0):
      return False

    challenge = 'Basic realm="WallyWorld"'

    self.send_response(401)  # unauthorized
    self.send_header('WWW-Authenticate', challenge)
    self.send_header('Connection', 'close')
    self.end_headers()
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
    self.wfile.write(contents)
    return True

  # called by the redirect handling function when there is no parameter
  def sendRedirectHelp(self, redirect_name):
    self.send_response(200)
    self.send_header('Content-Type', 'text/html')
    self.end_headers()
    self.wfile.write('<html><body><h1>Error: no redirect destination</h1>')
    self.wfile.write('Use <pre>%s?http://dest...</pre>' % redirect_name)
    self.wfile.write('</body></html>')

  # called by chunked handling function
  def sendChunkHelp(self, chunk):
    # Each chunk consists of: chunk size (hex), CRLF, chunk body, CRLF
    self.wfile.write('%X\r\n' % len(chunk))
    self.wfile.write(chunk)
    self.wfile.write('\r\n')


class OCSPHandler(testserver_base.BasePageHandler):
  def __init__(self, request, client_address, socket_server):
    handlers = [self.OCSPResponse, self.CaIssuersResponse]
    self.ocsp_response = socket_server.ocsp_response
    self.ocsp_response_intermediate = socket_server.ocsp_response_intermediate
    self.ca_issuers_response = socket_server.ca_issuers_response
    testserver_base.BasePageHandler.__init__(self, request, client_address,
                                             socket_server, [], handlers, [],
                                             handlers, [])

  def OCSPResponse(self):
    if self._ShouldHandleRequest("/ocsp"):
      response = self.ocsp_response
    elif self._ShouldHandleRequest("/ocsp_intermediate"):
      response = self.ocsp_response_intermediate
    else:
      return False
    print 'handling ocsp request'
    self.send_response(200)
    self.send_header('Content-Type', 'application/ocsp-response')
    self.send_header('Content-Length', str(len(response)))
    self.end_headers()

    self.wfile.write(response)

  def CaIssuersResponse(self):
    if not self._ShouldHandleRequest("/ca_issuers"):
      return False
    print 'handling ca_issuers request'
    self.send_response(200)
    self.send_header('Content-Type', 'application/pkix-cert')
    self.send_header('Content-Length', str(len(self.ca_issuers_response)))
    self.end_headers()

    self.wfile.write(self.ca_issuers_response)


class TCPEchoHandler(SocketServer.BaseRequestHandler):
  """The RequestHandler class for TCP echo server.

  It is instantiated once per connection to the server, and overrides the
  handle() method to implement communication to the client.
  """

  def handle(self):
    """Handles the request from the client and constructs a response."""

    data = self.request.recv(65536).strip()
    # Verify the "echo request" message received from the client. Send back
    # "echo response" message if "echo request" message is valid.
    try:
      return_data = echo_message.GetEchoResponseData(data)
      if not return_data:
        return
    except ValueError:
      return

    self.request.send(return_data)


class UDPEchoHandler(SocketServer.BaseRequestHandler):
  """The RequestHandler class for UDP echo server.

  It is instantiated once per connection to the server, and overrides the
  handle() method to implement communication to the client.
  """

  def handle(self):
    """Handles the request from the client and constructs a response."""

    data = self.request[0].strip()
    request_socket = self.request[1]
    # Verify the "echo request" message received from the client. Send back
    # "echo response" message if "echo request" message is valid.
    try:
      return_data = echo_message.GetEchoResponseData(data)
      if not return_data:
        return
    except ValueError:
      return
    request_socket.sendto(return_data, self.client_address)


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
      sock.send('%s %s %s\r\n' % (
          self.command, path, self.protocol_version))
      for header in self.headers.headers:
        header = header.strip()
        if (header.lower().startswith('connection') or
            header.lower().startswith('proxy')):
          continue
        sock.send('%s\r\n' % header)
      sock.send('\r\n')
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

    auth = self.headers.getheader('Proxy-Authorization')
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
    self.__ocsp_server = None

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

  def __parse_ocsp_options(self, states_option, date_option, produced_option):
    if states_option is None:
      return None, None, None

    ocsp_states = list()
    for ocsp_state_arg in states_option.split(':'):
      if ocsp_state_arg == 'ok':
        ocsp_state = minica.OCSP_STATE_GOOD
      elif ocsp_state_arg == 'revoked':
        ocsp_state = minica.OCSP_STATE_REVOKED
      elif ocsp_state_arg == 'invalid':
        ocsp_state = minica.OCSP_STATE_INVALID_RESPONSE
      elif ocsp_state_arg == 'unauthorized':
        ocsp_state = minica.OCSP_STATE_UNAUTHORIZED
      elif ocsp_state_arg == 'unknown':
        ocsp_state = minica.OCSP_STATE_UNKNOWN
      elif ocsp_state_arg == 'later':
        ocsp_state = minica.OCSP_STATE_TRY_LATER
      elif ocsp_state_arg == 'invalid_data':
        ocsp_state = minica.OCSP_STATE_INVALID_RESPONSE_DATA
      elif ocsp_state_arg == "mismatched_serial":
        ocsp_state = minica.OCSP_STATE_MISMATCHED_SERIAL
      else:
        raise testserver_base.OptionError('unknown OCSP status: ' +
            ocsp_state_arg)
      ocsp_states.append(ocsp_state)

    if len(ocsp_states) > 1:
      if set(ocsp_states) & OCSP_STATES_NO_SINGLE_RESPONSE:
        raise testserver_base.OptionError('Multiple OCSP responses '
            'incompatible with states ' + str(ocsp_states))

    ocsp_dates = list()
    for ocsp_date_arg in date_option.split(':'):
      if ocsp_date_arg == 'valid':
        ocsp_date = minica.OCSP_DATE_VALID
      elif ocsp_date_arg == 'old':
        ocsp_date = minica.OCSP_DATE_OLD
      elif ocsp_date_arg == 'early':
        ocsp_date = minica.OCSP_DATE_EARLY
      elif ocsp_date_arg == 'long':
        ocsp_date = minica.OCSP_DATE_LONG
      elif ocsp_date_arg == 'longer':
        ocsp_date = minica.OCSP_DATE_LONGER
      else:
        raise testserver_base.OptionError('unknown OCSP date: ' +
            ocsp_date_arg)
      ocsp_dates.append(ocsp_date)

    if len(ocsp_states) != len(ocsp_dates):
      raise testserver_base.OptionError('mismatched ocsp and ocsp-date '
          'count')

    ocsp_produced = None
    if produced_option == 'valid':
      ocsp_produced = minica.OCSP_PRODUCED_VALID
    elif produced_option == 'before':
      ocsp_produced = minica.OCSP_PRODUCED_BEFORE_CERT
    elif produced_option == 'after':
      ocsp_produced = minica.OCSP_PRODUCED_AFTER_CERT
    else:
      raise testserver_base.OptionError('unknown OCSP produced: ' +
          produced_option)

    return ocsp_states, ocsp_dates, ocsp_produced

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

    if self.options.server_type == SERVER_HTTP:
      if self.options.https:
        pem_cert_and_key = None
        ocsp_der = None
        if self.options.cert_and_key_file:
          if not os.path.isfile(self.options.cert_and_key_file):
            raise testserver_base.OptionError(
                'specified server cert file not found: ' +
                self.options.cert_and_key_file + ' exiting...')
          pem_cert_and_key = file(self.options.cert_and_key_file, 'r').read()
        elif self.options.aia_intermediate:
          self.__ocsp_server = OCSPServer((host, 0), OCSPHandler)
          print ('AIA server started on %s:%d...' %
              (host, self.__ocsp_server.server_port))

          ocsp_server_port = self.__ocsp_server.server_port
          if self.options.ocsp_proxy_port_number != 0:
            ocsp_server_port = self.options.ocsp_proxy_port_number
            server_data['ocsp_port'] = self.__ocsp_server.server_port

          (pem_cert_and_key, intermediate_cert_der) = \
              minica.GenerateCertKeyAndIntermediate(
                  subject = self.options.cert_common_name,
                  ip_sans=ip_sans, dns_sans=dns_sans,
                  ca_issuers_url =
                      ("http://%s:%d/ca_issuers" % (host, ocsp_server_port)),
                  serial = self.options.cert_serial)

          self.__ocsp_server.ocsp_response = None
          self.__ocsp_server.ocsp_response_intermediate = None
          self.__ocsp_server.ca_issuers_response = intermediate_cert_der
        else:
          # generate a new certificate and run an OCSP server for it.
          self.__ocsp_server = OCSPServer((host, 0), OCSPHandler)
          print ('OCSP server started on %s:%d...' %
              (host, self.__ocsp_server.server_port))

          ocsp_states, ocsp_dates, ocsp_produced =  self.__parse_ocsp_options(
                  self.options.ocsp,
                  self.options.ocsp_date,
                  self.options.ocsp_produced)

          (ocsp_intermediate_states, ocsp_intermediate_dates,
           ocsp_intermediate_produced) =  self.__parse_ocsp_options(
                  self.options.ocsp_intermediate,
                  self.options.ocsp_intermediate_date,
                  self.options.ocsp_intermediate_produced)

          ocsp_server_port = self.__ocsp_server.server_port
          if self.options.ocsp_proxy_port_number != 0:
            ocsp_server_port = self.options.ocsp_proxy_port_number
            server_data['ocsp_port'] = self.__ocsp_server.server_port

          pem_cert_and_key, (ocsp_der,
           ocsp_intermediate_der) = minica.GenerateCertKeyAndOCSP(
              subject = self.options.cert_common_name,
              ip_sans = ip_sans,
              dns_sans = dns_sans,
              ocsp_url = ("http://%s:%d/ocsp" % (host, ocsp_server_port)),
              ocsp_states = ocsp_states,
              ocsp_dates = ocsp_dates,
              ocsp_produced = ocsp_produced,
              ocsp_intermediate_url = (
                  "http://%s:%d/ocsp_intermediate" % (host, ocsp_server_port)
                  if ocsp_intermediate_states else None),
              ocsp_intermediate_states = ocsp_intermediate_states,
              ocsp_intermediate_dates = ocsp_intermediate_dates,
              ocsp_intermediate_produced = ocsp_intermediate_produced,
              serial = self.options.cert_serial)

          if self.options.ocsp_server_unavailable:
            # SEQUENCE containing ENUMERATED with value 3 (tryLater).
            self.__ocsp_server.ocsp_response_intermediate = \
                self.__ocsp_server.ocsp_response = '30030a0103'.decode('hex')
          else:
            self.__ocsp_server.ocsp_response = ocsp_der
            self.__ocsp_server.ocsp_response_intermediate = \
                ocsp_intermediate_der
          self.__ocsp_server.ca_issuers_response = None

        for ca_cert in self.options.ssl_client_ca:
          if not os.path.isfile(ca_cert):
            raise testserver_base.OptionError(
                'specified trusted client CA file not found: ' + ca_cert +
                ' exiting...')

        stapled_ocsp_response = None
        if self.options.staple_ocsp_response:
          # TODO(mattm): Staple the intermediate response too (if applicable,
          # and if chrome ever supports it).
          stapled_ocsp_response = ocsp_der

        server = HTTPSServer((host, port), TestPageHandler, pem_cert_and_key,
                             self.options.ssl_client_auth,
                             self.options.ssl_client_ca,
                             self.options.ssl_client_cert_type,
                             self.options.ssl_bulk_cipher,
                             self.options.ssl_key_exchange,
                             self.options.alpn_protocols,
                             self.options.npn_protocols,
                             self.options.record_resume,
                             self.options.tls_intolerant,
                             self.options.tls_intolerance_type,
                             self.options.signed_cert_timestamps_tls_ext.decode(
                                 "base64"),
                             self.options.fallback_scsv,
                             stapled_ocsp_response,
                             self.options.alert_after_handshake,
                             self.options.disable_channel_id,
                             self.options.disable_extended_master_secret,
                             self.options.simulate_tls13_downgrade,
                             self.options.simulate_tls12_downgrade,
                             self.options.tls_max_version)
        print 'HTTPS server started on https://%s:%d...' % \
            (host, server.server_port)
      else:
        server = HTTPServer((host, port), TestPageHandler)
        print 'HTTP server started on http://%s:%d...' % \
            (host, server.server_port)

      server.data_dir = self.__make_data_dir()
      server.file_root_url = self.options.file_root_url
      server_data['port'] = server.server_port
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
      print 'Trying to start websocket server on %s://%s:%d...' % \
          (scheme, websocket_options.server_host, websocket_options.port)
      server = WebSocketServer(websocket_options)
      print 'WebSocket server started on %s://%s:%d...' % \
          (scheme, host, server.server_port)
      server_data['port'] = server.server_port
      websocket_options.use_basic_auth = self.options.ws_basic_auth
    elif self.options.server_type == SERVER_TCP_ECHO:
      # Used for generating the key (randomly) that encodes the "echo request"
      # message.
      random.seed()
      server = TCPEchoServer((host, port), TCPEchoHandler)
      print 'Echo TCP server started on port %d...' % server.server_port
      server_data['port'] = server.server_port
    elif self.options.server_type == SERVER_UDP_ECHO:
      # Used for generating the key (randomly) that encodes the "echo request"
      # message.
      random.seed()
      server = UDPEchoServer((host, port), UDPEchoHandler)
      print 'Echo UDP server started on port %d...' % server.server_port
      server_data['port'] = server.server_port
    elif self.options.server_type == SERVER_PROXY:
      ProxyRequestHandler.redirect_connect_to_localhost = \
          self.options.redirect_connect_to_localhost
      server = ThreadingHTTPServer((host, port), ProxyRequestHandler)
      print 'Proxy server started on port %d...' % server.server_port
      server_data['port'] = server.server_port
    elif self.options.server_type == SERVER_BASIC_AUTH_PROXY:
      ProxyRequestHandler.redirect_connect_to_localhost = \
          self.options.redirect_connect_to_localhost
      server = ThreadingHTTPServer((host, port), BasicAuthProxyRequestHandler)
      print 'BasicAuthProxy server started on port %d...' % server.server_port
      server_data['port'] = server.server_port
    elif self.options.server_type == SERVER_FTP:
      my_data_dir = self.__make_data_dir()

      # Instantiate a dummy authorizer for managing 'virtual' users
      authorizer = pyftpdlib.ftpserver.DummyAuthorizer()

      # Define a new user having full r/w permissions
      authorizer.add_user('chrome', 'chrome', my_data_dir, perm='elradfmw')

      # Define a read-only anonymous user unless disabled
      if not self.options.no_anonymous_ftp_user:
        authorizer.add_anonymous(my_data_dir)

      # Instantiate FTP handler class
      ftp_handler = pyftpdlib.ftpserver.FTPHandler
      ftp_handler.authorizer = authorizer

      # Define a customized banner (string returned when client connects)
      ftp_handler.banner = ("pyftpdlib %s based ftpd ready." %
                            pyftpdlib.ftpserver.__ver__)

      # Instantiate FTP server class and listen to address:port
      server = pyftpdlib.ftpserver.FTPServer((host, port), ftp_handler)
      server_data['port'] = server.socket.getsockname()[1]
      print 'FTP server started on port %d...' % server_data['port']
    else:
      raise testserver_base.OptionError('unknown server type' +
          self.options.server_type)

    return server

  def run_server(self):
    if self.__ocsp_server:
      self.__ocsp_server.serve_forever_on_thread()

    testserver_base.TestServerRunner.run_server(self)

    if self.__ocsp_server:
      self.__ocsp_server.stop_serving()

  def add_options(self):
    testserver_base.TestServerRunner.add_options(self)
    self.option_parser.add_option('-f', '--ftp', action='store_const',
                                  const=SERVER_FTP, default=SERVER_HTTP,
                                  dest='server_type',
                                  help='start up an FTP server.')
    self.option_parser.add_option('--tcp-echo', action='store_const',
                                  const=SERVER_TCP_ECHO, default=SERVER_HTTP,
                                  dest='server_type',
                                  help='start up a tcp echo server.')
    self.option_parser.add_option('--udp-echo', action='store_const',
                                  const=SERVER_UDP_ECHO, default=SERVER_HTTP,
                                  dest='server_type',
                                  help='start up a udp echo server.')
    self.option_parser.add_option('--proxy', action='store_const',
                                  const=SERVER_PROXY,
                                  default=SERVER_HTTP, dest='server_type',
                                  help='start up a proxy server.')
    self.option_parser.add_option('--basic-auth-proxy', action='store_const',
                                  const=SERVER_BASIC_AUTH_PROXY,
                                  default=SERVER_HTTP, dest='server_type',
                                  help='start up a proxy server which requires '
                                  'basic authentication.')
    self.option_parser.add_option('--websocket', action='store_const',
                                  const=SERVER_WEBSOCKET, default=SERVER_HTTP,
                                  dest='server_type',
                                  help='start up a WebSocket server.')
    self.option_parser.add_option('--https', action='store_true',
                                  dest='https', help='Specify that https '
                                  'should be used.')
    self.option_parser.add_option('--cert-and-key-file',
                                  dest='cert_and_key_file', help='specify the '
                                  'path to the file containing the certificate '
                                  'and private key for the server in PEM '
                                  'format')
    self.option_parser.add_option('--aia-intermediate', action='store_true',
                                  dest='aia_intermediate',
                                  help='generate a certificate chain that '
                                  'requires AIA cert fetching, and run a '
                                  'server to respond to the AIA request.')
    self.option_parser.add_option('--ocsp', dest='ocsp', default='ok',
                                  help='The type of OCSP response generated '
                                  'for the automatically generated '
                                  'certificate. One of [ok,revoked,invalid]')
    self.option_parser.add_option('--ocsp-date', dest='ocsp_date',
                                  default='valid', help='The validity of the '
                                  'range between thisUpdate and nextUpdate')
    self.option_parser.add_option('--ocsp-produced', dest='ocsp_produced',
                                  default='valid', help='producedAt relative '
                                  'to certificate expiry')
    self.option_parser.add_option('--ocsp-intermediate',
                                  dest='ocsp_intermediate', default=None,
                                  help='If specified, the automatically '
                                  'generated chain will include an '
                                  'intermediate certificate with this type '
                                  'of OCSP response (see docs for --ocsp)')
    self.option_parser.add_option('--ocsp-intermediate-date',
                                  dest='ocsp_intermediate_date',
                                  default='valid', help='The validity of the '
                                  'range between thisUpdate and nextUpdate')
    self.option_parser.add_option('--ocsp-intermediate-produced',
                                  dest='ocsp_intermediate_produced',
                                  default='valid', help='producedAt relative '
                                  'to certificate expiry')
    self.option_parser.add_option('--cert-serial', dest='cert_serial',
                                  default=0, type=int,
                                  help='If non-zero then the generated '
                                  'certificate will have this serial number')
    self.option_parser.add_option('--cert-common-name', dest='cert_common_name',
                                  default="127.0.0.1",
                                  help='The generated certificate will have '
                                  'this common name')
    self.option_parser.add_option('--tls-intolerant', dest='tls_intolerant',
                                  default='0', type='int',
                                  help='If nonzero, certain TLS connections '
                                  'will be aborted in order to test version '
                                  'fallback. 1 means all TLS versions will be '
                                  'aborted. 2 means TLS 1.1 or higher will be '
                                  'aborted. 3 means TLS 1.2 or higher will be '
                                  'aborted. 4 means TLS 1.3 or higher will be '
                                  'aborted.')
    self.option_parser.add_option('--tls-intolerance-type',
                                  dest='tls_intolerance_type',
                                  default="alert",
                                  help='Controls how the server reacts to a '
                                  'TLS version it is intolerant to. Valid '
                                  'values are "alert", "close", and "reset".')
    self.option_parser.add_option('--signed-cert-timestamps-tls-ext',
                                  dest='signed_cert_timestamps_tls_ext',
                                  default='',
                                  help='Base64 encoded SCT list. If set, '
                                  'server will respond with a '
                                  'signed_certificate_timestamp TLS extension '
                                  'whenever the client supports it.')
    self.option_parser.add_option('--fallback-scsv', dest='fallback_scsv',
                                  default=False, const=True,
                                  action='store_const',
                                  help='If given, TLS_FALLBACK_SCSV support '
                                  'will be enabled. This causes the server to '
                                  'reject fallback connections from compatible '
                                  'clients (e.g. Chrome).')
    self.option_parser.add_option('--staple-ocsp-response',
                                  dest='staple_ocsp_response',
                                  default=False, action='store_true',
                                  help='If set, server will staple the OCSP '
                                  'response whenever OCSP is on and the client '
                                  'supports OCSP stapling.')
    self.option_parser.add_option('--https-record-resume',
                                  dest='record_resume', const=True,
                                  default=False, action='store_const',
                                  help='Record resumption cache events rather '
                                  'than resuming as normal. Allows the use of '
                                  'the /ssl-session-cache request')
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
    self.option_parser.add_option('--ssl-client-cert-type', action='append',
                                  default=[], help='Specify that the client '
                                  'certificate request should include the '
                                  'specified certificate_type value. This '
                                  'option may appear multiple times, '
                                  'indicating multiple values should be send '
                                  'in the request. Valid values are '
                                  '"rsa_sign", "dss_sign", and "ecdsa_sign". '
                                  'If omitted, "rsa_sign" will be used.')
    self.option_parser.add_option('--ssl-bulk-cipher', action='append',
                                  help='Specify the bulk encryption '
                                  'algorithm(s) that will be accepted by the '
                                  'SSL server. Valid values are "aes128gcm", '
                                  '"aes256", "aes128", "3des", "rc4". If '
                                  'omitted, all algorithms will be used. This '
                                  'option may appear multiple times, '
                                  'indicating multiple algorithms should be '
                                  'enabled.');
    self.option_parser.add_option('--ssl-key-exchange', action='append',
                                  help='Specify the key exchange algorithm(s)'
                                  'that will be accepted by the SSL server. '
                                  'Valid values are "rsa", "dhe_rsa", '
                                  '"ecdhe_rsa". If omitted, all algorithms '
                                  'will be used. This option may appear '
                                  'multiple times, indicating multiple '
                                  'algorithms should be enabled.');
    self.option_parser.add_option('--alpn-protocols', action='append',
                                  help='Specify the list of ALPN protocols.  '
                                  'The server will not send an ALPN response '
                                  'if this list does not overlap with the '
                                  'list of protocols the client advertises.')
    self.option_parser.add_option('--npn-protocols', action='append',
                                  help='Specify the list of protocols sent in '
                                  'an NPN response.  The server will not'
                                  'support NPN if the list is empty.')
    self.option_parser.add_option('--file-root-url', default='/files/',
                                  help='Specify a root URL for files served.')
    # TODO(ricea): Generalize this to support basic auth for HTTP too.
    self.option_parser.add_option('--ws-basic-auth', action='store_true',
                                  dest='ws_basic_auth',
                                  help='Enable basic-auth for WebSocket')
    self.option_parser.add_option('--ocsp-server-unavailable',
                                  dest='ocsp_server_unavailable',
                                  default=False, action='store_true',
                                  help='If set, the OCSP server will return '
                                  'a tryLater status rather than the actual '
                                  'OCSP response.')
    self.option_parser.add_option('--ocsp-proxy-port-number', default=0,
                                  type='int', dest='ocsp_proxy_port_number',
                                  help='Port allocated for OCSP proxy '
                                  'when connection is proxied.')
    self.option_parser.add_option('--alert-after-handshake',
                                  dest='alert_after_handshake',
                                  default=False, action='store_true',
                                  help='If set, the server will send a fatal '
                                  'alert immediately after the handshake.')
    self.option_parser.add_option('--no-anonymous-ftp-user',
                                  dest='no_anonymous_ftp_user',
                                  default=False, action='store_true',
                                  help='If set, the FTP server will not create '
                                  'an anonymous user.')
    self.option_parser.add_option('--disable-channel-id', action='store_true')
    self.option_parser.add_option('--disable-extended-master-secret',
                                  action='store_true')
    self.option_parser.add_option('--simulate-tls13-downgrade',
                                  action='store_true')
    self.option_parser.add_option('--simulate-tls12-downgrade',
                                  action='store_true')
    self.option_parser.add_option('--tls-max-version', default='0', type='int',
                                  help='If non-zero, the maximum TLS version '
                                  'to support. 1 means TLS 1.0, 2 means '
                                  'TLS 1.1, and 3 means TLS 1.2.')
    self.option_parser.add_option('--redirect-connect-to-localhost',
                                  dest='redirect_connect_to_localhost',
                                  default=False, action='store_true',
                                  help='If set, the Proxy server will connect '
                                  'to localhost instead of the requested URL '
                                  'on CONNECT requests')


if __name__ == '__main__':
  sys.exit(ServerRunner().main())
