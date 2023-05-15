# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code to allow tests to communicate via a websocket server."""

import asyncio
import logging
import sys
import threading
from typing import Optional

import websockets  # pylint: disable=import-error
import websockets.server as ws_server  # pylint: disable=import-error

WEBSOCKET_PORT_TIMEOUT_SECONDS = 10
WEBSOCKET_SETUP_TIMEOUT_SECONDS = 5

# The client (Chrome) should never be closing the connection. If it does, it's
# indicative of something going wrong like a renderer crash.
ClientClosedConnectionError = websockets.exceptions.ConnectionClosedOK

# Alias for readability and so that users don't have to import asyncio.
WebsocketReceiveMessageTimeoutError = asyncio.TimeoutError


class WebsocketServer():
  def __init__(self):
    """Server that abstracts the asyncio calls used under the hood.

    Only supports one active connection at a time.
    """
    self.server_port = None
    self.server_stopper = None
    self.connection_stopper = None
    self.websocket = None
    self.port_set_event = threading.Event()
    self.connection_received_event = threading.Event()
    self.event_loop = None
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
    if self.connection_stopper:
      self.connection_stopper.cancel()
      try:
        self.connection_stopper.exception()
      except asyncio.CancelledError:
        pass
    self.connection_stopper = None
    self.websocket = None
    self.connection_received_event.clear()

  def WaitForConnection(self, timeout: Optional[int] = None) -> None:
    if self.websocket:
      return
    timeout = timeout or WEBSOCKET_SETUP_TIMEOUT_SECONDS
    self.connection_received_event.wait(timeout)
    if not self.websocket:
      raise RuntimeError('Websocket connection was not established')

  def StopServer(self) -> None:
    self.ClearCurrentConnection()
    if self.server_stopper:
      self.server_stopper.cancel()
      try:
        self.server_stopper.exception()
      except asyncio.CancelledError:
        pass
    self.server_stopper = None
    self.server_port = None

    self._server_thread.join(5)
    if self._server_thread.is_alive():
      logging.error(
          'Websocket server did not shut down properly - this might be '
          'indicative of an issue in the test harness')

  def Send(self, message: str) -> None:
    asyncio.run_coroutine_threadsafe(self.websocket.send(message),
                                     self.event_loop)

  def Receive(self, timeout: int) -> str:
    future = asyncio.run_coroutine_threadsafe(
        asyncio.wait_for(self.websocket.recv(), timeout), self.event_loop)
    try:
      return future.result()
    except asyncio.exceptions.TimeoutError as e:
      raise WebsocketReceiveMessageTimeoutError(
          'Timed out after %d seconds waiting for websocket message' %
          timeout) from e


class _ServerThread(threading.Thread):
  def __init__(self, server_instance: WebsocketServer, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._server_instance = server_instance

  def run(self) -> None:
    try:
      asyncio.run(StartWebsocketServer(self._server_instance))
    except asyncio.CancelledError:
      pass
    except Exception as e:  # pylint: disable=broad-except
      sys.stdout.write('Server thread had an exception: %s\n' % e)


async def StartWebsocketServer(server_instance: WebsocketServer) -> None:
  async def HandleWebsocketConnection(
      websocket: ws_server.WebSocketServerProtocol) -> None:
    # We only allow one active connection - if there are multiple, something is
    # wrong.
    assert server_instance.connection_stopper is None
    assert server_instance.websocket is None
    server_instance.connection_stopper = asyncio.Future()
    # Keep our own reference in case the server clears its reference before the
    # await finishes.
    connection_stopper = server_instance.connection_stopper
    server_instance.websocket = websocket
    server_instance.connection_received_event.set()
    await connection_stopper

  async with websockets.serve(HandleWebsocketConnection,
                              '127.0.0.1',
                              0,
                              ping_interval=None,
                              ping_timeout=None) as server:
    server_instance.event_loop = asyncio.get_running_loop()
    server_instance.server_port = server.sockets[0].getsockname()[1]
    server_instance.port_set_event.set()
    server_instance.server_stopper = asyncio.Future()
    server_stopper = server_instance.server_stopper
    await server_stopper
