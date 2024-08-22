# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code to allow tests to communicate via a websocket server."""

import logging
import threading
from typing import Optional

import websockets  # pylint: disable=import-error
import websockets.sync.server as sync_server  # pylint: disable=import-error

WEBSOCKET_PORT_TIMEOUT_SECONDS = 10
WEBSOCKET_SETUP_TIMEOUT_SECONDS = 5
WEBSOCKET_CLOSE_TIMEOUT_SECONDS = 2
SERVER_SHUTDOWN_TIMEOUT_SECONDS = 5

# The client (Chrome) should never be closing the connection. If it does, it's
# indicative of something going wrong like a renderer crash.
ClientClosedConnectionError = websockets.exceptions.ConnectionClosedOK

# Alias for readability.
WebsocketReceiveMessageTimeoutError = TimeoutError


class WebsocketServer():

  def __init__(self):
    """Server that abstracts the websocket library under the hood.

    Only supports one active connection at a time.
    """
    self.server_port = None
    self.websocket = None
    self.connection_stopper_event = None
    self.connection_closed_event = None
    self.port_set_event = threading.Event()
    self.connection_received_event = threading.Event()
    self._server_thread = None

  def StartServer(self) -> None:
    """Starts the websocket server on a separate thread."""
    assert self._server_thread is None, 'Server already running'
    self._server_thread = _ServerThread(self)
    self._server_thread.daemon = True
    self._server_thread.start()
    got_port = self.port_set_event.wait(WEBSOCKET_PORT_TIMEOUT_SECONDS)
    if not got_port:
      raise RuntimeError('Websocket server did not provide a port')
    # Note: We don't need to set up any port forwarding for remote platforms
    # after this point due to Telemetry's use of --proxy-server to send all
    # traffic through the TsProxyServer. This causes network traffic to pop out
    # on the host, which means that using the websocket server's port directly
    # works.

  def ClearCurrentConnection(self) -> None:
    if self.connection_stopper_event:
      self.connection_stopper_event.set()
      closed = self.connection_closed_event.wait(
          WEBSOCKET_CLOSE_TIMEOUT_SECONDS)
      if not closed:
        raise RuntimeError('Websocket connection did not close')
    self.connection_stopper_event = None
    self.connection_closed_event = None
    self.websocket = None
    self.connection_received_event.clear()

  def WaitForConnection(self, timeout: Optional[float] = None) -> None:
    if self.websocket:
      return
    timeout = timeout or WEBSOCKET_SETUP_TIMEOUT_SECONDS
    self.connection_received_event.wait(timeout)
    if not self.websocket:
      raise RuntimeError('Websocket connection was not established')

  def StopServer(self) -> None:
    self.ClearCurrentConnection()
    self._server_thread.shutdown()
    self._server_thread.join(SERVER_SHUTDOWN_TIMEOUT_SECONDS)
    if self._server_thread.is_alive():
      logging.error(
          'Websocket server did not shut down properly - this might be '
          'indicative of an issue in the test harness')

  def Send(self, message: str) -> None:
    self.websocket.send(message)

  def Receive(self, timeout: int) -> str:
    try:
      return self.websocket.recv(timeout)
    except TimeoutError as e:
      raise WebsocketReceiveMessageTimeoutError(
          'Timed out after %d seconds waiting for websocket message' %
          timeout) from e


class _ServerThread(threading.Thread):
  def __init__(self, server_instance: WebsocketServer, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._server_instance = server_instance
    self.websocket_server = None

  def run(self) -> None:
    StartWebsocketServer(self, self._server_instance)

  def shutdown(self) -> None:
    self.websocket_server.shutdown()


def StartWebsocketServer(server_thread: _ServerThread,
                         server_instance: WebsocketServer) -> None:
  def HandleWebsocketConnection(
      websocket: sync_server.ServerConnection) -> None:
    # We only allow one active connection - if there are multiple, something is
    # wrong.
    assert server_instance.connection_stopper_event is None
    assert server_instance.connection_closed_event is None
    assert server_instance.websocket is None
    server_instance.connection_stopper_event = threading.Event()
    server_instance.connection_closed_event = threading.Event()
    # Keep our own reference in case the server clears its reference before the
    # await finishes.
    connection_stopper_event = server_instance.connection_stopper_event
    connection_closed_event = server_instance.connection_closed_event
    server_instance.websocket = websocket
    server_instance.connection_received_event.set()
    connection_stopper_event.wait()
    connection_closed_event.set()

  with sync_server.serve(HandleWebsocketConnection, '127.0.0.1', 0) as server:
    server_thread.websocket_server = server
    server_instance.server_port = server.socket.getsockname()[1]
    server_instance.port_set_event.set()
    server.serve_forever()
