#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# [VPYTHON:BEGIN]
# wheel: <
#   name: "infra/python/wheels/pytz-py2_py3"
#   version: "version:2018.4"
# >
# wheel: <
#   name: "infra/python/wheels/tempora-py2_py3"
#   version: "version:1.11"
# >
# wheel: <
#   name: "infra/python/wheels/more-itertools-py2_py3"
#   version: "version:4.1.0"
# >
# wheel: <
#   name: "infra/python/wheels/backports_functools_lru_cache-py2_py3"
#   version: "version:1.5"
# >
# wheel: <
#   name: "infra/python/wheels/six-py2_py3"
#   version: "version:1.12.0"
# >
# wheel: <
#   name: "infra/python/wheels/portend-py2_py3"
#   version: "version:2.2"
# >
# wheel: <
#   name: "infra/python/wheels/cheroot-py2_py3"
#   version: "version:6.2.4"
# >
# wheel: <
#   name: "infra/python/wheels/cherrypy-py2_py3"
#   version: "version:14.2.0"
# >
# [VPYTHON:END]

"""Constrained Network Server. Serves files with supplied network constraints.

The CNS exposes a web based API allowing network constraints to be imposed on
file serving.

TODO(dalecurtis): Add some more docs here.

"""

import logging
from logging import handlers
import mimetypes
import optparse
import os
import signal
import sys
import threading
import time
import urllib
import urllib2

import traffic_control

try:
  import cherrypy
except ImportError:
  print ('CNS requires CherryPy v3 or higher to be installed. Please install\n'
         'and try again. On Linux: sudo apt-get install python-cherrypy3\n')
  sys.exit(1)

# Add webm file types to mimetypes map since cherrypy's default type is text.
mimetypes.types_map['.webm'] = 'video/webm'

# Default logging is ERROR. Use --verbose to enable DEBUG logging.
_DEFAULT_LOG_LEVEL = logging.ERROR

# Default port to serve the CNS on.
_DEFAULT_SERVING_PORT = 9000

# Default port range for constrained use.
_DEFAULT_CNS_PORT_RANGE = (50000, 51000)

# Default number of seconds before a port can be torn down.
_DEFAULT_PORT_EXPIRY_TIME_SECS = 5 * 60


class PortAllocator(object):
  """Dynamically allocates/deallocates ports with a given set of constraints."""

  def __init__(self, port_range, expiry_time_secs=5 * 60):
    """Sets up initial state for the Port Allocator.

    Args:
      port_range: Range of ports available for allocation.
      expiry_time_secs: Amount of time in seconds before constrained ports are
          cleaned up.
    """
    self._port_range = port_range
    self._expiry_time_secs = expiry_time_secs

    # Keeps track of ports we've used, the creation key, and the last request
    # time for the port so they can be cached and cleaned up later.
    self._ports = {}

    # Locks port creation and cleanup. TODO(dalecurtis): If performance becomes
    # an issue a per-port based lock system can be used instead.
    self._port_lock = threading.RLock()

  def Get(self, key, new_port=False, **kwargs):
    """Sets up a constrained port using the requested parameters.

    Requests for the same key and constraints will result in a cached port being
    returned if possible, subject to new_port.

    Args:
      key: Used to cache ports with the given constraints.
      new_port: Whether to create a new port or use an existing one if possible.
      **kwargs: Constraints to pass into traffic control.

    Returns:
      None if no port can be setup or the port number of the constrained port.
    """
    with self._port_lock:
      # Check port key cache to see if this port is already setup. Update the
      # cache time and return the port if so. Performance isn't a concern here,
      # so just iterate over ports dict for simplicity.
      full_key = (key,) + tuple(kwargs.values())
      if not new_port:
        for port, status in self._ports.iteritems():
          if full_key == status['key']:
            self._ports[port]['last_update'] = time.time()
            return port

      # Cleanup ports on new port requests. Do it after the cache check though
      # so we don't erase and then setup the same port.
      if self._expiry_time_secs > 0:
        self.Cleanup(all_ports=False)

      # Performance isn't really an issue here, so just iterate over the port
      # range to find an unused port. If no port is found, None is returned.
      for port in xrange(self._port_range[0], self._port_range[1]):
        if port in self._ports:
          continue
        if self._SetupPort(port, **kwargs):
          kwargs['port'] = port
          self._ports[port] = {'last_update': time.time(), 'key': full_key,
                               'config': kwargs}
          return port

  def _SetupPort(self, port, **kwargs):
    """Setup network constraints on port using the requested parameters.

    Args:
      port: The port number to setup network constraints on.
      **kwargs: Network constraints to set up on the port.

    Returns:
      True if setting the network constraints on the port was successful, false
      otherwise.
    """
    kwargs['port'] = port
    try:
      cherrypy.log('Setting up port %d' % port)
      traffic_control.CreateConstrainedPort(kwargs)
      return True
    except traffic_control.TrafficControlError as e:
      cherrypy.log('Error: %s\nOutput: %s' % (e.msg, e.error))
      return False

  def Cleanup(self, all_ports, request_ip=None):
    """Cleans up expired ports, or if all_ports=True, all allocated ports.

    By default, ports which haven't been used for self._expiry_time_secs are
    torn down. If all_ports=True then they are torn down regardless.

    Args:
      all_ports: Should all ports be torn down regardless of expiration?
      request_ip: Tear ports matching the IP address regarless of expiration.
    """
    with self._port_lock:
      now = time.time()
      # Use .items() instead of .iteritems() so we can delete keys w/o error.
      for port, status in self._ports.items():
        expired = now - status['last_update'] > self._expiry_time_secs
        matching_ip = request_ip and status['key'][0].startswith(request_ip)
        if all_ports or expired or matching_ip:
          cherrypy.log('Cleaning up port %d' % port)
          self._DeletePort(port)
          del self._ports[port]

  def _DeletePort(self, port):
    """Deletes network constraints on port.

    Args:
      port: The port number associated with the network constraints.
    """
    try:
      traffic_control.DeleteConstrainedPort(self._ports[port]['config'])
    except traffic_control.TrafficControlError as e:
      cherrypy.log('Error: %s\nOutput: %s' % (e.msg, e.error))


