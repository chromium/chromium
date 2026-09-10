// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The socket properties specified in the <code>create</code> or
// <code>update</code> function. Each property is optional. If a property
// value is not specified, a default value is used when calling
// <code>create</code>, or the existing value if preserved when calling
// <code>update</code>.
dictionary SocketProperties {
  // Flag indicating if the socket remains open when the event page of the
  // application is unloaded (see
  // <a href="http://developer.chrome.com/apps/app_lifecycle.html">Manage App
  // Lifecycle</a>). The default value is "false". When the application is
  // loaded, any sockets previously opened with persistent=true can be fetched
  // with <code>getSockets</code>.
  boolean persistent;

  // An application-defined string associated with the socket.
  DOMString name;
};

// Result of <code>create</code> call.
dictionary CreateInfo {
  // The ID of the newly created server socket. Note that socket IDs created
  // from this API are not compatible with socket IDs created from other APIs,
  // such as the deprecated <code>$(ref:socket)</code> API.
  required long socketId;
};

// Result of the <code>getInfo</code> method.
dictionary SocketInfo {
  // The socket identifier.
  required long socketId;

  // Flag indicating if the socket remains open when the event page of the
  // application is unloaded (see <code>SocketProperties.persistent</code>).
  // The default value is "false".
  required boolean persistent;

  // Application-defined string associated with the socket.
  DOMString name;

  // Flag indicating whether connection requests on a listening socket are
  // dispatched through the <code>onAccept</code> event or queued up in the
  // listen queue backlog.
  // See <code>setPaused</code>. The default value is "false".
  required boolean paused;

  // If the socket is listening, contains its local IPv4/6 address.
  DOMString localAddress;

  // If the socket is listening, contains its local port.
  long localPort;
};

// Data from an <code>onAccept</code> event.
dictionary AcceptInfo {
  // The server socket identifier.
  required long socketId;

  // The client socket identifier, i.e. the socket identifier of the newly
  // established connection. This socket identifier should be used only with
  // functions from the <code>chrome.sockets.tcp</code> namespace. Note the
  // client socket is initially paused and must be explicitly un-paused by the
  // application to start receiving data.
  required long clientSocketId;
};

// Data from an <code>onAcceptError</code> event.
dictionary AcceptErrorInfo {
  // The server socket identifier.
  required long socketId;

  // The result code returned from the underlying network call.
  required long resultCode;
};

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback ListenCallback = undefined (long result);

// |info|: The event data.
callback OnAcceptListener = undefined (AcceptInfo info);

interface OnAcceptEvent : ExtensionEvent {
  static undefined addListener(OnAcceptListener listener);
  static undefined removeListener(OnAcceptListener listener);
  static boolean hasListener(OnAcceptListener listener);
};

// |info|: The event data.
callback OnAcceptErrorListener = undefined (AcceptErrorInfo info);

interface OnAcceptErrorEvent : ExtensionEvent {
  static undefined addListener(OnAcceptErrorListener listener);
  static undefined removeListener(OnAcceptErrorListener listener);
  static boolean hasListener(OnAcceptErrorListener listener);
};

// Use the <code>chrome.sockets.tcpServer</code> API to create server
// applications using TCP connections. This API supersedes the TCP functionality
// previously found in the <code>chrome.socket</code> API.
interface TcpServer {
  // Creates a TCP server socket.
  // |properties|: The socket properties (optional).
  // |Returns|: Called when the socket has been created.
  // |PromiseValue|: createInfo: The result of the socket creation.
  static Promise<CreateInfo> create(optional SocketProperties properties);

  // Updates the socket properties.
  // |socketId|: The socket identifier.
  // |properties|: The properties to update.
  // |Returns|: Called when the properties are updated.
  static Promise<undefined> update(
      long socketId,
      SocketProperties properties);

  // Enables or disables a listening socket from accepting new connections.
  // When paused, a listening socket accepts new connections until its backlog
  // (see <code>listen</code> function) is full then refuses additional
  // connection requests. <code>onAccept</code> events are raised only when
  // the socket is un-paused.
  // |Returns|: Callback from the <code>setPaused</code> method.
  static Promise<undefined> setPaused(long socketId, boolean paused);

  // Listens for connections on the specified port and address.
  // If the port/address is in use, the callback indicates a failure.
  // |socketId|: The socket identifier.
  // |address|: The address of the local machine.
  // |port|: The port of the local machine. When set to <code>0</code>, a
  // free port is chosen dynamically. The dynamically allocated port can be
  // found by calling <code>getInfo</code>.
  // |backlog|: Length of the socket's listen queue. The default value
  // depends on the Operating System (SOMAXCONN), which ensures a reasonable
  // queue length for most applications.
  // |callback|: Called when listen operation completes.
  static undefined listen(
      long socketId,
      DOMString address,
      long port,
      optional long backlog,
      ListenCallback callback);

  // Disconnects the listening socket, i.e. stops accepting new connections
  // and releases the address/port the socket is bound to. The socket
  // identifier remains valid, e.g. it can be used with <code>listen</code> to
  // accept connections on a new port and address.
  // |socketId|: The socket identifier.
  // |Returns|: Called when the disconnect attempt is complete.
  static Promise<undefined> disconnect(long socketId);

  // Disconnects and destroys the socket. Each socket created should be
  // closed after use. The socket id is no longer valid as soon at the
  // function is called. However, the socket is guaranteed to be closed only
  // when the callback is invoked.
  // |socketId|: The socket identifier.
  // |Returns|: Called when the <code>close</code> operation completes.
  static Promise<undefined> close(long socketId);

  // Retrieves the state of the given socket.
  // |socketId|: The socket identifier.
  // |Returns|: Called when the socket state is available.
  // |PromiseValue|: socketInfo: Object containing the socket information.
  static Promise<SocketInfo> getInfo(long socketId);

  // Retrieves the list of currently opened sockets owned by the application.
  // |Returns|: Called when the list of sockets is available.
  // |PromiseValue|: socketInfos: Array of object containing socket information.
  static Promise<sequence<SocketInfo>> getSockets();

  // Event raised when a connection has been made to the server socket.
  static attribute OnAcceptEvent onAccept;

  // Event raised when a network error occurred while the runtime was waiting
  // for new connections on the socket address and port. Once this event is
  // raised, the socket is set to <code>paused</code> and no more
  // <code>onAccept</code> events are raised for this socket until the socket
  // is resumed.
  static attribute OnAcceptErrorEvent onAcceptError;
};

partial interface Sockets {
  static attribute TcpServer tcpServer;
};

partial interface Browser {
  static attribute Sockets sockets;
};
