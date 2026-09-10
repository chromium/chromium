// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The socket properties specified in the <code>create</code> or
// <code>update</code> function. Each property is optional. If a property
// value is not specified, a default value is used when calling
// <code>create</code>, or the existing value if preserved when calling
// <code>update</code>.
dictionary SocketProperties {
  // Flag indicating if the socket is left open when the event page of
  // the application is unloaded (see
  // <a href="http://developer.chrome.com/apps/app_lifecycle.html">Manage App
  // Lifecycle</a>). The default value is "false". When the application is
  // loaded, any sockets previously opened with persistent=true can be fetched
  // with <code>getSockets</code>.
  boolean persistent;

  // An application-defined string associated with the socket.
  DOMString name;

  // The size of the buffer used to receive data. The default value is 4096.
  long bufferSize;
};

// Result of <code>create</code> call.
dictionary CreateInfo {
  // The ID of the newly created socket. Note that socket IDs created from
  // this API are not compatible with socket IDs created from other APIs, such
  // as the deprecated <code>$(ref:socket)</code> API.
  required long socketId;
};

// DNS resolution preferences. The default is <code>any</code> and uses the
// current OS config which may return IPv4 or IPv6. <code>ipv4</code> forces
// IPv4, and <code>ipv6</code> forces IPv6.
enum DnsQueryType {
  "any",
  "ipv4",
  "ipv6"
};

// Result of the <code>send</code> method.
dictionary SendInfo {
  // The result code returned from the underlying network call.
  // A negative value indicates an error.
  required long resultCode;

  // The number of bytes sent (if result == 0)
  long bytesSent;
};

dictionary TLSVersionConstraints {
  // The minimum and maximum acceptable versions of TLS. Supported values are
  // <code>tls1.2</code> or <code>tls1.3</code>.
  //
  // The values <code>tls1</code> and <code>tls1.1</code> are no longer
  // supported. If |min| is set to one of these values, it will be silently
  // clamped to <code>tls1.2</code>. If |max| is set to one of those values,
  // or any other unrecognized value, it will be silently ignored.
  DOMString min;
  DOMString max;
};

dictionary SecureOptions {
  TLSVersionConstraints tlsVersion;
};

// Result of the <code>getInfo</code> method.
dictionary SocketInfo {
  // The socket identifier.
  required long socketId;

  // Flag indicating whether the socket is left open when the application is
  // suspended (see <code>SocketProperties.persistent</code>).
  required boolean persistent;

  // Application-defined string associated with the socket.
  DOMString name;

  // The size of the buffer used to receive data. If no buffer size has been
  // specified explicitly, the value is not provided.
  long bufferSize;

  // Flag indicating whether a connected socket blocks its peer from sending
  // more data (see <code>setPaused</code>).
  required boolean paused;

  // Flag indicating whether the socket is connected to a remote peer.
  required boolean connected;

  // If the underlying socket is connected, contains its local IPv4/6 address.
  DOMString localAddress;

  // If the underlying socket is connected, contains its local port.
  long localPort;

  // If the underlying socket is connected, contains the peer/ IPv4/6 address.
  DOMString peerAddress;

  // If the underlying socket is connected, contains the peer port.
  long peerPort;
};

// Data from an <code>onReceive</code> event.
dictionary ReceiveInfo {
  // The socket identifier.
  required long socketId;

  // The data received, with a maxium size of <code>bufferSize</code>.
  required ArrayBuffer data;
};

// Data from an <code>onReceiveError</code> event.
dictionary ReceiveErrorInfo {
  // The socket identifier.
  required long socketId;

  // The result code returned from the underlying network call.
  required long resultCode;
};

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback ConnectCallback = undefined (long result);

// |sendInfo|: Result of the <code>send</code> method.
callback SendCallback = undefined (SendInfo sendInfo);

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback SetKeepAliveCallback = undefined (long result);

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback SetNoDelayCallback = undefined (long result);

callback SecureCallback = undefined (long result);

// |info|: The event data.
callback OnReceiveListener = undefined (ReceiveInfo info);

interface OnReceiveEvent : ExtensionEvent {
  static undefined addListener(OnReceiveListener listener);
  static undefined removeListener(OnReceiveListener listener);
  static boolean hasListener(OnReceiveListener listener);
};

