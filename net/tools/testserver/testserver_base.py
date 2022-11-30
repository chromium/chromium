# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from six.moves import BaseHTTPServer
import errno
import json
import optparse
import os
import re
import socket
from six.moves import socketserver as SocketServer
import struct
import sys
import warnings

# Ignore deprecation warnings, they make our output more cluttered.
warnings.filterwarnings("ignore", category=DeprecationWarning)

if sys.platform == 'win32':
  import msvcrt

# Using debug() seems to cause hangs on XP: see http://crbug.com/64515.
debug_output = sys.stderr
def debug(string):
  debug_output.write(string + "\n")
  debug_output.flush()


class Error(Exception):
  """Error class for this module."""


class OptionError(Error):
  """Error for bad command line options."""


class FileMultiplexer(object):
  def __init__(self, fd1, fd2) :
    self.__fd1 = fd1
    self.__fd2 = fd2

  def __del__(self) :
    if self.__fd1 != sys.stdout and self.__fd1 != sys.stderr:
      self.__fd1.close()
    if self.__fd2 != sys.stdout and self.__fd2 != sys.stderr:
      self.__fd2.close()

  def write(self, text) :
    self.__fd1.write(text)
    self.__fd2.write(text)

  def flush(self) :
    self.__fd1.flush()
    self.__fd2.flush()


class ClientRestrictingServerMixIn:
  """Implements verify_request to limit connections to our configured IP
  address."""

  def verify_request(self, _request, client_address):
    return client_address[0] == self.server_address[0]


class BrokenPipeHandlerMixIn:
  """Allows the server to deal with "broken pipe" errors (which happen if the
  browser quits with outstanding requests, like for the favicon). This mix-in
  requires the class to derive from SocketServer.BaseServer and not override its
  handle_error() method. """

  def handle_error(self, request, client_address):
    value = sys.exc_info()[1]
    if isinstance(value, socket.error):
      err = value.args[0]
      if sys.platform in ('win32', 'cygwin'):
        # "An established connection was aborted by the software in your host."
        pipe_err = 10053
      else:
        pipe_err = errno.EPIPE
      if err == pipe_err:
        print("testserver.py: Broken pipe")
        return
      if err == errno.ECONNRESET:
        print("testserver.py: Connection reset by peer")
        return
    SocketServer.BaseServer.handle_error(self, request, client_address)


class StoppableHTTPServer(BaseHTTPServer.HTTPServer):
  """This is a specialization of BaseHTTPServer to allow it
  to be exited cleanly (by setting its "stop" member to True)."""

  def serve_forever(self):
    self.stop = False
    self.nonce_time = None
    while not self.stop:
      self.handle_request()
    self.socket.close()


def MultiplexerHack(std_fd, log_fd):
  """Creates a FileMultiplexer that will write to both specified files.

  When running on Windows XP bots, stdout and stderr will be invalid file
  handles, so log_fd will be returned directly.  (This does not occur if you
  run the test suite directly from a console, but only if the output of the
  test executable is redirected.)
  """
  if std_fd.fileno() <= 0:
    return log_fd
  return FileMultiplexer(std_fd, log_fd)


class BasePageHandler(BaseHTTPServer.BaseHTTPRequestHandler):

  def __init__(self, request, client_address, socket_server,
               connect_handlers, get_handlers, head_handlers, post_handlers,
               put_handlers):
    self._connect_handlers = connect_handlers
    self._get_handlers = get_handlers
    self._head_handlers = head_handlers
    self._post_handlers = post_handlers
    self._put_handlers = put_handlers
    BaseHTTPServer.BaseHTTPRequestHandler.__init__(
      self, request, client_address, socket_server)

  def log_request(self, *args, **kwargs):
    # Disable request logging to declutter test log output.
    pass

  def _ShouldHandleRequest(self, handler_name):
    """Determines if the path can be handled by the handler.

    We consider a handler valid if the path begins with the
    handler name. It can optionally be followed by "?*", "/*".
    """

    pattern = re.compile('%s($|\?|/).*' % handler_name)
    return pattern.match(self.path)

  def do_CONNECT(self):
    for handler in self._connect_handlers:
      if handler():
        return

  def do_GET(self):
    for handler in self._get_handlers:
      if handler():
        return

  def do_HEAD(self):
    for handler in self._head_handlers:
      if handler():
        return

  def do_POST(self):
    for handler in self._post_handlers:
      if handler():
        return

  def do_PUT(self):
    for handler in self._put_handlers:
      if handler():
        return


