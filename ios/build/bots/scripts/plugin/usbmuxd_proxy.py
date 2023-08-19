# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import grpc
import logging
import os
import select
import signal
import socket
import subprocess
import sys
import threading
import time

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import PLUGIN_PROTOS_PATH, PLUGIN_SERVICE_ADDRESS, PLUGIN_PROXY_SERVICE_PORT, REMOTE_PLUGIN_PROXY_PORT
from test_plugin_client import TestPluginClient

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc

LOGGER = logging.getLogger(__name__)


class PluginServiceProxyWrapper:

  def __init__(self, plugin_service_address, plugin_proxy_service_port,
               remote_proxy_port):
    """ Wrapper for ptroxy service that handles usbmuxd requests/response

    Args:
      plugin_service_address: address for the plugin service in
        test_plugin_service.py.
      plugin_proxy_service_port: port for the proxy service
      remote_proxy_port: port for the proxy service on the test app side.

    """
    self.plugin_service_address = plugin_service_address
    self.plugin_proxy_service_port = plugin_proxy_service_port
    self.remote_proxy_port = remote_proxy_port
    self.proxy_process_stop_flag = threading.Event()
    self.plugin_service_proxy = self.PluginServiceProxy(
        plugin_service_address, plugin_proxy_service_port,
        self.proxy_process_stop_flag)

    # iproxy is the built-in service that forwards/receives data through usbmuxd
    self.iproxy_process = None

    # proxy process will handle the data and communicate with local
    # plugin service
    self.proxy_process = None

  def start(self):
    self.iproxy_process = self.start_iproxy()

    self.proxy_process = threading.Thread(
        target=self.plugin_service_proxy.start)
    self.proxy_process.start()

  def tear_down(self):
    LOGGER.info('terminating proxy process...')
    if self.proxy_process != None:
      self.proxy_process_stop_flag.set()
      self.proxy_process.join()
      self.proxy_process_stop_flag.clear()

    LOGGER.info('terminating iproxy process...')
    if self.iproxy_process != None:
      os.kill(self.iproxy_process.pid, signal.SIGTERM)

  def reset(self):
    # re-establish remote connection with the test app. This is usually called
    # during test retries, when the remote proxy service re-launched and we
    # need to re-establish the connection.
    if self.proxy_process != None:
      self.proxy_process_stop_flag.set()
      self.proxy_process.join()
      self.proxy_process_stop_flag.clear()

    self.proxy_process = threading.Thread(
        target=self.plugin_service_proxy.start)
    self.proxy_process.start()

  def start_iproxy(self):
    # starts iproxy process for port forwarding.
    cmd = ['iproxy', self.plugin_proxy_service_port, self.remote_proxy_port]
    process = subprocess.Popen(cmd)
    # iproxy does not start right away when the command is called.
    # We should read the output of the command to determine if the service
    # is up. Temporary using time.sleep for workaround.
    time.sleep(2)
    return process

  class PluginServiceProxy:

    def __init__(self, plugin_service_address, plugin_proxy_service_port,
                 stop_flag):
      """ Proxy service that handles usbmuxd requests/response.

      The service is responsible for forwarding data received from usbmuxd
      to plugin service. It also forwards data received from plugin service
      back to the remote usbmuxd proxy on the test app side.

      Args:
        plugin_service_address: address for the plugin service in
          test_plugin_service.py.
        plugin_proxy_service_port: port for the proxy service.
        stop_flag: since the service will be continuously running in the
          background to handle requests async. We need a thread flag to
          stop the service process.

      """
      self.plugin_client = TestPluginClient(plugin_service_address)
      self.plugin_proxy_service_port = plugin_proxy_service_port
      self.stop_flag = stop_flag

    def start(self):
      sock = None
      try:
        LOGGER.info(
            'Attemping to establish connection with remote proxy service...')
        received = ""
        # While loop to connect to remote proxy service
        # receiving a response means the connect is successful
        while received == "" and not self.stop_flag.is_set():
          sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
          sock.connect(('localhost', int(self.plugin_proxy_service_port)))
          try:
            # Just a placeholder below. Refactor this for proper handshake
            # once we implement test app side proxy.
            sock.sendall(bytes("hello world" + "\n", "utf-8"))
            received = str(sock.recv(1024), "utf-8")
          except ConnectionResetError as e:
            LOGGER.error(
                'unable to connect to remote device server, retrying...', e)
            sock.close()
            sock = None
            # wait for 3 seconds before next retry
            time.sleep(3)
        LOGGER.info(
            'Connection with remote proxy service is successfully established!')

        # As long as the thread is not killed (stop flag is set),
        # the proxy service will run continuously to handle requests.
        while not self.stop_flag.is_set():
          self.receiveRequests()
          self.forwardRequests()
      except Exception as e:
        LOGGER.error('Proxy service unexpectedly exited due to error ', e)
      finally:
        if sock != None:
          sock.close()

    def receiveRequests(self):
      # receive requests from the remote port, and massage into grpc request
      return

    def forwardRequests(self):
      # forwards grpc requests to the service client, and respond back
      # to the remote port
      return


# for testing purpose only when running locally
if __name__ == '__main__':
  server = PluginServiceProxyWrapper(PLUGIN_SERVICE_ADDRESS,
                                     PLUGIN_PROXY_SERVICE_PORT,
                                     REMOTE_PLUGIN_PROXY_PORT)
  server.start()
