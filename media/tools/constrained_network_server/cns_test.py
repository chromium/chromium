#!/usr/bin/env vpython
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

"""Tests for Constrained Network Server."""
import os
import signal
import subprocess
import tempfile
import time
import unittest
import urllib2
import cherrypy
import cns
import traffic_control

# The local interface to test on.
_INTERFACE = 'lo'


class PortAllocatorTest(unittest.TestCase):
  """Unit tests for the Port Allocator class."""

  # Expiration time for ports. In mock time.
  _EXPIRY_TIME = 6

  def setUp(self):
    # Mock out time.time() to accelerate port expiration testing.
    self._old_time = time.time
    self._current_time = 0
    time.time = lambda: self._current_time

    # TODO(dalecurtis): Mock out actual calls to shadi's port setup.
    self._pa = cns.PortAllocator(cns._DEFAULT_CNS_PORT_RANGE, self._EXPIRY_TIME)
    self._MockTrafficControl()

  def tearDown(self):
    self._pa.Cleanup(all_ports=True)
    # Ensure ports are cleaned properly.
    self.assertEquals(self._pa._ports, {})
    time.time = self._old_time
    self._RestoreTrafficControl()

  def _MockTrafficControl(self):
    self.old_CreateConstrainedPort = traffic_control.CreateConstrainedPort
    self.old_DeleteConstrainedPort = traffic_control.DeleteConstrainedPort
    self.old_TearDown = traffic_control.TearDown

    traffic_control.CreateConstrainedPort = lambda config: True
    traffic_control.DeleteConstrainedPort = lambda config: True
    traffic_control.TearDown = lambda config: True

  def _RestoreTrafficControl(self):
    traffic_control.CreateConstrainedPort = self.old_CreateConstrainedPort
    traffic_control.DeleteConstrainedPort = self.old_DeleteConstrainedPort
    traffic_control.TearDown = self.old_TearDown

  def testPortAllocator(self):
    # Ensure Get() succeeds and returns the correct port.
    self.assertEquals(self._pa.Get('test'), cns._DEFAULT_CNS_PORT_RANGE[0])

    # Call again with the same key and make sure we get the same port.
    self.assertEquals(self._pa.Get('test'), cns._DEFAULT_CNS_PORT_RANGE[0])

    # Call with a different key and make sure we get a different port.
    self.assertEquals(self._pa.Get('test2'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)

    # Update fake time so that ports should expire.
    self._current_time += self._EXPIRY_TIME + 1

    # Test to make sure cache is checked before expiring ports.
    self.assertEquals(self._pa.Get('test2'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)

    # Update fake time so that ports should expire.
    self._current_time += self._EXPIRY_TIME + 1

    # Request a new port, old ports should be expired, so we should get the
    # first port in the range. Make sure this is the only allocated port.
    self.assertEquals(self._pa.Get('test3'), cns._DEFAULT_CNS_PORT_RANGE[0])
    self.assertEquals(self._pa._ports.keys(), [cns._DEFAULT_CNS_PORT_RANGE[0]])

  def testPortAllocatorExpiresOnlyCorrectPorts(self):
    # Ensure Get() succeeds and returns the correct port.
    self.assertEquals(self._pa.Get('test'), cns._DEFAULT_CNS_PORT_RANGE[0])

    # Stagger port allocation and so we can ensure only ports older than the
    # expiry time are actually expired.
    self._current_time += self._EXPIRY_TIME / 2 + 1

    # Call with a different key and make sure we get a different port.
    self.assertEquals(self._pa.Get('test2'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)

    # After this sleep the port with key 'test' should expire on the next Get().
    self._current_time += self._EXPIRY_TIME / 2 + 1

    # Call with a different key and make sure we get the first port.
    self.assertEquals(self._pa.Get('test3'), cns._DEFAULT_CNS_PORT_RANGE[0])
    self.assertEquals(set(self._pa._ports.keys()), set([
        cns._DEFAULT_CNS_PORT_RANGE[0], cns._DEFAULT_CNS_PORT_RANGE[0] + 1]))

  def testPortAllocatorNoExpiration(self):
    # Setup PortAllocator w/o port expiration.
    self._pa = cns.PortAllocator(cns._DEFAULT_CNS_PORT_RANGE, 0)

    # Ensure Get() succeeds and returns the correct port.
    self.assertEquals(self._pa.Get('test'), cns._DEFAULT_CNS_PORT_RANGE[0])

    # Update fake time to see if ports expire.
    self._current_time += self._EXPIRY_TIME

    # Send second Get() which would normally cause ports to expire. Ensure that
    # the ports did not expire.
    self.assertEquals(self._pa.Get('test2'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)
    self.assertEquals(set(self._pa._ports.keys()), set([
        cns._DEFAULT_CNS_PORT_RANGE[0], cns._DEFAULT_CNS_PORT_RANGE[0] + 1]))

  def testPortAllocatorCleanMatchingIP(self):
    # Setup PortAllocator w/o port expiration.
    self._pa = cns.PortAllocator(cns._DEFAULT_CNS_PORT_RANGE, 0)

    # Ensure Get() succeeds and returns the correct port.
    self.assertEquals(self._pa.Get('ip1', t=1), cns._DEFAULT_CNS_PORT_RANGE[0])
    self.assertEquals(self._pa.Get('ip1', t=2),
                      cns._DEFAULT_CNS_PORT_RANGE[0] + 1)
    self.assertEquals(self._pa.Get('ip1', t=3),
                      cns._DEFAULT_CNS_PORT_RANGE[0] + 2)
    self.assertEquals(self._pa.Get('ip2', t=1),
                      cns._DEFAULT_CNS_PORT_RANGE[0] + 3)

    self._pa.Cleanup(all_ports=False, request_ip='ip1')

    self.assertEquals(self._pa._ports.keys(),
                      [cns._DEFAULT_CNS_PORT_RANGE[0] + 3])
    self.assertEquals(self._pa.Get('ip2'), cns._DEFAULT_CNS_PORT_RANGE[0])
    self.assertEquals(self._pa.Get('ip1'), cns._DEFAULT_CNS_PORT_RANGE[0] + 1)

    self._pa.Cleanup(all_ports=False, request_ip='ip2')
    self.assertEquals(self._pa._ports.keys(),
                      [cns._DEFAULT_CNS_PORT_RANGE[0] + 1])

    self._pa.Cleanup(all_ports=False, request_ip='abc')
    self.assertEquals(self._pa._ports.keys(),
                      [cns._DEFAULT_CNS_PORT_RANGE[0] + 1])

    self._pa.Cleanup(all_ports=False, request_ip='ip1')
    self.assertEquals(self._pa._ports.keys(), [])


class ConstrainedNetworkServerTest(unittest.TestCase):
  """End to end tests for ConstrainedNetworkServer system.

  These tests require root access and run the cherrypy server along with
  tc/iptables commands.
  """

  # Amount of time to wait for the CNS to start up.
  _SERVER_START_SLEEP_SECS = 1

  # Sample data used to verify file serving.
  _TEST_DATA = 'The quick brown fox jumps over the lazy dog'

  # Server information.
  _SERVER_URL = ('http://127.0.0.1:%d/ServeConstrained?' %
                 cns._DEFAULT_SERVING_PORT)

  # Setting for latency testing.
  _LATENCY_TEST_SECS = 1

  def _StartServer(self):
    """Starts the CNS, returns pid."""
    cmd = ['python', 'cns.py', '--interface=%s' % _INTERFACE]
    process = subprocess.Popen(cmd, stderr=subprocess.PIPE)

    # Wait for server to startup.
    line = True
    while line:
      line = process.stderr.readline()
      if 'STARTED' in line:
        return process.pid

    self.fail('Failed to start CNS.')

  def setUp(self):
    # Start the CNS.
    self._server_pid = self._StartServer()

    # Create temp file for serving. Run after server start so if a failure
    # during setUp() occurs we don't leave junk files around.
    f, self._file = tempfile.mkstemp(dir=os.getcwd())
    os.write(f, self._TEST_DATA)
    os.close(f)

    # Strip cwd off so we have a proper relative path.
    self._relative_fn = self._file[len(os.getcwd()) + 1:]

  def tearDown(self):
    os.unlink(self._file)
    os.kill(self._server_pid, signal.SIGTERM)

  def testServerServesFiles(self):
    now = time.time()

    f = urllib2.urlopen('%sf=%s' % (self._SERVER_URL, self._relative_fn))

    # Verify file data is served correctly.
    self.assertEqual(self._TEST_DATA, f.read())

    # For completeness ensure an unconstrained call takes less time than our
    # artificial constraints checked in the tests below.
    self.assertTrue(time.time() - now < self._LATENCY_TEST_SECS)

  def testServerLatencyConstraint(self):
    """Tests serving a file with a latency network constraint."""
    # Abort if does not have root access.
    self.assertEqual(os.geteuid(), 0, 'You need root access to run this test.')
    now = time.time()

    base_url = '%sf=%s' % (self._SERVER_URL, self._relative_fn)
    url = '%s&latency=%d' % (base_url, self._LATENCY_TEST_SECS * 1000)
    f = urllib2.urlopen(url)

    # Verify file data is served correctly.
    self.assertEqual(self._TEST_DATA, f.read())

    # Verify the request took longer than the requested latency.
    self.assertTrue(time.time() - now > self._LATENCY_TEST_SECS)

    # Verify the server properly redirected the URL.
    self.assertTrue(f.geturl().startswith(base_url.replace(
        str(cns._DEFAULT_SERVING_PORT), str(cns._DEFAULT_CNS_PORT_RANGE[0]))))


class ConstrainedNetworkServerUnitTests(unittest.TestCase):
  """ConstrainedNetworkServer class unit tests."""

  def testGetServerURL(self):
    """Test server URL is correct when using Cherrypy port."""
    cns_obj = cns.ConstrainedNetworkServer(self.DummyOptions(), None)

    self.assertEqual(cns_obj._GetServerURL('ab/xz.webm', port=1234, t=1),
                     'http://127.0.0.1:1234/ServeConstrained?f=ab/xz.webm&t=1')

  def testGetServerURLWithLocalServer(self):
    """Test server URL is correct when using --local-server-port port."""
    cns_obj = cns.ConstrainedNetworkServer(self.DummyOptionsWithServer(), None)

    self.assertEqual(cns_obj._GetServerURL('ab/xz.webm', port=1234, t=1),
                     'http://127.0.0.1:1234/media/ab/xz.webm?t=1')

  class DummyOptions(object):
    www_root = 'media'
    port = 9000
    cherrypy.url = lambda: 'http://127.0.0.1:9000/ServeConstrained'
    local_server_port = None

  class DummyOptionsWithServer(object):
    www_root = 'media'
    port = 9000
    cherrypy.url = lambda: 'http://127.0.0.1:9000/ServeConstrained'
    local_server_port = 8080


if __name__ == '__main__':
  unittest.main()