// |info|: The event data.
callback OnReceiveErrorListener = undefined (ReceiveErrorInfo info);

interface OnReceiveErrorEvent : ExtensionEvent {
  static undefined addListener(OnReceiveErrorListener listener);
  static undefined removeListener(OnReceiveErrorListener listener);
  static boolean hasListener(OnReceiveErrorListener listener);
};

// Use the <code>chrome.sockets.tcp</code> API to send and receive data over the
// network using TCP connections. This API supersedes the TCP functionality
// previously found in the <code>chrome.socket</code> API.
interface Tcp {
  // Creates a TCP socket.
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

  // Enables or disables the application from receiving messages from its
  // peer. The default value is "false". Pausing a socket is typically used
  // by an application to throttle data sent by its peer. When a socket is
  // paused, no <code>onReceive</code> event is raised. When a socket is
  // connected and un-paused, <code>onReceive</code> events are raised again
  // when messages are received.
  // |Returns|: Callback from the <code>setPaused</code> method.
  static Promise<undefined> setPaused(long socketId, boolean paused);

  // Enables or disables the keep-alive functionality for a TCP connection.
  // |socketId|: The socket identifier.
  // |enable|: If true, enable keep-alive functionality.
  // |delay|: Set the delay seconds between the last data packet received
  // and the first keepalive probe. Default is 0.
  // |callback|: Called when the setKeepAlive attempt is complete.
  static undefined setKeepAlive(
      long socketId,
      boolean enable,
      optional long delay,
      SetKeepAliveCallback callback);

  // Sets or clears <code>TCP_NODELAY</code> for a TCP connection. Nagle's
  // algorithm will be disabled when <code>TCP_NODELAY</code> is set.
  // |socketId|: The socket identifier.
  // |noDelay|: If true, disables Nagle's algorithm.
  // |callback|: Called when the setNoDelay attempt is complete.
  static undefined setNoDelay(
      long socketId,
      boolean noDelay,
      SetNoDelayCallback callback);

  // Connects the socket to a remote machine. When the <code>connect</code>
  // operation completes successfully, <code>onReceive</code> events are
  // raised when data is received from the peer. If a network error occurs
  // while the runtime is receiving packets, a <code>onReceiveError</code>
  // event is raised, at which point no more <code>onReceive</code> event will
  // be raised for this socket until the <code>resume</code> method is called.
  // |socketId|: The socket identifier.
  // |peerAddress|: The address of the remote machine. DNS name, IPv4 and
  //  IPv6 formats are supported.
  // |peerPort|: The port of the remote machine.
  // |dnsQueryType|: The address resolution preference.
  // |callback|: Called when the connect attempt is complete.
  static undefined connect(
      long socketId,
      DOMString peerAddress,
      long peerPort,
      optional DnsQueryType dnsQueryType,
      ConnectCallback callback);

  // Disconnects the socket.
  // |socketId|: The socket identifier.
  // |Returns|: Called when the disconnect attempt is complete.
  static Promise<undefined> disconnect(long socketId);

  // Start a TLS client connection over the connected TCP client socket.
  // |socketId|: The existing, connected socket to use.
  // |options|: Constraints and parameters for the TLS connection.
  // |callback|: Called when the connection attempt is complete.
  static undefined secure(
      long socketId,
      optional SecureOptions options,
      SecureCallback callback);

  // Sends data on the given TCP socket.
  // |socketId|: The socket identifier.
  // |data|: The data to send.
  // |callback|: Called when the <code>send</code> operation completes.
  static undefined send(
      long socketId,
      ArrayBuffer data,
      SendCallback callback);

  // Closes the socket and releases the address/port the socket is bound to.
  // Each socket created should be closed after use. The socket id is no
  // no longer valid as soon at the function is called. However, the socket is
  // guaranteed to be closed only when the callback is invoked.
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

  // Event raised when data has been received for a given socket.
  static attribute OnReceiveEvent onReceive;

  // Event raised when a network error occurred while the runtime was waiting
  // for data on the socket address and port. Once this event is raised, the
  // socket is set to <code>paused</code> and no more <code>onReceive</code>
  // events are raised for this socket.
  static attribute OnReceiveErrorEvent onReceiveError;
};

partial interface Sockets {
  static attribute Tcp tcp;
};

partial interface Browser {
  static attribute Sockets sockets;
};