class ConstrainedNetworkServer(object):
  """A CherryPy-based HTTP server for serving files with network constraints."""

  def __init__(self, options, port_allocator):
    """Sets up initial state for the CNS.

    Args:
      options: optparse based class returned by ParseArgs()
      port_allocator: A port allocator instance.
    """
    self._options = options
    self._port_allocator = port_allocator

  @cherrypy.expose
  def Cleanup(self):
    """Cleans up all the ports allocated using the request IP address.

    When requesting a constrained port, the cherrypy.request.remote.ip is used
    as a key for that port (in addition to other request parameters).  Such
    ports created for the same IP address are removed.
    """
    cherrypy.log('Cleaning up ports allocated by %s.' %
                 cherrypy.request.remote.ip)
    self._port_allocator.Cleanup(all_ports=False,
                                 request_ip=cherrypy.request.remote.ip)

  @cherrypy.expose
  def ServeConstrained(self, f=None, bandwidth=None, latency=None, loss=None,
                       new_port=False, no_cache=False, **kwargs):
    """Serves the requested file with the requested constraints.

    Subsequent requests for the same constraints from the same IP will share the
    previously created port unless new_port equals True. If no constraints
    are provided the file is served as is.

    Args:
      f: path relative to http root of file to serve.
      bandwidth: maximum allowed bandwidth for the provided port (integer
          in kbit/s).
      latency: time to add to each packet (integer in ms).
      loss: percentage of packets to drop (integer, 0-100).
      new_port: whether to use a new port for this request or not.
      no_cache: Set reponse's cache-control to no-cache.
    """
    if no_cache:
      response = cherrypy.response
      response.headers['Pragma'] = 'no-cache'
      response.headers['Cache-Control'] = 'no-cache'

    # CherryPy is a bit wonky at detecting parameters, so just make them all
    # optional and validate them ourselves.
    if not f:
      raise cherrypy.HTTPError(400, 'Invalid request. File must be specified.')

    # Check existence early to prevent wasted constraint setup.
    self._CheckRequestedFileExist(f)

    # If there are no constraints, just serve the file.
    if bandwidth is None and latency is None and loss is None:
      return self._ServeFile(f)

    constrained_port = self._GetConstrainedPort(
        f, bandwidth=bandwidth, latency=latency, loss=loss, new_port=new_port,
        **kwargs)

    # Build constrained URL using the constrained port and original URL
    # parameters except the network constraints (bandwidth, latency, and loss).
    constrained_url = self._GetServerURL(f, constrained_port,
                                         no_cache=no_cache, **kwargs)

    # Redirect request to the constrained port.
    cherrypy.log('Redirect to %s' % constrained_url)
    cherrypy.lib.cptools.redirect(constrained_url, internal=False)

  def _CheckRequestedFileExist(self, f):
    """Checks if the requested file exists, raises HTTPError otherwise."""
    if self._options.local_server_port:
      self._CheckFileExistOnLocalServer(f)
    else:
      self._CheckFileExistOnServer(f)

  def _CheckFileExistOnServer(self, f):
    """Checks if requested file f exists to be served by this server."""
    # Sanitize and check the path to prevent www-root escapes.
    sanitized_path = os.path.abspath(os.path.join(self._options.www_root, f))
    if not sanitized_path.startswith(self._options.www_root):
      raise cherrypy.HTTPError(403, 'Invalid file requested.')
    if not os.path.exists(sanitized_path):
      raise cherrypy.HTTPError(404, 'File not found.')

  def _CheckFileExistOnLocalServer(self, f):
    """Checks if requested file exists on local server hosting files."""
    test_url = self._GetServerURL(f, self._options.local_server_port)
    try:
      cherrypy.log('Check file exist using URL: %s' % test_url)
      return urllib2.urlopen(test_url) is not None
    except Exception:
      raise cherrypy.HTTPError(404, 'File not found on local server.')

  def _ServeFile(self, f):
    """Serves the file as an http response."""
    if self._options.local_server_port:
      redirect_url = self._GetServerURL(f, self._options.local_server_port)
      cherrypy.log('Redirect to %s' % redirect_url)
      cherrypy.lib.cptools.redirect(redirect_url, internal=False)
    else:
      sanitized_path = os.path.abspath(os.path.join(self._options.www_root, f))
      return cherrypy.lib.static.serve_file(sanitized_path)

  def _GetServerURL(self, f, port, **kwargs):
    """Returns a URL for local server to serve the file on given port.

    Args:
      f: file name to serve on local server. Relative to www_root.
      port: Local server port (it can be a configured constrained port).
      kwargs: extra parameteres passed in the URL.
    """
    url = '%s?f=%s&' % (cherrypy.url(), f)
    if self._options.local_server_port:
      url = '%s/%s?' % (
          cherrypy.url().replace('ServeConstrained', self._options.www_root), f)

    url = url.replace(':%d' % self._options.port, ':%d' % port)
    extra_args = urllib.urlencode(kwargs)
    if extra_args:
      url += extra_args
    return url

  def _GetConstrainedPort(self, f=None, bandwidth=None, latency=None, loss=None,
                          new_port=False, **kwargs):
    """Creates or gets a port with specified network constraints.

    See ServeConstrained() for more details.
    """
    # Validate inputs. isdigit() guarantees a natural number.
    bandwidth = self._ParseIntParameter(
        bandwidth, 'Invalid bandwidth constraint.', lambda x: x > 0)
    latency = self._ParseIntParameter(
        latency, 'Invalid latency constraint.', lambda x: x >= 0)
    loss = self._ParseIntParameter(
        loss, 'Invalid loss constraint.', lambda x: x <= 100 and x >= 0)

    redirect_port = self._options.port
    if self._options.local_server_port:
      redirect_port = self._options.local_server_port

    start_time = time.time()
    # Allocate a port using the given constraints. If a port with the requested
    # key and kwargs already exist then reuse that port.
    constrained_port = self._port_allocator.Get(
        cherrypy.request.remote.ip, server_port=redirect_port,
        interface=self._options.interface, bandwidth=bandwidth, latency=latency,
        loss=loss, new_port=new_port, file=f, **kwargs)

    cherrypy.log('Time to set up port %d = %.3fsec.' %
                 (constrained_port, time.time() - start_time))

    if not constrained_port:
      raise cherrypy.HTTPError(503, 'Service unavailable. Out of ports.')
    return constrained_port

  def _ParseIntParameter(self, param, msg, check):
    """Returns integer value of param and verifies it satisfies the check.

    Args:
      param: Parameter name to check.
      msg: Message in error if raised.
      check: Check to verify the parameter value.

    Returns:
      None if param is None, integer value of param otherwise.

    Raises:
      cherrypy.HTTPError if param can not be converted to integer or if it does
      not satisfy the check.
    """
    if param:
      try:
        int_value = int(param)
        if check(int_value):
          return int_value
      except:
        pass
      raise cherrypy.HTTPError(400, msg)


