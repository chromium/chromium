// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The socket properties specified in the <code>create</code> or
// <code>update</code> function. Each property is optional. If a property
// value is not specified, a default value is used when calling
// <code>create</code>, or the existing value if preserved when calling
// <code>update</code>.
dictionary SocketProperties {
  // Flag indicating if the socket is left open when the event page of the
  // application is unloaded (see
  // <a href="http://developer.chrome.com/apps/app_lifecycle.html">Manage App
  // Lifecycle</a>). The default value is "false." When the application is
  // loaded, any sockets previously opened with persistent=true can be fetched
  // with <code>getSockets</code>.
  boolean persistent;

  // An application-defined string associated with the socket.
  DOMString name;

  // The size of the buffer used to receive data. If the buffer is too small
  // to receive the UDP packet, data is lost. The default value is 4096.
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
  // specified explictly, the value is not provided.
  long bufferSize;

  // Flag indicating whether the socket is blocked from firing onReceive
  // events.
  required boolean paused;

  // If the underlying socket is bound, contains its local
  // IPv4/6 address.
  DOMString localAddress;

  // If the underlying socket is bound, contains its local port.
  long localPort;
};

// Data from an <code>onReceive</code> event.
dictionary ReceiveInfo {
  // The socket ID.
  required long socketId;

  // The UDP packet content (truncated to the current buffer size).
  required ArrayBuffer data;

  // The address of the host the packet comes from.
  required DOMString remoteAddress;

  // The port of the host the packet comes from.
  required long remotePort;
};

// Data from an <code>onReceiveError</code> event.
dictionary ReceiveErrorInfo {
  // The socket ID.
  required long socketId;

  // The result code returned from the underlying recvfrom() call.
  required long resultCode;
};

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback BindCallback = undefined (long result);

// |sendInfo|: Result of the <code>send</code> method.
callback SendCallback = undefined (SendInfo sendInfo);

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback JoinGroupCallback = undefined (long result);

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback LeaveGroupCallback = undefined (long result);

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback SetMulticastTimeToLiveCallback = undefined (long result);

// |result|: The result code returned from the underlying network call.
// A negative value indicates an error.
callback SetMulticastLoopbackModeCallback = undefined (long result);

// |result|: The result code returned from the underlying network call.
callback SetBroadcastCallback = undefined (long result);

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

// Use the <code>chrome.sockets.udp</code> API to send and receive data over the
// network using UDP connections. This API supersedes the UDP functionality
// previously found in the "socket" API.
interface Udp {
  // Creates a UDP socket with the given properties.
  // |properties|: The socket properties (optional).
  // |Returns|: Called when the socket has been created.
  // |PromiseValue|: createInfo: The result of the socket creation.
  static Promise<CreateInfo> create(optional SocketProperties properties);

  // Updates the socket properties.
  // |socketId|: The socket ID.
  // |properties|: The properties to update.
  // |Returns|: Called when the properties are updated.
  static Promise<undefined> update(
      long socketId,
      SocketProperties properties);

  // Pauses or unpauses a socket. A paused socket is blocked from firing
  // <code>onReceive</code> events.
  // |connectionId|: The socket ID.
  // |paused|: Flag to indicate whether to pause or unpause.
  // |Returns|: Called when the socket has been successfully paused or
  // unpaused.
  static Promise<undefined> setPaused(long socketId, boolean paused);

  // Binds the local address and port for the socket. For a client socket, it
  // is recommended to use port 0 to let the platform pick a free port.
  //
  // Once the <code>bind</code> operation completes successfully,
  // <code>onReceive</code> events are raised when UDP packets arrive on the
  // address/port specified -- unless the socket is paused.
  //
  // |socketId|: The socket ID.
  // |address|: The address of the local machine. DNS name, IPv4 and IPv6
  // formats are supported. Use "0.0.0.0" to accept packets from all local
  // available network interfaces.
  // |port|: The port of the local machine. Use "0" to bind to a free port.
  // |callback|: Called when the <code>bind</code> operation completes.
  static undefined bind(
      long socketId,
      DOMString address,
      long port,
      BindCallback callback);

  // Sends data on the given socket to the given address and port. The socket
  // must be bound to a local port before calling this method.
  // |socketId|: The socket ID.
  // |data|: The data to send.
  // |address|: The address of the remote machine.
  // |port|: The port of the remote machine.
  // |dnsQueryType|: The address resolution preference.
  // |callback|: Called when the <code>send</code> operation completes.
  static undefined send(
      long socketId,
      ArrayBuffer data,
      DOMString address,
      long port,
      optional DnsQueryType dnsQueryType,
      SendCallback callback);