class TestServerRunner(object):
  """Runs a test server and communicates with the controlling C++ test code.

  Subclasses should override the create_server method to create their server
  object, and the add_options method to add their own options.
  """

  def __init__(self):
    self.option_parser = optparse.OptionParser()
    self.add_options()

  def main(self):
    self.options, self.args = self.option_parser.parse_args()

    logfile = open(self.options.log_file, 'w')

    # http://crbug.com/248796 : Error logs streamed to normal sys.stderr will be
    # written to HTTP response payload when remote test server is used.
    # For this reason, some tests like ResourceFetcherTests.ResourceFetcher404
    # were failing on Android because remote test server is being used there.
    # To fix them, we need to use sys.stdout as sys.stderr if remote test server
    # is used.
    if self.options.on_remote_server:
      sys.stderr = sys.stdout

    sys.stderr = MultiplexerHack(sys.stderr, logfile)
    if self.options.log_to_console:
      sys.stdout = MultiplexerHack(sys.stdout, logfile)
    else:
      sys.stdout = logfile

    server_data = {
      'host': self.options.host,
    }
    self.server = self.create_server(server_data)
    self._notify_startup_complete(server_data)
    self.run_server()

  def create_server(self, server_data):
    """Creates a server object and returns it.

    Must populate server_data['port'], and can set additional server_data
    elements if desired."""
    raise NotImplementedError()

  def run_server(self):
    try:
      self.server.serve_forever()
    except KeyboardInterrupt:
      print('shutting down server')
      self.server.stop = True

  def add_options(self):
    self.option_parser.add_option('--startup-pipe', type='int',
                                  dest='startup_pipe',
                                  help='File handle of pipe to parent process')
    self.option_parser.add_option('--log-to-console', action='store_const',
                                  const=True, default=False,
                                  dest='log_to_console',
                                  help='Enables or disables sys.stdout logging '
                                  'to the console.')
    self.option_parser.add_option('--log-file', default='testserver.log',
                                  dest='log_file',
                                  help='The name of the server log file.')
    self.option_parser.add_option('--port', default=0, type='int',
                                  help='Port used by the server. If '
                                  'unspecified, the server will listen on an '
                                  'ephemeral port.')
    self.option_parser.add_option('--host', default='127.0.0.1',
                                  dest='host',
                                  help='Hostname or IP upon which the server '
                                  'will listen. Client connections will also '
                                  'only be allowed from this address.')
    self.option_parser.add_option('--data-dir', dest='data_dir',
                                  help='Directory from which to read the '
                                  'files.')
    self.option_parser.add_option('--on-remote-server', action='store_const',
                                  const=True, default=False,
                                  dest='on_remote_server',
                                  help='Whether remote server is being used or '
                                  'not.')

  def _notify_startup_complete(self, server_data):
    # Notify the parent that we've started. (BaseServer subclasses
    # bind their sockets on construction.)
    if self.options.startup_pipe is not None:
      server_data_json = json.dumps(server_data).encode()
      server_data_len = len(server_data_json)
      print('sending server_data: %s (%d bytes)' %
            (server_data_json, server_data_len))
      if sys.platform == 'win32':
        fd = msvcrt.open_osfhandle(self.options.startup_pipe, 0)
      else:
        fd = self.options.startup_pipe
      startup_pipe = os.fdopen(fd, "wb")
      # First write the data length as an unsigned 4-byte value.  This
      # is _not_ using network byte ordering since the other end of the
      # pipe is on the same machine.
      startup_pipe.write(struct.pack('=L', server_data_len))
      startup_pipe.write(server_data_json)
      startup_pipe.close()
