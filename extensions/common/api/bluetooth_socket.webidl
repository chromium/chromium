// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary SocketProperties {
  // Flag indicating whether the socket is left open when the event page of
  // the application is unloaded (see <a
  // href="http://developer.chrome.com/apps/app_lifecycle.html">Manage App
  // Lifecycle</a>). The default value is <code>false.</code> When the
  // application is loaded, any sockets previously opened with persistent=true
  // can be fetched with $ref:getSockets.
  boolean persistent;

  // An application-defined string associated with the socket.
  DOMString? name;

  // The size of the buffer used to receive data. The default value is 4096.
  long? bufferSize;
};

dictionary CreateInfo {
  // The ID of the newly created socket. Note that socket IDs created
  // from this API are not compatible with socket IDs created from other APIs,
  // such as the <code>$(ref:sockets.tcp)</code> API.
  required long socketId;
};

// Options that may be passed to the <code>listenUsingRfcomm</code> and
// <code>listenUsingL2cap</code> methods. Each property is optional with a
// default being used if not specified.
dictionary ListenOptions {
  // The RFCOMM Channel used by <code>listenUsingRfcomm</code>. If specified,
  // this channel must not be previously in use or the method call will fail.
  // When not specified, an unused channel will be automatically allocated.
  long? channel;

  // The L2CAP PSM used by <code>listenUsingL2cap</code>. If specified, this
  // PSM must not be previously in use or the method call with fail. When
  // not specified, an unused PSM will be automatically allocated.
  long? psm;

  // Length of the socket's listen queue. The default value depends on the
  // operating system's host subsystem.
  long? backlog;
};

dictionary SocketInfo {
  // The socket identifier.
  required long socketId;

  // Flag indicating if the socket remains open when the event page of the
  // application is unloaded (see <code>SocketProperties.persistent</code>).
  // The default value is "false".
  required boolean persistent;

  // Application-defined string associated with the socket.
  DOMString? name;

  // The size of the buffer used to receive data. If no buffer size has been
  // specified explictly, the value is not provided.
  long? bufferSize;

  // Flag indicating whether a connected socket blocks its peer from sending
  // more data, or whether connection requests on a listening socket are
  // dispatched through the <code>onAccept</code> event or queued up in the
  // listen queue backlog.
  // See <code>setPaused</code>. The default value is "false".
  required boolean paused;

  // Flag indicating whether the socket is connected to a remote peer.
  required boolean connected;

  // If the underlying socket is connected, contains the Bluetooth address of
  // the device it is connected to.
  DOMString? address;

  // If the underlying socket is connected, contains information about the
  // service UUID it is connected to, otherwise if the underlying socket is
  // listening, contains information about the service UUID it is listening
  // on.
  DOMString? uuid;
};

dictionary AcceptInfo {
  // The server socket identifier.
  required long socketId;

  // The client socket identifier, i.e. the socket identifier of the newly
  // established connection. This socket identifier should be used only with
  // functions from the <code>chrome.bluetoothSocket</code> namespace. Note
  // the client socket is initially paused and must be explictly un-paused by
  // the application to start receiving data.
  required long clientSocketId;
};

enum AcceptError {
  // A system error occurred and the connection may be unrecoverable.
  "system_error",

  // The socket is not listening.
  "not_listening"
};

dictionary AcceptErrorInfo {
  // The server socket identifier.
  required long socketId;

  // The error message.
  required DOMString errorMessage;

  // An error code indicating what went wrong.
  required AcceptError error;
};

dictionary ReceiveInfo {
  // The socket identifier.
  required long socketId;

  // The data received, with a maxium size of <code>bufferSize</code>.
  required ArrayBuffer data;
};

enum ReceiveError {
  // The connection was disconnected.
  "disconnected",

  // A system error occurred and the connection may be unrecoverable.
  "system_error",

  // The socket has not been connected.
  "not_connected"
};

dictionary ReceiveErrorInfo {
  // The socket identifier.
  required long socketId;

  // The error message.
  required DOMString errorMessage;

  // An error code indicating what went wrong.
  required ReceiveError error;
};

// |info| : The event data.
callback OnAcceptListener = undefined (AcceptInfo info);

interface OnAcceptEvent : ExtensionEvent {
  static undefined addListener(OnAcceptListener listener);
  static undefined removeListener(OnAcceptListener listener);
  static boolean hasListener(OnAcceptListener listener);
};

// |info| : The event data.
callback OnAcceptErrorListener = undefined (AcceptErrorInfo info);

interface OnAcceptErrorEvent : ExtensionEvent {
  static undefined addListener(OnAcceptErrorListener listener);
  static undefined removeListener(OnAcceptErrorListener listener);
  static boolean hasListener(OnAcceptErrorListener listener);
};

// |info| : The event data.
callback OnReceiveListener = undefined (ReceiveInfo info);

interface OnReceiveEvent : ExtensionEvent {
  static undefined addListener(OnReceiveListener listener);
  static undefined removeListener(OnReceiveListener listener);
  static boolean hasListener(OnReceiveListener listener);
};

// |info| : The event data.
callback OnReceiveErrorListener = undefined (ReceiveErrorInfo info);