  // Closes the socket and releases the address/port the socket is bound to.
  // Each socket created should be closed after use. The socket id is no
  // longer valid as soon at the function is called. However, the socket is
  // guaranteed to be closed only when the callback is invoked.
  // |socketId|: The socket ID.
  // |Returns|: Called when the <code>close</code> operation completes.
  static Promise<undefined> close(long socketId);

  // Retrieves the state of the given socket.
  // |socketId|: The socket ID.
  // |Returns|: Called when the socket state is available.
  // |PromiseValue|: socketInfo: Object containing the socket information.
  static Promise<SocketInfo> getInfo(long socketId);

  // Retrieves the list of currently opened sockets owned by the application.
  // |Returns|: Called when the list of sockets is available.
  // |PromiseValue|: socketInfos: Array of object containing socket information.
  static Promise<sequence<SocketInfo>> getSockets();

  // Joins the multicast group and starts to receive packets from that group.
  // The socket must be bound to a local port before calling this method.
  // |socketId|: The socket ID.
  // |address|: The group address to join. Domain names are not supported.
  // |callback|: Called when the <code>joinGroup</code> operation completes.
  static undefined joinGroup(
      long socketId,
      DOMString address,
      JoinGroupCallback callback);

  // Leaves the multicast group previously joined using
  // <code>joinGroup</code>. This is only necessary to call if you plan to
  // keep using the socketafterwards, since it will be done automatically by
  // the OS when the socket is closed.
  //
  // Leaving the group will prevent the router from sending multicast
  // datagrams to the local host, presuming no other process on the host is
  // still joined to the group.
  //
  // |socketId|: The socket ID.
  // |address|: The group address to leave. Domain names are not supported.
  // |callback|: Called when the <code>leaveGroup</code> operation completes.
  static undefined leaveGroup(
      long socketId,
      DOMString address,
      LeaveGroupCallback callback);

  // Sets the time-to-live of multicast packets sent to the multicast group.
  //
  // Calling this method does not require multicast permissions.
  //
  // |socketId|: The socket ID.
  // |ttl|: The time-to-live value.
  // |callback|: Called when the configuration operation completes.
  static undefined setMulticastTimeToLive(
      long socketId,
      long ttl,
      SetMulticastTimeToLiveCallback callback);

  // Sets whether multicast packets sent from the host to the multicast group
  // will be looped back to the host.
  //
  // Note: the behavior of <code>setMulticastLoopbackMode</code> is slightly
  // different between Windows and Unix-like systems. The inconsistency
  // happens only when there is more than one application on the same host
  // joined to the same multicast group while having different settings on
  // multicast loopback mode. On Windows, the applications with loopback off
  // will not RECEIVE the loopback packets; while on Unix-like systems, the
  // applications with loopback off will not SEND the loopback packets to
  // other applications on the same host. See MSDN:
  // https://learn.microsoft.com/en-us/windows/win32/winsock/ip-multicast-2
  //
  // Calling this method does not require multicast permissions.
  //
  // |socketId|: The socket ID.
  // |enabled|: Indicate whether to enable loopback mode.
  // |callback|: Called when the configuration operation completes.
  static undefined setMulticastLoopbackMode(
      long socketId,
      boolean enabled,
      SetMulticastLoopbackModeCallback callback);

  // Gets the multicast group addresses the socket is currently joined to.
  // |socketId|: The socket ID.
  // |Returns|: Called with an array of strings of the result.
  // |PromiseValue|: groups: Array of groups the socket joined.
  static Promise<sequence<DOMString>> getJoinedGroups(long socketId);

  // Enables or disables broadcast packets on this socket.
  //
  // |socketId|: The socket ID.
  // |enabled|: <code>true</code> to enable broadcast packets,
  // <code>false</code> to disable them.
  // |callback|: Callback from the <code>setBroadcast</code> method.
  static undefined setBroadcast(
      long socketId,
      boolean enabled,
      SetBroadcastCallback callback);

  // Event raised when a UDP packet has been received for the given socket.
  static attribute OnReceiveEvent onReceive;

  // Event raised when a network error occured while the runtime was waiting
  // for data on the socket address and port. Once this event is raised, the
  // socket is paused and no more <code>onReceive</code> events will be raised
  // for this socket until the socket is resumed.
  static attribute OnReceiveErrorEvent onReceiveError;
};

partial interface Sockets {
  static attribute Udp udp;
};

partial interface Browser {
  static attribute Sockets sockets;
};