def ParseArgs():
  """Define and parse the command-line arguments."""
  parser = optparse.OptionParser()

  parser.add_option('--expiry-time', type='int',
                    default=_DEFAULT_PORT_EXPIRY_TIME_SECS,
                    help=('Number of seconds before constrained ports expire '
                          'and are cleaned up. 0=Disabled. Default: %default'))
  parser.add_option('--port', type='int', default=_DEFAULT_SERVING_PORT,
                    help='Port to serve the API on. Default: %default')
  parser.add_option('--port-range', default=_DEFAULT_CNS_PORT_RANGE,
                    help=('Range of ports for constrained serving. Specify as '
                          'a comma separated value pair. Default: %default'))
  parser.add_option('--interface', default='eth0',
                    help=('Interface to setup constraints on. Use lo for a '
                          'local client. Default: %default'))
  parser.add_option('--socket-timeout', type='int',
                    default=cherrypy.server.socket_timeout,
                    help=('Number of seconds before a socket connection times '
                          'out. Default: %default'))
  parser.add_option('--threads', type='int',
                    default=cherrypy._cpserver.Server.thread_pool,
                    help=('Number of threads in the thread pool. Default: '
                          '%default'))
  parser.add_option('--www-root', default='',
                    help=('Directory root to serve files from. If --local-'
                          'server-port is used, the path is appended to the '
                          'redirected URL of local server. Defaults to the '
                          'current directory (if --local-server-port is not '
                          'used): %s' % os.getcwd()))
  parser.add_option('--local-server-port', type='int',
                    help=('Optional local server port to host files.'))
  parser.add_option('-v', '--verbose', action='store_true', default=False,
                    help='Turn on verbose output.')

  options = parser.parse_args()[0]

  # Convert port range into the desired tuple format.
  try:
    if isinstance(options.port_range, str):
      options.port_range = [int(port) for port in options.port_range.split(',')]
  except ValueError:
    parser.error('Invalid port range specified.')

  if options.expiry_time < 0:
    parser.error('Invalid expiry time specified.')

  # Convert the path to an absolute to remove any . or ..
  if not options.local_server_port:
    if not options.www_root:
      options.www_root = os.getcwd()
    options.www_root = os.path.abspath(options.www_root)

  _SetLogger(options.verbose)

  return options