interface OnReceiveErrorEvent : ExtensionEvent {
  static undefined addListener(OnReceiveErrorListener listener);
  static undefined removeListener(OnReceiveErrorListener listener);
  static boolean hasListener(OnReceiveErrorListener listener);
};

// Use the <code>chrome.bluetoothSocket</code> API to send and receive data
// to Bluetooth devices using RFCOMM and L2CAP connections.
interface BluetoothSocket {
  // Creates a Bluetooth socket.
  // |properties| : The socket properties (optional).
  // |Returns|: Called when the socket has been created.
  // |PromiseValue|: createInfo: The result of the socket creation.
  [requiredCallback] static Promise<CreateInfo> create(
      optional SocketProperties properties);

  // Updates the socket properties.
  // |socketId| : The socket identifier.
  // |properties| : The properties to update.
  // |Returns|: Called when the properties are updated.
  static Promise<undefined> update(
      long socketId,
      SocketProperties properties);

  // Enables or disables a connected socket from receiving messages from its
  // peer, or a listening socket from accepting new connections. The default
  // value is "false". Pausing a connected socket is typically used by an
  // application to throttle data sent by its peer. When a connected socket
  // is paused, no <code>onReceive</code>event is raised. When a socket is
  // connected and un-paused, <code>onReceive</code> events are raised again
  // when messages are received. When a listening socket is paused, new
  // connections are accepted until its backlog is full then additional
  // connection requests are refused. <code>onAccept</code> events are raised
  // only when the socket is un-paused.
  // |Returns|: Callback from the <code>setPaused</code> method.
  static Promise<undefined> setPaused(
      long socketId,
      boolean paused);

  // Listen for connections using the RFCOMM protocol.
  // |socketId| : The socket identifier.
  // |uuid| : Service UUID to listen on.
  // |options| : Optional additional options for the service.
  // |Returns|: Called when listen operation completes.
  [requiredCallback] static Promise<undefined> listenUsingRfcomm(
      long socketId,
      DOMString uuid,
      optional ListenOptions options);

  // Listen for connections using the L2CAP protocol.
  // |socketId| : The socket identifier.
  // |uuid| : Service UUID to listen on.
  // |options| : Optional additional options for the service.
  // |Returns|: Called when listen operation completes.
  [requiredCallback] static Promise<undefined> listenUsingL2cap(
      long socketId,
      DOMString uuid,
      optional ListenOptions options);

  // Connects the socket to a remote Bluetooth device. When the
  // <code>connect</code> operation completes successfully,
  // <code>onReceive</code> events are raised when data is received from the
  // peer. If a network error occur while the runtime is receiving packets,
  // a <code>onReceiveError</code> event is raised, at which point no more
  // <code>onReceive</code> event will be raised for this socket until the
  // <code>setPaused(false)</code> method is called.
  // |socketId| : The socket identifier.
  // |address| : The address of the Bluetooth device.
  // |uuid| : The UUID of the service to connect to.
  // |Returns|: Called when the connect attempt is complete.
  [requiredCallback] static Promise<undefined> connect(
      long socketId,
      DOMString address,
      DOMString uuid);

  // Disconnects the socket. The socket identifier remains valid.
  // |socketId| : The socket identifier.
  // |Returns|: Called when the disconnect attempt is complete.
  static Promise<undefined> disconnect(
      long socketId);

  // Disconnects and destroys the socket. Each socket created should be
  // closed after use. The socket id is no longer valid as soon at the
  // function is called. However, the socket is guaranteed to be closed only
  // when the callback is invoked.
  // |socketId| : The socket identifier.
  // |Returns|: Called when the <code>close</code> operation completes.
  static Promise<undefined> close(
      long socketId);

  // Sends data on the given Bluetooth socket.
  // |socketId| : The socket identifier.
  // |data| : The data to send.
  // |Returns|: Called with the number of bytes sent.
  // |PromiseValue|: bytesSent: The number of bytes sent.
  static Promise<long> send(
      long socketId,
      ArrayBuffer data);

  // Retrieves the state of the given socket.
  // |socketId| : The socket identifier.
  // |Returns|: Called when the socket state is available.
  // |PromiseValue|: socketInfo: Object containing the socket information.
  [requiredCallback] static Promise<SocketInfo> getInfo(
      long socketId);

  // Retrieves the list of currently opened sockets owned by the application.
  // |Returns|: Called when the list of sockets is available.
  // |PromiseValue|: sockets: Array of object containing socket information.
  [requiredCallback] static Promise<sequence<SocketInfo>> getSockets();

  // Event raised when a connection has been established for a given socket.
  static attribute OnAcceptEvent onAccept;

  // Event raised when a network error occurred while the runtime was waiting
  // for new connections on the given socket. Once this event is raised, the
  // socket is set to <code>paused</code> and no more <code>onAccept</code>
  // events are raised for this socket.
  static attribute OnAcceptErrorEvent onAcceptError;

  // Event raised when data has been received for a given socket.
  static attribute OnReceiveEvent onReceive;

  // Event raised when a network error occured while the runtime was waiting
  // for data on the socket. Once this event is raised, the socket is set to
  // <code>paused</code> and no more <code>onReceive</code> events are raised
  // for this socket.
  static attribute OnReceiveErrorEvent onReceiveError;
};

partial interface Browser {
  static attribute BluetoothSocket bluetoothSocket;
};