def _SetLogger(verbose):
  file_handler = handlers.RotatingFileHandler('cns.log', 'a', 10000000, 10)
  file_handler.setFormatter(logging.Formatter('[%(threadName)s] %(message)s'))

  log_level = _DEFAULT_LOG_LEVEL
  if verbose:
    log_level = logging.DEBUG
  file_handler.setLevel(log_level)

  cherrypy.log.error_log.addHandler(file_handler)
  cherrypy.log.access_log.addHandler(file_handler)


def Main():
  """Configure and start the ConstrainedNetworkServer."""
  options = ParseArgs()

  try:
    traffic_control.CheckRequirements()
  except traffic_control.TrafficControlError as e:
    cherrypy.log(e.msg)
    return

  cherrypy.config.update({'server.socket_host': '::',
                          'server.socket_port': options.port})

  if options.threads:
    cherrypy.config.update({'server.thread_pool': options.threads})

  if options.socket_timeout:
    cherrypy.config.update({'server.socket_timeout': options.socket_timeout})

  # Setup port allocator here so we can call cleanup on failures/exit.
  pa = PortAllocator(options.port_range, expiry_time_secs=options.expiry_time)

  try:
    cherrypy.quickstart(ConstrainedNetworkServer(options, pa))
  finally:
    # Disable Ctrl-C handler to prevent interruption of cleanup.
    signal.signal(signal.SIGINT, lambda signal, frame: None)
    pa.Cleanup(all_ports=True)


if __name__ == '__main__':
  Main()
