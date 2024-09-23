// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: No header guards are used, since this file is intended to be expanded
// directly into net_log.h. DO NOT include this file anywhere else.
// The following line silences a presubmit warning that would otherwise be
// triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

// In the event of a failure, many end events will have a |net_error|
// parameter with the integer error code associated with the failure.  Most
// of these parameters are not individually documented.
//
// For best practices on how to add new NetLog events see:
// https://chromium.googlesource.com/chromium/src/+/HEAD/net/docs/net-log.md

// --------------------------------------------------------------------------
// General pseudo-events
// --------------------------------------------------------------------------

// Something got cancelled (we determine what is cancelled based on the
// log context around it.)
EVENT_TYPE(CANCELLED)

// Something failed (we determine what failed based on the log context
// around it.)
// The event has the following parameters:
//
//   {
//     "net_error": <The net error code integer for the failure>,
//   }
EVENT_TYPE(FAILED)

// Marks the creation/destruction of a request (net::URLRequest).
EVENT_TYPE(REQUEST_ALIVE)

// ------------------------------------------------------------------------
// HostResolverManager (previously known as HostResolverImpl)
// ------------------------------------------------------------------------

// The start/end of a host resolve (DNS) request.  Note that these events are
// logged for all DNS requests, though not all requests result in the creation
// of a HostResolvedImpl::Request object.
//
// The BEGIN phase contains the following parameters:
//
//   {
//     "host": <Serialized scheme/host/port of the request>,
//     "dns_query_type": <The type of the DNS query>,
//     "allow_cached_response": <Whether it is ok to return a result from
//                               the host cache>,
//     "is_speculative": <Whether this request was started by the DNS
//                        prefetcher>,
//     "network_anonymization_key": <NetworkAnonymizationKey associated with the
//                                   request>,
//     "secure_dns_policy": <SecureDnsPolicy of the request>,
//   }
//
// If an error occurred, the END phase will contain these parameters:
//   {
//     "net_error": <The net error code integer for the failure>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_REQUEST)

// This event is created (in a source of the same name) when the host resolver
// creates a UDP socket to check for global IPv6 connectivity.
// It contains the following parameter:
//
//   {
//     "ipv6_available": <True if the probe indicates ipv6 connectivity>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_IPV6_REACHABILITY_CHECK)

// This event is logged when a request is handled by a cache entry.
// It contains the following parameter:
//   {
//     "results": <HostCache::Entry of results>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_CACHE_HIT)

// This event is logged when a request is handled by a HOSTS entry.
// It contains the following parameter:
//   {
//     "results": <HostCache::Entry of results>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_HOSTS_HIT)

// This event is logged when a bootstrap request matches a config preset entry.
// It contains the following parameter:
//   {
//     "results": <HostCache::Entry of results>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_CONFIG_PRESET_MATCH)

// This event is created when a new HostResolverManager::Job is about to be
// created for a request.
EVENT_TYPE(HOST_RESOLVER_MANAGER_CREATE_JOB)

// The creation/completion of a HostResolverManager::Job which is created for
// Requests that cannot be resolved synchronously.
//
// The BEGIN phase contains the following parameters:
//
//   {
//     "dns_query_type": <DnsQueryType of the job>,
//     "host": <Serialized scheme/host/port associated with the job>,
//     "network_anonymization_key": <NetworkAnonymizationKey associated with the
//                                   job>,
//     "secure_dns_mode": <SecureDnsMode of the job>,
//     "source_dependency": <Source id, if any, of what created the job>,
//   }
//
// The END phase will contain these parameters:
//   {
//     "net_error": <The net error code integer for the failure>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_JOB)

// This event is created when a HostResolverManager::Job is evicted from
// PrioritizedDispatcher before it can start, because the limit on number of
// queued Jobs was reached.
EVENT_TYPE(HOST_RESOLVER_MANAGER_JOB_EVICTED)

// This event is created when a HostResolverManager::Job is started by
// PrioritizedDispatcher.
EVENT_TYPE(HOST_RESOLVER_MANAGER_JOB_STARTED)

// This event is created when HostResolverManager::ProcTask is about to start a
// new attempt to resolve the host.
//
// The ATTEMPT_STARTED event has the parameters:
//
//   {
//     "attempt_number": <the number of the attempt that is resolving the host>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_ATTEMPT_STARTED)

// This event is created when HostResolverManager::ProcTask has finished
// resolving the host.
//
// The ATTEMPT_FINISHED event has the parameters:
//
//   {
//     "attempt_number": <the number of the attempt that has resolved the host>,
//   }
// If an error occurred, the END phase will contain these additional parameters:
//   {
//     "net_error": <The net error code integer for the failure>,
//     "os_error": <The exact error code integer that getaddrinfo() returned>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_ATTEMPT_FINISHED)

// This is logged for a request when it's attached to a
// HostResolverManager::Job. When this occurs without a preceding
// HOST_RESOLVER_MANAGER_CREATE_JOB entry, it means the request was attached to
// an existing HostResolverManager::Job.
//
// The event contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the attached Job>,
//   }
//
EVENT_TYPE(HOST_RESOLVER_MANAGER_JOB_ATTACH)

// This event is logged for the job to which the request is attached.
// In that case, the event contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the attached Request>,
//     "priority": <New priority of the job>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_JOB_REQUEST_ATTACH)

// This is logged for a job when a request is cancelled and detached.
//
// The event contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the detached Request>,
//     "priority": <New priority of the job>,
//   }
EVENT_TYPE(HOST_RESOLVER_MANAGER_JOB_REQUEST_DETACH)

// The creation/completion of a HostResolverSystemTask to call getaddrinfo. The
// BEGIN phase contains the following parameters:
//
//   {
//     "hostname": <Hostname associated with the request>,
//   }
//
// On success, the END phase has these parameters:
//   {
//     "address_list": <The resolved addresses>,
//   }
// If an error occurred, the END phase will contain these parameters:
//   {
//     "net_error": <The net error code integer for the failure>,
//     "os_error": <The exact error code integer that getaddrinfo() returned>,
//   }
EVENT_TYPE(HOST_RESOLVER_SYSTEM_TASK)

// The creation/completion of a HostResolverDnsTask to manage a DnsTransaction.
// The BEGIN phase contains the following parameters:
//
//   {
//     "secure": <Whether or not the task will use secure DNS>,
//     "transactions_needed": [{
//       "dns_query_type": <DnsQueryType for which a transaction is expected>,
//     }],
//   }
//
// On success, the END phase has these parameters:
//   {
//     "results": <HostCache::Entry of results>,
//   }
// If an error occurred, the END phase will contain these parameters:
//   {
//     "dns_query_type": <The DnsQueryType of the transaction that failed>,
//     "error_ttl_sec": <TTL in seconds for caching the error>,
//     "net_error": <The net error code integer for the failure>,
//     "saved_results": <HostCache::Entry of any previously completed
//                       transactions>,
//   }
EVENT_TYPE(HOST_RESOLVER_DNS_TASK)

// Logged when DnsResponseResultExtractor returns an error to
// HostResolverDnsTask when attempting to extract results from a DnsResponse.
// Contains the following parameters:
//
//   {
//     "extraction_error": <The DnsResponseResultExtractor::ExtractionError>
//     "dns_query_type": <The DnsQueryType requested from the extractor>
//   }
EVENT_TYPE(HOST_RESOLVER_DNS_TASK_EXTRACTION_FAILURE)

// Logged when a HostResolverDnsTask extracted HostResolverInternalResults from
// a DnsResponse. Contains the following parameters:
//
//   {
//     "results": <List of HostResolverInternalResult>,
//   }
EVENT_TYPE(HOST_RESOLVER_DNS_TASK_EXTRACTION_RESULTS)

// Logged when a HostResolverDnsTask times out and cancels unfinished
// transactions.
// The event contains the following parameters:
//
//   {
//     "started_transactions": [{
//       "dns_query_type": <DnsQueryType of a started but incomplete
//       transaction>,
//     }],
//     "queued_transactions": [{
//       "dns_query_type": <DnsQueryType of a not-yet-started transaction>,
//     }],
//   }
EVENT_TYPE(HOST_RESOLVER_DNS_TASK_TIMEOUT)

// Logged when a DnsTaskResultsManager updates intermediate service
// endpoints. Contains the following parameters:
//
//   {
//      "endpoints": [{
//        "ipv4_endpoints": <List of IPv4 endpoint>,
//        "ipv6_endpoints": <List of IPv6 endpoint>,
//        "metadata": <ConnectionEndpointMetadata of this service endpoint>
//      }]
//   }
EVENT_TYPE(HOST_RESOLVER_SERVICE_ENDPOINTS_UPDATED)

// Logged when a DnsTaskResultsManager's resolution timer is timed out,
// or AAAA response is received before the timer timed out.
//   {
//      "timedout": <Whether or not the timer is timed out>,
//      "elapsed": <Elapsed time in milliseconds>
//   }
EVENT_TYPE(HOST_RESOLVER_SERVICE_ENDPOINTS_RESOLUTION_DELAY)

// ------------------------------------------------------------------------
// InitProxyResolver
// ------------------------------------------------------------------------

// The start/end of auto-detect + custom PAC URL configuration.
EVENT_TYPE(PAC_FILE_DECIDER)

// The start/end of when proxy autoconfig was artificially paused following
// a network change event. (We wait some amount of time after being told of
// network changes to avoid hitting spurious errors during auto-detect).
EVENT_TYPE(PAC_FILE_DECIDER_WAIT)

// The start/end of download of a PAC script. This could be the well-known
// WPAD URL (if testing auto-detect), or a custom PAC URL.
//
// The START event has the parameters:
//   {
//     "source": <String describing where PAC script comes from>,
//   }
//
// If the fetch failed, then the END phase has these parameters:
//   {
//      "net_error": <Net error code integer>,
//   }
EVENT_TYPE(PAC_FILE_DECIDER_FETCH_PAC_SCRIPT)

// This event means that initialization failed because there was no
// configured script fetcher. (This indicates a configuration error).
EVENT_TYPE(PAC_FILE_DECIDER_HAS_NO_FETCHER)

// This event is emitted after deciding to fall-back to the next source
// of PAC scripts in the list.
EVENT_TYPE(PAC_FILE_DECIDER_FALLING_BACK_TO_NEXT_PAC_SOURCE)

// ------------------------------------------------------------------------
// ConfiguredProxyResolutionService
// ------------------------------------------------------------------------

// The start/end of a proxy resolve request.
EVENT_TYPE(PROXY_RESOLUTION_SERVICE)

// The time while a request is waiting on InitProxyResolver to configure
// against either WPAD or custom PAC URL. The specifics on this time
// are found from ConfiguredProxyResolutionService::init_proxy_resolver_log().
EVENT_TYPE(PROXY_RESOLUTION_SERVICE_WAITING_FOR_INIT_PAC)

// This event is emitted to show what the PAC script returned. It can contain
// extra parameters that are either:
//   {
//      "pac_string": <List of valid proxy servers, in PAC format>,
//   }
//
//  Or if the the resolver failed:
//   {
//      "net_error": <Net error code that resolver failed with>,
//   }
EVENT_TYPE(PROXY_RESOLUTION_SERVICE_RESOLVED_PROXY_LIST)

// This event is emitted after proxies marked as bad have been deprioritized.
//
// It contains these parameters:
//   {
//      "pac_string": <List of valid proxy servers, in PAC format>,
//   }
EVENT_TYPE(PROXY_RESOLUTION_SERVICE_DEPRIORITIZED_BAD_PROXIES)

// This event is emitted whenever the proxy settings used by
// ConfiguredProxyResolutionService change.
//
// It contains these parameters:
//  {
//     "old_config": <Dump of the previous proxy settings>,
//     "new_config": <Dump of the new proxy settings>,
//  }
//
// Note that the "old_config" key will be omitted on the first fetch of the
// proxy settings (since there wasn't a previous value).
EVENT_TYPE(PROXY_CONFIG_CHANGED)

// Emitted when a list of bad proxies is reported to the proxy service.
//
// Parameters:
//   {
//     "bad_proxy_list": <List of bad proxies>,
//   }
EVENT_TYPE(BAD_PROXY_LIST_REPORTED)

// ------------------------------------------------------------------------
// ProxyList
// ------------------------------------------------------------------------

// Emitted when the first proxy server in a list is being marked as
// bad and proxy resolution is going to failover to the next one in
// the list.  The fallback is local to the request.
//
// Parameters:
//   {
//     "bad_proxy": <URI representation of the failed proxy server>,
//   }
EVENT_TYPE(PROXY_LIST_FALLBACK)

// ------------------------------------------------------------------------
// ProxyResolverV8Tracing
// ------------------------------------------------------------------------

// This event is emitted when a javascript error has been triggered by a
// PAC script. It contains the following event parameters:
//   {
//      "line_number": <The line number in the PAC script
//                      (or -1 if not applicable)>,
//      "message": <The error message>,
//   }
EVENT_TYPE(PAC_JAVASCRIPT_ERROR)

// This event is emitted when a PAC script called alert(). It contains the
// following event parameters:
//   {
//      "message": <The string of the alert>,
//   }
EVENT_TYPE(PAC_JAVASCRIPT_ALERT)

// ------------------------------------------------------------------------
// MultiThreadedProxyResolver
// ------------------------------------------------------------------------

// Measures the time that a proxy resolve request was stalled waiting for a
// proxy resolver thread to free-up.
EVENT_TYPE(WAITING_FOR_PROXY_RESOLVER_THREAD)

// This event is emitted just before a PAC request is bound to a thread. It
// contains these parameters:
//
//   {
//     "thread_number": <Identifier for the PAC thread that is going to
//                       run this request>,
//   }
EVENT_TYPE(SUBMITTED_TO_RESOLVER_THREAD)

// ------------------------------------------------------------------------
// Socket (Shared by stream and datagram sockets)
// ------------------------------------------------------------------------

// Marks the begin/end of a socket (TCP/SOCKS/SSL/UDP/"SpdyProxyClientSocket").
//
// The BEGIN phase contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the controlling entity>,
//   }
EVENT_TYPE(SOCKET_ALIVE)

// Records Open calls to sockets.
//   {
//     "net_error": <On failure; net integer error code>
//   }
EVENT_TYPE(SOCKET_OPEN)

// Records Connect calls to sockets.
//   {
//     "address": <Remote address being connected to>
//     "net_error": <On failure; net integer error code>
//   }
EVENT_TYPE(SOCKET_CONNECT)

// Records BindToNetwork calls to sockets.
//   {
//     "network": <Network this socket is being bound to>
//     "net_error": <On failure; net integer error code>
//   }
EVENT_TYPE(SOCKET_BIND_TO_NETWORK)

// ------------------------------------------------------------------------
// Brokered Socket (Shared by stream and datagram sockets)
// ------------------------------------------------------------------------

// Marks the begin/end of a brokered socket (TCP/UDP)
// The BEGIN phase contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the controlling entity>,
//   }
EVENT_TYPE(BROKERED_SOCKET_ALIVE)

// The start/end of creating a new socketfd in the network service sandbox. This
// event is only used when a socket needs to be created outside of the network
// sandbox.
//
// The END event wil contain the following parameters:
//   {
//     "net_error": <Net integer error code>
//   }
EVENT_TYPE(BROKERED_CREATE_SOCKET)

// ------------------------------------------------------------------------
// StreamSocket
// ------------------------------------------------------------------------

// The start/end of a TCP connect(). This corresponds with a call to
// TCPClientSocket::Connect().
//
// The START event contains these parameters:
//
//   {
//     "address_list": <List of network address strings>,
//   }
//
// And the END event will contain the following parameters:
//
//   {
//     "net_error": <Net integer error code, on error>,
//     "source_address": <Local source address of the connection, on success>,
//   }
EVENT_TYPE(TCP_CONNECT)

// Nested within TCP_CONNECT, there may be multiple attempts to connect
// to the individual addresses. The START event will describe the
// address the attempt is for:
//
//   {
//     "address": <String of the network address>,
//   }
//
// The END event will contain one of the following:
//
// On success:
//   {
//     "local_address": <The local address, as a string>,
//     "remote_address": <The remote address, as a string>,
//   }
//
// On success, but unable to determine the connected address:
//   {
//     "get_address_net_error": <Net integer error code>,
//   }
//
//   {
//     "net_error": <Net integer error code>,
//   }
EVENT_TYPE(TCP_CONNECT_ATTEMPT)

// The start/end of a TCP accept(). This corresponds with a call to
// TCPServerSocket::Accept().
//
// The END event will contain the following parameters on success:
//   {
//     "address": <Remote address of the accepted connection>,
//   }
// On failure it contains the following parameters
//   {
//     "net_error": <Net integer error code>,
//   }
EVENT_TYPE(TCP_ACCEPT)

// This event is logged to the socket stream whenever the socket is
// acquired/released via a ClientSocketHandle.
//
// The BEGIN phase contains the following parameters:
//
//   {
//     "source_dependency": <Source identifier for the controlling entity>,
//   }
EVENT_TYPE(SOCKET_IN_USE)

// The start/end of a SOCKS connect().
EVENT_TYPE(SOCKS_CONNECT)

// The start/end of a SOCKS5 connect().
EVENT_TYPE(SOCKS5_CONNECT)

// This event is emitted when the SOCKS connect fails because the provided
// was longer than 255 characters.
EVENT_TYPE(SOCKS_HOSTNAME_TOO_BIG)

// These events are emitted when insufficient data was read while
// trying to establish a connection to the SOCKS proxy server
// (during the greeting phase or handshake phase, respectively).
EVENT_TYPE(SOCKS_UNEXPECTEDLY_CLOSED_DURING_GREETING)
EVENT_TYPE(SOCKS_UNEXPECTEDLY_CLOSED_DURING_HANDSHAKE)

// This event indicates that a bad version number was received in the
// proxy server's response. The extra parameters show its value:
//   {
//     "version": <Integer version number in the response>,
//   }
EVENT_TYPE(SOCKS_UNEXPECTED_VERSION)

// This event indicates that the SOCKS proxy server returned an error while
// trying to create a connection. The following parameters will be attached
// to the event:
//   {
//     "error_code": <Integer error code returned by the server>,
//   }
EVENT_TYPE(SOCKS_SERVER_ERROR)

// This event indicates that the SOCKS proxy server asked for an authentication
// method that we don't support. The following parameters are attached to the
// event:
//   {
//     "method": <Integer method code>,
//   }
EVENT_TYPE(SOCKS_UNEXPECTED_AUTH)

// This event indicates that the SOCKS proxy server's response indicated an
// address type which we are not prepared to handle.
// The following parameters are attached to the event:
//   {
//     "address_type": <Integer code for the address type>,
//   }
EVENT_TYPE(SOCKS_UNKNOWN_ADDRESS_TYPE)

// The start/end of an SSL "connect" (aka client handshake).
// The following parameters are attached to the END event:
//
//  {
//    "version": <String name of the TLS version negotiated>,
//    "cipher_suite": <Integer code for the cipher suite>,
//    "is_resumed": <Whether we resumed a session>,
//    "next_proto": <The next protocol negotiated via ALPN>,
//  }
EVENT_TYPE(SSL_CONNECT)

// Emitted at the start of an SSL handshakes that use ECH. The following
// parameters are attached.
// {
//     "bytes": <The ECHConfigList used, base64 encoded>
// }
EVENT_TYPE(SSL_ECH_CONFIG_LIST)

// The start/end of an SSL server handshake (aka "accept").
EVENT_TYPE(SSL_SERVER_HANDSHAKE)

// The SSL server requested a client certificate.
EVENT_TYPE(SSL_CLIENT_CERT_REQUESTED)

// The SSL stack blocked on a private key operation. The following parameters
// are attached to the event.
//   {
//     "algorithm": <TLS signature algorithm used>,
//     "provider": <Human-readable name of the crypto provider>,
//   }
EVENT_TYPE(SSL_PRIVATE_KEY_OP)

// A client certificate (or none) was provided to the SSL library to be sent
// to the SSL server.
// The following parameters are attached to the event:
//   {
//     "cert_count": <Number of certificates>,
//   }
//   A cert_count of 0 means no client certificate was provided.
//   A cert_count of -1 means a client certificate was provided but we don't
//   know the size of the certificate chain.
EVENT_TYPE(SSL_CLIENT_CERT_PROVIDED)

// An SSL error occurred while trying to do the indicated activity.
// The following parameters are attached to the event:
//   {
//     "net_error": <Integer code for the specific error type>,
//     "ssl_lib_error": <SSL library's integer code for the specific error type>
//   }
EVENT_TYPE(SSL_HANDSHAKE_ERROR)
EVENT_TYPE(SSL_READ_ERROR)
EVENT_TYPE(SSL_WRITE_ERROR)

// We found that our prediction of the server's certificates was correct and
// we merged the verification with the SSLHostInfo. (Note: now obsolete.)
EVENT_TYPE(SSL_VERIFICATION_MERGED)

// An SSL connection sent or received an alert.
// The following parameters are attached:
//   {
//     "bytes": <The exact bytes sent, Base64 encoded>
//   }
EVENT_TYPE(SSL_ALERT_RECEIVED)
EVENT_TYPE(SSL_ALERT_SENT)

// The SSL connection is being confirmed.
EVENT_TYPE(SSL_CONFIRM_HANDSHAKE)

// An SSL connection sent or received a handshake message.
// `SSL_ENCRYPTED_CLIENT_HELLO` means the handshake message was not sent over
// the wire but encrypted with the Encrypted ClientHello (ECH) extension. The
// following parameters are attached:
//   {
//     "type": <The type of the handshake message, as an integer>
//     "bytes": <The exact bytes sent, Base64 encoded.
//               May be elided in some cases>
//   }
EVENT_TYPE(SSL_HANDSHAKE_MESSAGE_RECEIVED)
EVENT_TYPE(SSL_HANDSHAKE_MESSAGE_SENT)
EVENT_TYPE(SSL_ENCRYPTED_CLIENT_HELLO)

// The specified number of bytes were sent on the socket.  Depending on the
// source of the event, may be logged either once the data is sent, or when it
// is queued to be sent.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just sent>,
//     "bytes": <The exact bytes sent, Base64 encoded.
//               Only present when byte logging is enabled>,
//   }
EVENT_TYPE(SOCKET_BYTES_SENT)
EVENT_TYPE(SSL_SOCKET_BYTES_SENT)

// The specified number of bytes were received on the socket.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just received>,
//     "bytes": <The exact bytes received, Base64 encoded.
//               Only present when byte logging is enabled>,
//   }
EVENT_TYPE(SOCKET_BYTES_RECEIVED)
EVENT_TYPE(SSL_SOCKET_BYTES_RECEIVED)

// A socket error occurred while trying to do the indicated activity.
// The following parameters are attached to the event:
//   {
//     "net_error": <Integer code for the specific error type>,
//     "os_error": <Integer error code the operating system returned>
//   }
EVENT_TYPE(SOCKET_READ_ERROR)
EVENT_TYPE(SOCKET_WRITE_ERROR)

// The socket was closed locally (The socket may or may not have been closed
// by the remote side already)
EVENT_TYPE(SOCKET_CLOSED)

// Certificates were received from the SSL server (during a handshake or
// renegotiation). The following parameters are attached to the event:
//  {
//    "certificates": <A list of PEM encoded certificates in the order that
//                     they were sent by the server>,
//  }
EVENT_TYPE(SSL_CERTIFICATES_RECEIVED)

// Signed Certificate Timestamps were received from the server.
// The following parameters are attached to the event:
// {
//    "embedded_scts": Base64-encoded SignedCertificateTimestampList,
//    "scts_from_ocsp_response": Base64-encoded SignedCertificateTimestampList,
//    "scts_from_tls_extension": Base64-encoded SignedCertificateTimestampList,
// }
//
// The SignedCertificateTimestampList is defined in RFC6962 and is exactly as
// received from the server.
EVENT_TYPE(SIGNED_CERTIFICATE_TIMESTAMPS_RECEIVED)

// Signed Certificate Timestamps were checked.
// The following parameters are attached to the event:
// {
//    "verified_scts": <A list of SCTs>,
//    "invalid_scts": <A list of SCTs>,
//    "scts_from_unknown_logs": <A list of SCTs>,
// }
//
// Where each SCT is an object:
// {
//    "origin": <one of: "Embedded in certificate", "tls_extension", "ocsp">,
//    "version": <numeric version>,
//    "log_id": <base64-encoded log id>,
//    "timestamp": <numeric timestamp in milliseconds since the Unix epoch>,
//    "hash_algorithm": <name of the hash algorithm>,
//    "signature_algorithm": <name of the signature algorithm>,
//    "signature_data": <base64-encoded signature bytes>,
// }
EVENT_TYPE(SIGNED_CERTIFICATE_TIMESTAMPS_CHECKED)

// The certificate was checked for compliance with Certificate Transparency
// requirements.
//
// The following parameters are attached to the event:
// {
//    "certificate": <An X.509 certificate, same format as in
//                   CERT_VERIFIER_JOB.>
//    "build_timely": <boolean>
//    "ct_compliance_status": <string describing compliance status>
// }
EVENT_TYPE(CERT_CT_COMPLIANCE_CHECKED)

// A Certificate Transparency log entry was audited for inclusion in the
// log.
//
// The following parameters are attached to the event:
// {
//  "log_entry": <hex-encoded hash of the log entry>
//  "log_id": <hex-encoded hash of the CT log>
//  "success": <boolean>
// }

EVENT_TYPE(CT_LOG_ENTRY_AUDITED)

// ------------------------------------------------------------------------
// DatagramSocket
// ------------------------------------------------------------------------

// The start/end of a UDP client connecting.
//
// The START event contains these parameters:
//
//   {
//     "address": <Remote address being connected to>,
//     "bound_to_network": <optional; network handle for the network that this
//                          socket is bound to.  Only present when this socket
//                          is bound to a network.>
//   }
//
// And the END event will contain the following parameter:
//
//   {
//     "net_error": <Net integer error code, on failure>,
//   }
EVENT_TYPE(UDP_CONNECT)

// The local address of the UDP socket, retrieved via getsockname.
// The following parameters are attached:
//   {
//     "address": <Local address bound to the socket>,
//     "bound_to_network": <optional; network handle for the network that this
//                          socket is bound to.  Only present when this socket
//                          is bound to a network.>
//   }
EVENT_TYPE(UDP_LOCAL_ADDRESS)

// The specified number of bytes were transferred on the socket.
// The following parameters are attached:
//   {
//     "address": <Remote address of data transfer.  Not present when not
//                 specified for UDP_BYTES_SENT events>,
//     "byte_count": <Number of bytes that were just received>,
//     "bytes": <The exact bytes received, Base64 encoded.
//               Only present when byte logging is enabled>,
//   }
EVENT_TYPE(UDP_BYTES_RECEIVED)
EVENT_TYPE(UDP_BYTES_SENT)

// Logged when an error occurs while reading or writing to/from a UDP socket.
// The following parameters are attached:
//   {
//     "net_error": <Net error code>,
//   }
EVENT_TYPE(UDP_RECEIVE_ERROR)
EVENT_TYPE(UDP_SEND_ERROR)

// ------------------------------------------------------------------------
// ConnectJob
// ------------------------------------------------------------------------

// The start/end of a ConnectJob.
EVENT_TYPE(CONNECT_JOB)

// This event is logged whenever the ConnectJob gets a new socket
// association. The event parameters point to that socket:
//
//   {
//     "source_dependency": <The source identifier for the new socket.>,
//   }
EVENT_TYPE(CONNECT_JOB_SET_SOCKET)

// Whether the connect job timed out.
EVENT_TYPE(CONNECT_JOB_TIMED_OUT)

// ------------------------------------------------------------------------
// ConnectJob subclasses
// ------------------------------------------------------------------------

// The start/end of the TransportConnectJob::Connect().
EVENT_TYPE(TRANSPORT_CONNECT_JOB_CONNECT)

// The start/end of the SSLConnectJob::Connect().
EVENT_TYPE(SSL_CONNECT_JOB_CONNECT)

// The start/end of the SOCKSConnectJob::Connect().
EVENT_TYPE(SOCKS_CONNECT_JOB_CONNECT)

// The start/end of the HttpProxyConnectJob::Connect().
EVENT_TYPE(HTTP_PROXY_CONNECT_JOB_CONNECT)

// A TLS connection attempt failed because ECH was not negotiated. The
// connection will be retried with a new ECHConfigList from the client-facing
// server. If the new ECHConfigList is the empty string, the server has rolled
// back ECH and the new attempt will have ECH disabled.
//
//   {
//     "bytes": <The new ECHConfigList, base64-encoded>
//   }
EVENT_TYPE(SSL_CONNECT_JOB_RESTART_WITH_ECH_CONFIG_LIST)

// This event is logged when the TransportConnectJob IPv6 fallback timer expires
// and the IPv4 addresses are attempted.
EVENT_TYPE(TRANSPORT_CONNECT_JOB_IPV6_FALLBACK)

// This event is logged whenever the ConnectJob attempts a new TCP connection.
// association. The ConnectJob may attempt multiple addresses in parallel, so
// this event does not log when the connection attempt succeeds or fails. The
// source dependency may be used to determine this.
//
//   {
//     "address": <String of the network address being attempted>,
//     "source_dependency": <The source identifier for the new socket.>,
//   }
EVENT_TYPE(TRANSPORT_CONNECT_JOB_CONNECT_ATTEMPT)

// ------------------------------------------------------------------------
// ClientSocketPoolBaseHelper
// ------------------------------------------------------------------------

// The start/end of a client socket pool request for a socket.
EVENT_TYPE(SOCKET_POOL)

// The request stalled because there are too many sockets in the pool.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS)

// The request stalled because there are too many sockets in the group.
EVENT_TYPE(SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP)

// Indicates that we reused an existing socket. Attached to the event are
// the parameters:
//   {
//     "idle_ms": <The number of milliseconds the socket was sitting idle for>,
//   }
EVENT_TYPE(SOCKET_POOL_REUSED_AN_EXISTING_SOCKET)

// This event simply describes the host:port that were requested from the
// socket pool. Its parameters are:
//   {
//     "group_id": <The group id for the socket request>,
//   }
EVENT_TYPE(TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET)

// This event simply describes the host:port that were requested from the
// socket pool. Its parameters are:
//   {
//     "group_id": <The group id for the socket request>,
//   }
EVENT_TYPE(TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKETS)

// A connect job is created by a socket pool. Its parameters are:
//   {
//     "backup_job": <Whether this is a backup job created because the other
//                    ConnectJob was taking too long>,
//     "group_id": <The group id for the socket request>,
//   }
EVENT_TYPE(SOCKET_POOL_CONNECT_JOB_CREATED)

// This event is sent when a connect job is eventually bound to a request
// (because of late binding and socket backup jobs, we don't assign the job to
// a request until it has completed).
//
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for the connect job we are
//                            bound to>,
//   }
EVENT_TYPE(SOCKET_POOL_BOUND_TO_CONNECT_JOB)

// Identifies the NetLogSource() for the Socket assigned to the pending
// request. The event parameters are:
//   {
//      "source_dependency": <Source identifier for the socket we acquired>,
//   }
EVENT_TYPE(SOCKET_POOL_BOUND_TO_SOCKET)

// The start/end of a client socket pool request for multiple sockets.
// The event parameters are:
//   {
//      "num_sockets": <Number of sockets we're trying to ensure are connected>,
//   }
EVENT_TYPE(SOCKET_POOL_CONNECTING_N_SOCKETS)

// Logged when a socket pool is closing a socket:
//   {
//      "reason": <Reason the socket was closed>,
//   }
EVENT_TYPE(SOCKET_POOL_CLOSING_SOCKET)

// ------------------------------------------------------------------------
// StreamAttempt and subclasses
// ------------------------------------------------------------------------

// Emitted when a StreamAttempt is created by HttpStreamPool.
EVENT_TYPE(STREAM_ATTEMPT_BOUND_TO_POOL)

// Marks the creation/destruction of a TcpStreamAttempt.
// For the BEGIN phase, the following parameter is attached:
//   {
//      "ip_endpoint": <The IPEndPoint to connect>,
//   }
//
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>,
//   }
EVENT_TYPE(TCP_STREAM_ATTEMPT_ALIVE)

// Marks the creation/destruction of a TlsStreamAttempt.
// For the BEGIN phase, the following parameter is attached:
//   {
//      "host_port": <The host and port of the destination>,
//   }
//
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>,
//   }
EVENT_TYPE(TLS_STREAM_ATTEMPT_ALIVE)

// Measures the time TlsStreamAttempt was waiting SSLConfig to be ready.
EVENT_TYPE(TLS_STREAM_ATTEMPT_WAIT_FOR_SSL_CONFIG)

// Measures the time TlsStreamAttempt took to connect (TLS handshake).
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>,
//   }
EVENT_TYPE(TLS_STREAM_ATTEMPT_CONNECT)

// ------------------------------------------------------------------------
// URLRequest
// ------------------------------------------------------------------------

// Measures the time it took a net::URLRequestJob to start. For the most part
// this corresponds with the time between net::URLRequest::Start() and
// net::URLRequest::ResponseStarted(), however it is also repeated for every
// redirect, and every intercepted job that handles the request.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//      "url": <String of URL being loaded>,
//      "method": <The method ("POST" or "GET" or "HEAD" etc..)>,
//      "initiator": <Initiator origin of the request, if any, or else "not an
//                    origin">,
//      "load_flags": <Numeric value of the combined load flags>,
//      "network_isolation_key": <NIK associated with the request>,
//      "request_type": <Type of the request, which is "main frame", "subframe",
//                       or "other">,
//      "priority": <Numeric priority of the request>,
//      "site_for_cookies": <SiteForCookies associated with the request>,
//      "traffic_annotation": <int32 for the request's TrafficAnnotationTag>,
//      "upload_id" <String of upload body identifier, if present>,
//   }
//
// For the END phase, if there was an error, the following parameters are
// attached:
//   {
//      "net_error": <Net error code of the failure>,
//   }
EVENT_TYPE(URL_REQUEST_START_JOB)

// This event is sent once a net::URLRequest receives a redirect. The parameters
// attached to the event are:
//   {
//     "location": <The URL that was redirected to>,
//   }
EVENT_TYPE(URL_REQUEST_REDIRECTED)

// This event is sent once a net::URLRequest receives a (valid)
// `Activate-Storage-Access: retry` response header.
EVENT_TYPE(URL_REQUEST_RETRY_WITH_STORAGE_ACCESS)

// Measures the time between when a net::URLRequest calls a delegate that can
// block it, and when the delegate allows the request to resume. Each delegate
// type has a corresponding event type.
EVENT_TYPE(NETWORK_DELEGATE_BEFORE_START_TRANSACTION)
EVENT_TYPE(NETWORK_DELEGATE_BEFORE_URL_REQUEST)
EVENT_TYPE(NETWORK_DELEGATE_HEADERS_RECEIVED)
EVENT_TYPE(URL_REQUEST_DELEGATE_CERTIFICATE_REQUESTED)
EVENT_TYPE(URL_REQUEST_DELEGATE_RECEIVED_REDIRECT)
EVENT_TYPE(URL_REQUEST_DELEGATE_RESPONSE_STARTED)
EVENT_TYPE(URL_REQUEST_DELEGATE_SSL_CERTIFICATE_ERROR)

// Like the above events, but the END phase also has the following parameter:
//   {
//     "net_error": <Network error code. Only present on error.
//   }
EVENT_TYPE(URL_REQUEST_DELEGATE_CONNECTED)

// Logged when a delegate informs the URL_REQUEST of what's currently blocking
// the request. The parameters attached to the begin event are:
//   {
//     "delegate_blocked_by": <Information about what's blocking the request>,
//   }
EVENT_TYPE(DELEGATE_INFO)

// The specified number of bytes were read from the net::URLRequest.
// The filtered event is used when the bytes were passed through a filter before
// being read.  This event is only present when byte logging is enabled.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just sent>,
//     "bytes": <The exact bytes sent, Base64 encoded>,
//   }
EVENT_TYPE(URL_REQUEST_JOB_BYTES_READ)
EVENT_TYPE(URL_REQUEST_JOB_FILTERED_BYTES_READ)

// This event is sent when the priority of a net::URLRequest is
// changed after it has started. The following parameters are attached:
//   {
//     "priority": <Numerical value of the priority (higher is more important)>,
//   }
EVENT_TYPE(URL_REQUEST_SET_PRIORITY)

EVENT_TYPE(URL_REQUEST_REDIRECT_JOB)
// This event is logged when a URLRequestRedirectJob is started for a request.
// The following parameters are attached:
//   {
//     "reason": <Reason for the redirect, as a string>,
//   }

EVENT_TYPE(URL_REQUEST_FAKE_RESPONSE_HEADERS_CREATED)
// This event is logged when a URLRequestRedirectJob creates the fake response
// headers for a request, prior to returning them.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }

EVENT_TYPE(URL_REQUEST_FILTERS_SET)
// This event is logged when a URLRequestJob sets up the filters, if any
// filters were added to the job.  It logs the filters added.
// The following parameters are attached:
//   {
//     "filters": <The list of filter names>
//   }

// ------------------------------------------------------------------------
// HttpCache
// ------------------------------------------------------------------------

// Measures the time while getting a reference to the back end.
EVENT_TYPE(HTTP_CACHE_GET_BACKEND)

// Measures the time while getting a disk cache entry with OpenOrCreateEntry().
// The following parameters are attached:
//   {
//     "net_error": <Network error code.  Only present on error>,
//     "result": <"opened" or "created".  Only present on success>
//   }
EVENT_TYPE(HTTP_CACHE_OPEN_OR_CREATE_ENTRY)

// Measures the time while opening a disk cache entry.
EVENT_TYPE(HTTP_CACHE_OPEN_ENTRY)

// Measures the time while creating a disk cache entry.
EVENT_TYPE(HTTP_CACHE_CREATE_ENTRY)

// Measures the time it takes to add a HttpCache::Transaction to an http cache
// entry's list of active Transactions.
EVENT_TYPE(HTTP_CACHE_ADD_TO_ENTRY)

// Measures the time while deleting a disk cache entry.
EVENT_TYPE(HTTP_CACHE_DOOM_ENTRY)

// Measures the time while reading/writing a disk cache entry's response headers
// or metadata.
EVENT_TYPE(HTTP_CACHE_READ_INFO)
EVENT_TYPE(HTTP_CACHE_WRITE_INFO)

// Measures the time while reading/writing a disk cache entry's body.
EVENT_TYPE(HTTP_CACHE_READ_DATA)
EVENT_TYPE(HTTP_CACHE_WRITE_DATA)

// The request headers received by the HTTP cache.
// The following parameters are attached:
//   {
//     "line": <empty>,
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_CACHE_CALLER_REQUEST_HEADERS)

// Signal a significant change on the flow of the state machine: start again
// from scratch or create a new network request for byte-range operations.
// There are no parameters.
EVENT_TYPE(HTTP_CACHE_RESTART_PARTIAL_REQUEST)
EVENT_TYPE(HTTP_CACHE_RE_SEND_PARTIAL_REQUEST)

// ------------------------------------------------------------------------
// Disk Cache / Memory Cache
// ------------------------------------------------------------------------

// The creation/destruction of a disk_cache::EntryImpl object.  The "creation"
// is considered to be the point at which an Entry is first considered to be
// good and associated with a key.  Note that disk and memory cache entries
// share event types.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "created": <true if the Entry was created, rather than being opened>,
//     "key": <The Entry's key>,
//   }
EVENT_TYPE(DISK_CACHE_ENTRY_IMPL)
EVENT_TYPE(DISK_CACHE_MEM_ENTRY_IMPL)

// Logs the time required to read/write data from/to a cache entry.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "index": <Index being read/written>,
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//     "truncate": <If present for a write, the truncate flag is set to true.
//                  Not present in reads or writes where it is false>,
//   }
//
// For the END phase, the following parameters are attached:
//   {
//     "bytes_copied": <Number of bytes copied.  Not present on error>,
//     "net_error": <Network error code.  Only present on error>,
//   }
EVENT_TYPE(ENTRY_READ_DATA)
EVENT_TYPE(ENTRY_WRITE_DATA)

// Logged when sparse read/write operation starts/stops for an Entry.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "offset": <Offset at which to start reading>,
//     "buf_len": <Bytes to read/write>,
//   }
EVENT_TYPE(SPARSE_READ)
EVENT_TYPE(SPARSE_WRITE)

// Logged when a parent Entry starts/stops reading/writing a child Entry's data.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "source_dependency": <Source id of the child entry>,
//     "child_len": <Bytes to read/write from/to child>,
//   }
EVENT_TYPE(SPARSE_READ_CHILD_DATA)
EVENT_TYPE(SPARSE_WRITE_CHILD_DATA)

// Logged when sparse GetAvailableRange operation starts/stops for an Entry.
//
// For the BEGIN phase, the following parameters are attached:
//   {
//     "buf_len": <Bytes to read/write>,
//     "offset": <Offset at which to start reading>,
//   }
//
// For the END phase, the following parameters are attached.  No parameters are
// attached when cancelled:
//   {
//     "length": <Length of returned range. Only present on success>,
//     "start": <Position where returned range starts. Only present on success>,
//     "net_error": <Resulting error code. Only present on failure. This may be
//                   "OK" when there's no error, but no available bytes in the
//                   range>,
//   }
EVENT_TYPE(SPARSE_GET_RANGE)

// Indicates the children of a sparse EntryImpl are about to be deleted.
// Not logged for MemEntryImpls.
EVENT_TYPE(SPARSE_DELETE_CHILDREN)

// Logged when an EntryImpl is closed.  Not logged for MemEntryImpls.
EVENT_TYPE(ENTRY_CLOSE)

// Logged when an entry is doomed.
EVENT_TYPE(ENTRY_DOOM)

// ------------------------------------------------------------------------
// HttpStreamFactory
// ------------------------------------------------------------------------

// Measures the time taken to fulfill the HttpStreamRequest.
EVENT_TYPE(HTTP_STREAM_REQUEST)

// Measures the time taken to execute the HttpStreamFactory::Job
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for the Request with started
//                            this Job>,
//      "logical_destination":
//          <The scheme + origin + port that the connection is logically to.
//           This has any host mapping rules already applied, and for https/wss
//           is the host required for certificate validation>,
//      "destination":
//          <The scheme + origin + port that the connection will actually be
//           made to. The difference from `logical_destination` is that
//           this is after applying alternate protocol, if applicable, and
//           replacing ws/wss with http/https. Host mappy rules have also been
//           applied a second time, if there was an applicable alternate
//           protocol applied>,
//      "expect_spdy": <Boolean indicating whether the Job will use SPDY>,
//      "using_quic": <Boolean indicating whether the Job will use QUIC>,
//      "priority": <The priority of the Job>,
//      "type": <The type of this Job ("main", "alternative", "dns_alpn_h3",
//               "preconnect")>,
//   }
EVENT_TYPE(HTTP_STREAM_JOB)

// Measures the time and HttpStreamFactory::Job spends waiting for another job.
// The event parameters are:
//   {
//      "should_wait": <True if the job needs to wait>,
//   }
EVENT_TYPE(HTTP_STREAM_JOB_WAITING)

// Identifies the NetLogSource() for a Job started by the Request.
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for Job we started>,
//   }
EVENT_TYPE(HTTP_STREAM_REQUEST_STARTED_JOB)

// Emitted when a job is throttled.
EVENT_TYPE(HTTP_STREAM_JOB_THROTTLED)

// Emitted when a job resumes initializing a connection after being previously
// throttled.
EVENT_TYPE(HTTP_STREAM_JOB_RESUME_INIT_CONNECTION)

// Emitted when a job is asked to initialize a connection.
EVENT_TYPE(HTTP_STREAM_JOB_INIT_CONNECTION)

// Identifies the NetLogSource() for the Job that fulfilled the Request.
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for Job we acquired>,
//   }
EVENT_TYPE(HTTP_STREAM_REQUEST_BOUND_TO_JOB)

// Identifies the NetLogSource() for the Request that the Job was attached to.
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for the Request to which we were
//                            attached>,
//   }
EVENT_TYPE(HTTP_STREAM_JOB_BOUND_TO_REQUEST)

// Logs the protocol negotiated with the server. The event parameters are:
//   {
//      "proto": <The ALPN protocol negotiated>,
//   }
EVENT_TYPE(HTTP_STREAM_REQUEST_PROTO)

// Emitted when a Job is orphaned because the Request was bound to a different
// Job. The orphaned Job will continue to run to completion.
EVENT_TYPE(HTTP_STREAM_JOB_ORPHANED)

// Emitted when a job is delayed.
//   {
//     "delay_ms": <Number of milliseconds until job will be resumed>
//   }
EVENT_TYPE(HTTP_STREAM_JOB_DELAYED)

// Emitted when a job is asked to resume after non-zero microseconds.
//   {
//     "delay_ms": <Number of milliseconds the job was delayed before resuming>
//   }
EVENT_TYPE(HTTP_STREAM_JOB_RESUMED)

// Marks the start/end of a HttpStreamFactory::JobController.
// The following parameters are attached:
//   {
//      "url": <String of request URL>,
//      "url_after_host_mapping": <URL after applying hostmapping.
//                                 Only present if different from URL>,
//      "is_preconnect": <True if controller is created for a preconnect>,
//      "private_mode": <Privacy mode of the request>,
//   }
EVENT_TYPE(HTTP_STREAM_JOB_CONTROLLER)

// Links a JobController with its user (a URL_REQUEST).
// The event parameters are:
//   {
//      "source_dependency": <The source identifier for JobController if the
//          event is logged in URL_REQUEST or the source identifier for the
//          URL_REQUEST if the event is logged in HTTP_STREAM_JOB_CONTROLLER>,
//   }
EVENT_TYPE(HTTP_STREAM_JOB_CONTROLLER_BOUND)

// Logs the proxy server resolved for the controller. The event parameters are:
//   {
//      "proxy_server": The proxy server resolved for the Job,
//   }
EVENT_TYPE(HTTP_STREAM_JOB_CONTROLLER_PROXY_SERVER_RESOLVED)

// Logs an alternative service found by the controller. The event parameters
// are:
//   {
//      "alt_svc": The alternative service.
//      "broken": <boolean>
//   }
EVENT_TYPE(HTTP_STREAM_JOB_CONTROLLER_ALT_SVC_FOUND)

// ------------------------------------------------------------------------
// HttpStreamPool
// ------------------------------------------------------------------------

// Logged when the HttpStreamPool is closing a StreamSocket:
//   {
//      "reason": <Reason the socket was closed>,
//   }
EVENT_TYPE(HTTP_STREAM_POOL_CLOSING_SOCKET)

// Marks the start/end of a HttpStreamPool::Group.
// The following parameters are attached:
//   {
//      "stream_key": <The HttpStreamKey of the group>,
//      "force_quic": <True when QUIC is forced for the group>,
//   }
EVENT_TYPE(HTTP_STREAM_POOL_GROUP_ALIVE)

// Emitted when a group starts a stream. The event parameters are:
//   {
//     "priority": <The priority of the erquest>,
//     "allowed_bad_certs": <The list of allowed bad certs>,
//     "enable_ip_based_pooling": <True when the request enables IP based
//                                 pooling>,
//     "quic_version": <The QUIC version to attempt>,
//     "source_dependency": <The source identifier of the request>
//   }
EVENT_TYPE(HTTP_STREAM_POOL_GROUP_START_JOB)

// Emitted when a group is requested a preconnect. The event parameter is:
//   {
//      "num_streams": <The number of streams requested>,
//      "quic_version": <The QUIC version to attempt>
//   }
EVENT_TYPE(HTTP_STREAM_POOL_GROUP_PRECONNECT)

// Records on the caller's NetLog to indicate that an HttpStreamPool::Group
// starts a Job.
EVENT_TYPE(HTTP_STREAM_POOL_GROUP_JOB_BOUND)

// Emitted when an HttpStreamPool::AttemptManager is created. Used to add a
// reference to HttpStreamPool::Group's net log.
EVENT_TYPE(HTTP_STREAM_POOL_GROUP_ATTEMPT_MANAGER_CREATED)

// Emitted when an HttpStreamPool::AttemptManager is destroyed. Used to add a
// reference to HttpStreamPool::Group's net log.
EVENT_TYPE(HTTP_STREAM_POOL_GROUP_ATTEMPT_MANAGER_DESTROYED)

// Marks the start/end of a HttpStreamPool::AttemptManager.
// For the BEGIN event, the event parameters are:
//   {
//     "stream_attempt_delay": <The stream attempt delay in milliseconds>,
//     "source_dependency": <The source identifier of the parent group>
//   }
EVENT_TYPE(HTTP_STREAM_POOL_ATTEMPT_MANAGER_ALIVE)

// Emitted when an HttpStreamPool::AttemptManager started a StreamAttempt.
EVENT_TYPE(HTTP_STREAM_POOL_ATTEMPT_MANAGER_ATTEMPT_START)

// Emitted when an HttpStreamPool::AttemptManager received completion from a
// StreamAttempt.
EVENT_TYPE(HTTP_STREAM_POOL_ATTEMPT_MANAGER_ATTEMPT_END)

// Emitted when the stream attempt delay has passed on an
// HttpStreamPool::AttemptManager. The event parameter is:
//   {
//     "stream_attempt_delay": <The stream attempt delay in milliseconds>
//   }
EVENT_TYPE(HTTP_STREAM_POOL_ATTEMPT_MANAGER_STREAM_ATTEMPT_DELAY_PASSED)

// Records on an HttpStreamPool::AttemptManager's NetLog to indicate that an
// HttpStreamPool::QuicTask is bound to the AttemptManager.
EVENT_TYPE(HTTP_STREAM_POOL_ATTEMPT_MANAGER_QUIC_TASK_BOUND)

// Marks the start/end of a HttpStreamPool::QuicTask.
// For the BEGIN event, the event parameters are:
//   {
//     "quic_version": <The known QUIC version>,
//     "source_dependency": <The source identifier of the parent AttemptManager>
//   }
EVENT_TYPE(HTTP_STREAM_POOL_QUIC_TASK_ALIVE)

// Emitted when an HttpStreamPool::QuicTask started a QuicSessionAttempt.
// The event parameters are:
//   {
//     "quic_version": <The QUIC version of the attempt>,
//     "ip_endpoint": <The IPEndPoint to connect>,
//     "metadata": <ConnectionEndpointMetadata of the attempt>
//   }
EVENT_TYPE(HTTP_STREAM_POOL_QUIC_ATTEMPT_START)

// Emitted when an HttpStreamPool::QuicTask received completion from a
// QuicSessionAttempt. The event parameter is:
//   {
//      "net_error": <Net error code integer>,
//   }
EVENT_TYPE(HTTP_STREAM_POOL_QUIC_ATTEMPT_END)

// ------------------------------------------------------------------------
// HttpNetworkTransaction
// ------------------------------------------------------------------------

// Measures the time taken to send the tunnel request to the server.
EVENT_TYPE(HTTP_TRANSACTION_TUNNEL_SEND_REQUEST)

// This event is sent for a tunnel request.
// The following parameters are attached:
//   {
//     "line": <The HTTP request line, CRLF terminated>,
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_SEND_TUNNEL_HEADERS)

// Measures the time to read the tunnel response headers from the server.
EVENT_TYPE(HTTP_TRANSACTION_TUNNEL_READ_HEADERS)

// This event is sent on receipt of the HTTP response headers to a tunnel
// request.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS)

// Measures the time taken to send the request to the server.
EVENT_TYPE(HTTP_TRANSACTION_SEND_REQUEST)

// This event is sent for a HTTP request.
// The following parameters are attached:
//   {
//     "line": <The HTTP request line, CRLF terminated>,
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_SEND_REQUEST_HEADERS)

// Logged when a request body is sent.
// The following parameters are attached:
//   {
//     "did_merge": <True if the body was merged with the headers for writing>,
//     "is_chunked": <True if chunked>,
//     "length": <The length of the body.  May not be accurate when body is not
//                in memory>
//   }
EVENT_TYPE(HTTP_TRANSACTION_SEND_REQUEST_BODY)

// This event is sent for a HTTP request over an HTTP/2 stream.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_HTTP2_SEND_REQUEST_HEADERS)

// This event is sent for a HTTP request over a QUIC stream.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//     "quic_priority": <Integer representing the priority of this request>,
//     "quic_stream_id": <Id of the QUIC stream sending this request>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_QUIC_SEND_REQUEST_HEADERS)

// Measures the time to read HTTP response headers from the server.
EVENT_TYPE(HTTP_TRANSACTION_READ_HEADERS)

// This event is sent on receipt of the HTTP response headers.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_READ_RESPONSE_HEADERS)

// This event is sent on receipt of a 103 Early Hints response headers.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_READ_EARLY_HINTS_RESPONSE_HEADERS)

// Measures the time to read the entity body from the server.
EVENT_TYPE(HTTP_TRANSACTION_READ_BODY)

// Measures the time taken to read the response out of the socket before
// restarting for authentication, on keep alive connections.
EVENT_TYPE(HTTP_TRANSACTION_DRAIN_BODY_FOR_AUTH_RESTART)

// This event is sent when we try to restart a transaction after an error.
// The following parameters are attached:
//   {
//     "net_error": <The net error code integer for the failure, if applicable>,
//     "http_status_code": <HTTP status code indicating an error, if
//                          applicable>,
//   }
EVENT_TYPE(HTTP_TRANSACTION_RESTART_AFTER_ERROR)

// This event is sent when we try to restart a transaction after the initial
// attempt failed with HTTP 421 Misdirected Requested.
EVENT_TYPE(HTTP_TRANSACTION_RESTART_MISDIRECTED_REQUEST)

// ------------------------------------------------------------------------
// BidirectionalStream
// ------------------------------------------------------------------------

// Marks the creation/destruction of a net::BidirectionalStream.
// The following parameters are attached:
//   {
//      "url": <The URL being used>,
//      "method": <The HTTP method being used>,
//      "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_ALIVE)

// Marks the ReadData call of a net::BidirectionalStream.
// The following parameters are attached:
// {
//     "rv": <The value in int that is returned to the caller>
// }
EVENT_TYPE(BIDIRECTIONAL_STREAM_READ_DATA)

// Marks the SendvData call of a net::BidirectionalStream.
// The following parameters are attached:
// {
//     "num_buffers": <The number of buffers passed to SendvData>
// }
EVENT_TYPE(BIDIRECTIONAL_STREAM_SENDV_DATA)

// Marks the beginning/end of buffers sent in a net::BidirectionalStream.
// The following parameters are attached:
//   {
//      "num_buffers_coalesced": <number of buffers that were sent together>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_BYTES_SENT_COALESCED)

// The specified number of bytes were sent on the stream.  Depending on the
// source of the event, may be logged either once the data is sent, or when it
// is queued to be sent.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just sent>,
//     "bytes": <The exact bytes sent, Base64 encoded.
//               Only present when byte logging is enabled>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_BYTES_SENT)

// The specified number of bytes were received on the stream.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just received>,
//     "bytes": <The exact bytes received, Base64 encoded.
//               Only present when byte logging is enabled>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_BYTES_RECEIVED)

// This event is sent for receiving headers on the stream.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_RECV_HEADERS)

// This event is sent for receiving trailers on the stream.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_RECV_TRAILERS)

// This event is used when stream is successfully negotiated and is ready for
// sending data and reading data.
// The following parameters are attached:
//   {
//     "request_headers_sent": <boolean>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_READY)

// This event is used when stream has failed.
// The following parameters are attached:
//   {
//     "net_error": <Net error code for the failure>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_FAILED)

// Identifies the NetLogSource() for the QuicSession that handled the stream.
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for session that was used>,
//   }
EVENT_TYPE(BIDIRECTIONAL_STREAM_BOUND_TO_QUIC_SESSION)

// ------------------------------------------------------------------------
// SpdySession
// ------------------------------------------------------------------------

// The start/end of a SpdySession.
//   {
//     "host": <The host-port string>,
//     "proxy": <The Proxy PAC string>,
//   }
EVENT_TYPE(HTTP2_SESSION)

// The SpdySession has been initialized with a socket.
//   {
//     "source_dependency":  <Source identifier for the underlying socket>,
//   }
EVENT_TYPE(HTTP2_SESSION_INITIALIZED)

// This event is sent for sending an HTTP/2 HEADERS frame.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//     "fin": <True if this is the final data set by the peer on this stream>,
//     "stream_id": <The stream id>,
//     "has_priority": <True if the PRIORITY flag is set>,
//     "parent_stream_id": <Optional; the stream id of the parent stream>,
//     "priority": <Optional; the priority value of the stream>,
//     "exclusive": <Optional; true if the exclusive bit is set>.
//   }
EVENT_TYPE(HTTP2_SESSION_SEND_HEADERS)

// This event is sent for receiving an HTTP/2 HEADERS frame.
// The following parameters are attached:
//   {
//     "flags": <The control frame flags>,
//     "headers": <The list of header:value pairs>,
//     "id": <The stream id>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_HEADERS)

// On sending an HTTP/2 SETTINGS frame without ACK flag.
// The following parameters are attached:
//   {
//     "settings": <The list of setting ids and values>,
//   }
EVENT_TYPE(HTTP2_SESSION_SEND_SETTINGS)

// On sending an HTTP/2 SETTINGS frame with ACK flag.
EVENT_TYPE(HTTP2_SESSION_SEND_SETTINGS_ACK)

// Receipt of an HTTP/2 ACCEPT_CH frame.
// The following parameters are attached:
//   {
//     "origin": <The origin associated with the settings>,
//     "accept_ch":  <the raw ACCEPT_CH setting for that origin>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_ACCEPT_CH)

// Receipt of an HTTP/2 SETTINGS frame without ACK flag.
EVENT_TYPE(HTTP2_SESSION_RECV_SETTINGS)

// Receipt of an individual HTTP/2 setting.
// The following parameters are attached:
//   {
//     "id":    <The setting id>,
//     "value": <The setting value>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_SETTING)

// Receipt of an HTTP/2 SETTINGS frame with ACK flag.
EVENT_TYPE(HTTP2_SESSION_RECV_SETTINGS_ACK)

// The receipt of a RST_STREAM frame.
// The following parameters are attached:
//   {
//     "stream_id":  <The stream ID for the RST_STREAM>,
//     "error_code": <Error code>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_RST_STREAM)

// Sending of a RST_STREAM
// The following parameters are attached:
//   {
//     "stream_id":  <The stream ID for the RST_STREAM>,
//     "error_code": <Error code>,
//   }
EVENT_TYPE(HTTP2_SESSION_SEND_RST_STREAM)

// Sending of an HTTP/2 PING frame.
// The following parameters are attached:
//   {
//     "unique_id": <The unique id of the PING message>,
//     "type": <The PING type ("sent", "received")>,
//   }
EVENT_TYPE(HTTP2_SESSION_PING)

// Receipt of an HTTP/2 GOAWAY frame.
// The following parameters are attached:
//   {
//     "last_accepted_stream_id": <Last stream id accepted by the server, duh>,
//     "active_streams":          <Number of active streams>,
//     "unclaimed_streams":       <Number of unclaimed push streams>,
//     "error_code":              <Error code>,
//     "debug_data":              <Additional debug data>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_GOAWAY)

// Receipt of an HTTP/2 WINDOW_UPDATE frame (which controls the send window).
//   {
//     "stream_id": <The stream ID for the window update>,
//     "delta"    : <The delta window size>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_WINDOW_UPDATE)

// Sending of an HTTP/2 WINDOW_UPDATE frame (which controls the
// receive window).
//   {
//     "stream_id": <The stream ID for the window update>,
//     "delta"    : <The delta window size>,
//   }
EVENT_TYPE(HTTP2_SESSION_SEND_WINDOW_UPDATE)

// This event indicates that the send window has been updated for a session.
//   {
//     "delta":      <The window size delta>,
//     "new_window": <The new window size>,
//   }
EVENT_TYPE(HTTP2_SESSION_UPDATE_SEND_WINDOW)

// This event indicates that the recv window has been updated for a session.
//   {
//     "delta":      <The window size delta>,
//     "new_window": <The new window size>,
//   }
EVENT_TYPE(HTTP2_SESSION_UPDATE_RECV_WINDOW)

// This event indicates that an invalid response header has been received.
//   {
//     "header_name": <The header name>,
//     "header_value": <The header value>,
//     "error": <Error message>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_INVALID_HEADER)

// Sending a data frame
//   {
//     "stream_id": <The stream ID for the window update>,
//     "length"   : <The size of data sent>,
//     "flags"    : <Send data flags>,
//   }
EVENT_TYPE(HTTP2_SESSION_SEND_DATA)

// Receiving a data frame
//   {
//     "stream_id": <The stream ID for the window update>,
//     "length"   : <The size of data received>,
//     "flags"    : <Receive data flags>,
//   }
EVENT_TYPE(HTTP2_SESSION_RECV_DATA)

// A stream is stalled by the session send window being closed.
EVENT_TYPE(HTTP2_SESSION_STREAM_STALLED_BY_SESSION_SEND_WINDOW)

// A stream is stalled by its send window being closed.
EVENT_TYPE(HTTP2_SESSION_STREAM_STALLED_BY_STREAM_SEND_WINDOW)

// Session is closing
//   {
//     "net_error"  : <The error status of the closure>,
//     "description": <The textual description for the closure>,
//   }
EVENT_TYPE(HTTP2_SESSION_CLOSE)

// Event when the creation of a stream is stalled because we're at
// the maximum number of concurrent streams.
//  {
//
//    "num_active_streams": <Number of active streams>,
//    "num_created_streams": <Number of created streams>,
//    "num_pushed_streams": <Number of pushed streams>,
//    "max_concurrent_streams": <Maximum number of concurrent streams>,
//    "url": <Request URL>,
//  }
EVENT_TYPE(HTTP2_SESSION_STALLED_MAX_STREAMS)

// Received an out-of-range value for initial window size in SETTINGS
// frame.
//   {
//     "initial_window_size"  : <The initial window size>,
//   }
EVENT_TYPE(HTTP2_SESSION_INITIAL_WINDOW_SIZE_OUT_OF_RANGE)

// Updating streams send window size by the delta window size.
//   {
//     "delta_window_size"    : <The delta window size>,
//   }
EVENT_TYPE(HTTP2_SESSION_UPDATE_STREAMS_SEND_WINDOW_SIZE)

// Sending a greased frame (a frame of reserved type)
//   {
//     "stream_id": <The stream ID for the window update>,
//     "type"     : <Frame type>,
//     "flags"    : <Frame flags>,
//     "length"   : <Frame payload length>,
//     "priority" : <RequestPriority of the stream>,
//   }
EVENT_TYPE(HTTP2_SESSION_SEND_GREASED_FRAME)

// ------------------------------------------------------------------------
// SpdySessionPool
// ------------------------------------------------------------------------

// This event indicates the pool is reusing an existing session
//   {
//     "source_dependency": <The session id>,
//   }
EVENT_TYPE(HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION)

// This event indicates the pool is reusing an existing session from an
// IP pooling match.
//   {
//     "source_dependency": <The session id>,
//   }
EVENT_TYPE(HTTP2_SESSION_POOL_FOUND_EXISTING_SESSION_FROM_IP_POOL)

// This event indicates the pool created a new session
//   {
//     "source_dependency": <The session id>,
//   }
EVENT_TYPE(HTTP2_SESSION_POOL_CREATED_NEW_SESSION)

// This event indicates that a SSL socket has been upgraded to an HTTP/2
// session.
//   {
//     "source_dependency": <The session id>,
//   }
EVENT_TYPE(HTTP2_SESSION_POOL_IMPORTED_SESSION_FROM_SOCKET)

// This event indicates that the session has been removed.
//   {
//     "source_dependency": <The session id>,
//   }
EVENT_TYPE(HTTP2_SESSION_POOL_REMOVE_SESSION)

// ------------------------------------------------------------------------
// SpdyStream
// ------------------------------------------------------------------------

// The begin and end of an HTTP/2 STREAM.
EVENT_TYPE(HTTP2_STREAM)

// A stream is unstalled by flow control.
EVENT_TYPE(HTTP2_STREAM_FLOW_CONTROL_UNSTALLED)

// This event indicates that the send window has been updated for a stream.
//   {
//     "id":         <The stream id>,
//     "delta":      <The window size delta>,
//     "new_window": <The new window size>,
//   }
EVENT_TYPE(HTTP2_STREAM_UPDATE_SEND_WINDOW)

// This event indicates that the recv window has been updated for a stream.
//   {
//     "id":         <The stream id>,
//     "delta":      <The window size delta>,
//     "new_window": <The new window size>,
//   }
EVENT_TYPE(HTTP2_STREAM_UPDATE_RECV_WINDOW)

// This event indicates a stream error
//   {
//     "id":          <The stream id>,
//     "status":      <The error status>,
//     "description": <The textual description for the error>,
//   }
EVENT_TYPE(HTTP2_STREAM_ERROR)

// A PRIORITY update is sent to the server.
//   {
//     "stream_id":        <The stream id>,
//     "parent_stream_id": <The stream's new parent stream>,
//     "weight":           <The stream's new weight>,
//     "exclusive":        <Whether the new dependency is exclusive>,
//   }
EVENT_TYPE(HTTP2_STREAM_SEND_PRIORITY)

// ------------------------------------------------------------------------
// SpdyProxyClientSocket
// ------------------------------------------------------------------------

EVENT_TYPE(HTTP2_PROXY_CLIENT_SESSION)
// Identifies the HTTP/2 session a source is using.
//   {
//     "source_dependency":  <Source identifier for the underlying session>,
//   }

// ------------------------------------------------------------------------
// QuicSessionPool
// ------------------------------------------------------------------------

// This event is emitted when a session request ends up using an existing
// session.
//   {
//     "destination": <The destination of the request>,
//     "reason": <The reason for using the existing session>,
//     "source_dependency": <Source identifier for the session>,
//   }
EVENT_TYPE(QUIC_SESSION_POOL_USE_EXISTING_SESSION)

// This event is emitted for an existing session when an HttpStreamFactory::Job
// is attached to the session.
//   {
//     "source_dependency": <Source identifier for the HttpStreamFactory::Job>,
//   }
EVENT_TYPE(QUIC_SESSION_POOL_ATTACH_HTTP_STREAM_JOB_TO_EXISTING_SESSION)

// This event is emitted whenever a platform notification is received that
// could possibly trigger connection migration.
//   {
//     "signal": <Type of the platform notification>,
//     "network": <The network that triggered the notification>,
//   }
EVENT_TYPE(QUIC_SESSION_POOL_PLATFORM_NOTIFICATION)

// These events track QuicSessionPool's handling of OnIPAddressChanged and
// whether QuicSessions are closed or marked as going away.

EVENT_TYPE(QUIC_SESSION_POOL_ON_IP_ADDRESS_CHANGED)

// This event is emitted when a session request ends up using an existing
// session with the same IP after DNS resolution.
EVENT_TYPE(QUIC_SESSION_POOL_MATCHING_IP_SESSION_FOUND)

// This event is emitted when a session request ends up using an existing
// session with different IP after DNS resolution. This scenario occurs when the
// existing session receives an ORIGIN frame, and the received origins encompass
// the request's destination.
EVENT_TYPE(QUIC_SESSION_POOL_POOLED_WITH_DIFFERENT_IP_SESSION)

// This event is emitted when a session request can use an existing session but
// not due to IP mismatch.
EVENT_TYPE(QUIC_SESSION_POOL_CAN_POOL_BUT_DIFFERENT_IP)

// This event is emitted when a session request cannot use an existing session.
EVENT_TYPE(QUIC_SESSION_POOL_CANNOT_POOL_WITH_EXISTING_SESSIONS)

//   {
//     "net_error": <Net error code for the closure>,
//     "quic_error": <quic::QuicErrorCode in the frame>,
//     "before_active_sessions_size": <The number of active session before
//                                     closing>,
//     "before_all_sessions_size": <The number of all session before
//                                  closing>,
//     "after_active_sessions_size": <The number of active session after
//                                    closing>,
//     "after_all_sessions_size": <The number of all session after closing>,
//   }
EVENT_TYPE(QUIC_SESSION_POOL_CLOSE_ALL_SESSIONS)

EVENT_TYPE(QUIC_SESSION_POOL_MARK_ALL_ACTIVE_SESSIONS_GOING_AWAY)

// ------------------------------------------------------------------------
// QuicSessionPool::Job
// ------------------------------------------------------------------------

// Measures the time taken to execute the QuicSessionPool::Job.
// The event parameters are:
//   {
//     "host": <The origin hostname that the Job serves>,
//     "port": <The origin port>,
//     "privacy_mode": <The privacy mode of the Job>,
//     "proxy_chain": <The proxy chain of the Job>
//     "network_anonymization_key": <The NetworkAnonymizationKey of the Job>,
//   }
EVENT_TYPE(QUIC_SESSION_POOL_JOB)

// Identifies the NetLogSource() requesting the Job. A Job may serve multiple
// sources.
//
// The event parameters are:
//  {
//     "source_dependency": <Source identifier for the NetLogSource to
//                           which we were attached>,
//  }
EVENT_TYPE(QUIC_SESSION_POOL_JOB_BOUND_TO)

// Identifies the NetLogSource() for the QuicSessionPool::Job that the
// this source was bound to.
// The event parameters are:
//  {
//      "source_dependency": <Source identifier for the QuicSessionPool::Job
//                            to which we were attached>,
//  }
EVENT_TYPE(BOUND_TO_QUIC_SESSION_POOL_JOB)

// Measures the time taken by a DirectJob to establish a QUIC connection.
// The event parameters are:
//  {
//     "require_confirmation": <True if we require handshake confirmation
//                              in the connection>
//  }
EVENT_TYPE(QUIC_SESSION_POOL_JOB_CONNECT)

// Measures the time taken by a ProxyJob to establish a connection to its
// endpoint through the proxy.
EVENT_TYPE(QUIC_SESSION_POOL_PROXY_JOB_CONNECT)

// Measures the time taken by a ProxyJob to establish a session to the proxy.
EVENT_TYPE(QUIC_SESSION_POOL_PROXY_JOB_CREATE_PROXY_SESSION)

// This event indicates that the connection on the default network has failed
// before the handshake completed and a new connection on the alternate network
// will be attempted soon.
EVENT_TYPE(QUIC_SESSION_POOL_JOB_RETRY_ON_ALTERNATE_NETWORK)

// This event indicates that the stale host result is used to try connecting.
EVENT_TYPE(QUIC_SESSION_POOL_JOB_STALE_HOST_TRIED_ON_CONNECTION)

// This event indicates that stale host was not used to try connecting.
EVENT_TYPE(QUIC_SESSION_POOL_JOB_STALE_HOST_NOT_USED_ON_CONNECTION)

// This event indicates that the stale host doesn't match with fresh host.
EVENT_TYPE(QUIC_SESSION_POOL_JOB_STALE_HOST_RESOLUTION_NO_MATCH)

// This event indicates that stale host matches with fresh resolution.
EVENT_TYPE(QUIC_SESSION_POOL_JOB_STALE_HOST_RESOLUTION_MATCHED)

// This event indicates that a QuicSessionPool::Job has created a session.
//
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for session that was used>,
//   }
EVENT_TYPE(QUIC_SESSION_POOL_JOB_RESULT)

// ------------------------------------------------------------------------
// quic::QuicSession
// ------------------------------------------------------------------------

// The start/end of a quic::QuicSession.
//   {
//     "host": <The origin hostname string>,
//     "port": <The origin port>,
//     "connection_id": <The connection ID>,
//     "versions": <The supported QUIC versions>,
//     "require_confirmation": <True if the session will wait for a
//                              successful QUIC handshake before vending
//                              streams>,
//     "cert_verify_flags": <The certificate verification flags for the
//                           session>,
//     "server_id_privacy_mode": <The privacy mode from the server ID>,
//     "privacy_mode": <The privacy mode of the session>,
//     "proxy_chain": <The proxy chain of the session>,
//     "session_usage": <If the session is used for a connection to the
//                       destination or through a proxy>,
//     "network_anonymization_key": <The NetworkAnonymizationKey of the
//                                   session>,
//     "secure_dns_policy": <The policy for Secure DNS usage>,
//     "require_dns_https_alpn": <True if the session requires the alpn value of
//                                the HTTPS DNS record to exist>,
//     "client_connection_id": <optional, The client connection ID if not
//                              empty>,
//     "ech_config_list": <optional, The ECH config list if not empty>,
//     "source_dependency": <Source identifier for the attached Job>,
//   }
EVENT_TYPE(QUIC_SESSION)

// This event is logged for the associated QuicSessionPool::Job to indicate
// that a session is created.
//   {
//     "source_dependency": <Source identifier for the attached session>,
//   }
EVENT_TYPE(QUIC_SESSION_CREATED)

// Session is closing because of an error.
//   {
//     "net_error": <Net error code for the closure>,
//   }
EVENT_TYPE(QUIC_SESSION_CLOSE_ON_ERROR)

// Session verification of a certificate from the server failed.
//   {
//   }
EVENT_TYPE(QUIC_SESSION_CERTIFICATE_VERIFY_FAILED)

// Session verified a certificate from the server.
//   {
//     "subjects": <list of DNS names that the certificate is valid for>,
//   }
EVENT_TYPE(QUIC_SESSION_CERTIFICATE_VERIFIED)

// Session received a QUIC packet.
//   {
//     "peer_address": <The ip:port of the peer>,
//     "self_address": <The local ip:port which received the packet>,
//   }
EVENT_TYPE(QUIC_SESSION_PACKET_RECEIVED)

// Session sent a QUIC packet.
//   {
//     "encryption_level": <The quic::EncryptionLevel of the packet>,
//     "transmission_type": <The quic::TransmissionType of the packet>,
//     "packet_sequence_number": <The packet's full 64-bit sequence number,
//                                as a base-10 string.>,
//     "size": <The size of the packet in bytes>
//   }
EVENT_TYPE(QUIC_SESSION_PACKET_SENT)

// Session retransmitted a QUIC packet.
//   {
//     "old_packet_sequence_number": <The old packet's full 64-bit sequence
//                                    number, as a base-10 string.>,
//     "new_packet_sequence_number": <The new packet's full 64-bit sequence
//                                    number, as a base-10 string.>,
//   }
EVENT_TYPE(QUIC_SESSION_PACKET_RETRANSMITTED)

// Session declared a QUIC packet lost.
//   {
//     "transmission_type": <The quic::TransmissionType of the packet>,
//     "packet_number": <The packet's full 64-bit number as a base-10 string>,
//     "detection_time": <The time at which the packet was declared lost>
//   }
EVENT_TYPE(QUIC_SESSION_PACKET_LOST)

// Congestion control parameters have been configured for the session.
//   {
//     "congestion_control_type": <The name of the CC algorithm used>,
//     "use_pacing": <The bool indicating if pacing is enabled>,
//     "initial_congestion_window": <The size of the initial congestion window>
//   }
EVENT_TYPE(QUIC_CONGESTION_CONTROL_CONFIGURED)

// Session received a QUIC packet with a sequence number that had previously
// been received.
//   {
//     "packet_sequence_number": <The packet's full 64-bit sequence number>
//   }
EVENT_TYPE(QUIC_SESSION_DUPLICATE_PACKET_RECEIVED)

// Session received a QUIC packet header, which has not yet been authenticated.
//   {
//     "connection_id": <The 64-bit CONNECTION_ID for this connection, as a
//                       base-10 string>,
//     "packet_number": <The packet's full 64-bit sequence number,
//                       as a base-10 string.>,
//     "header_format": <The format of the header in string>,
//     If the header_format is long header:
//      "long_header_type": <log the long header type>
//     If Google QUIC is used:
//      "reset_flag": <True if the reset flag is set for this packet>,
//      "version_flag": <True if the version flag is set for this packet>
//   }
EVENT_TYPE(QUIC_SESSION_UNAUTHENTICATED_PACKET_HEADER_RECEIVED)

// Session has authenticated a QUIC packet.
EVENT_TYPE(QUIC_SESSION_PACKET_AUTHENTICATED)

// Session received a STREAM frame.
//   {
//     "stream_id": <The id of the stream which this data is for>,
//     "fin": <True if this is the final data set by the peer on this stream>,
//     "offset": <Offset in the byte stream where this data starts>,
//     "length": <Length of the data in this frame>,
//   }
EVENT_TYPE(QUIC_SESSION_STREAM_FRAME_RECEIVED)

// Session sent a STREAM frame.
//   {
//     "stream_id": <The id of the stream which this data is for>,
//     "fin": <True if this is the final data set by the peer on this stream>,
//     "offset": <Offset in the byte stream where this data starts>,
//     "length": <Length of the data in this frame>,
//   }
EVENT_TYPE(QUIC_SESSION_STREAM_FRAME_SENT)

// A stream frame is coalesced with an existing stream frame.
//   {
//     "stream_id": <The id of the stream which this data is for>,
//     "fin": <True if this is the final data set by the peer on this stream>,
//     "offset": <Offset in the byte stream where this data starts>,
//     "length": <Length of the data in this frame>,
//   }
EVENT_TYPE(QUIC_SESSION_STREAM_FRAME_COALESCED)

// Session received an ACK frame.
//   {
//     "largest_observed": <The largest packet number of a packet the peer has
//       received, a.k.a. Largest Acknowledged>,
//     "smallest_observed": <The smallest packet number covered by this ACK>,
//     "delta_time_largest_observed_us": <Time between when the largest
//       packet number was received and the ACK sent, a.k.a. Ack Delay>,
//     "missing_packets": <List of packet numbers of packets between
//       smallest_observed and largest_observed which were not received>,
//     "received_packet_times": <List of dictionaries containing
//       "packet_number" and "received" timestamps (if enabled)>,
//   }
EVENT_TYPE(QUIC_SESSION_ACK_FRAME_RECEIVED)

// Session sent an ACK frame.
//   {
//     "largest_observed": <The largest packet number of a packet we have
//       received, a.k.a. Largest Acknowledged>,
//     "smallest_observed": <The smallest packet number covered by this ACK>,
//     "delta_time_largest_observed_us": <Time between when the largest
//       packet number was received and the ACK sent, a.k.a. Ack Delay>,
//     "missing_packets": <List of packet numbers of packets between
//       smallest_observed and largest_observed which were not received>,
//     "received_packet_times": <List of dictionaries containing
//       "packet_number" and "received" timestamps (if enabled)>,
//   }
EVENT_TYPE(QUIC_SESSION_ACK_FRAME_SENT)

// Session received a WINDOW_UPDATE frame.
//   {
//     "stream_id": <The id of the stream which this data is for>,
//     "byte_offset": <Byte offset in the stream>,
//   }
EVENT_TYPE(QUIC_SESSION_WINDOW_UPDATE_FRAME_RECEIVED)

// Session sent a WINDOW_UPDATE frame.
//   {
//     "stream_id": <The id of the stream which this data is for>,
//     "byte_offset": <Byte offset in the stream>,
//   }
EVENT_TYPE(QUIC_SESSION_WINDOW_UPDATE_FRAME_SENT)

// Session received a BLOCKED frame.
//   {
//     "stream_id": <The id of the stream which this data is for>,
//   }
EVENT_TYPE(QUIC_SESSION_BLOCKED_FRAME_RECEIVED)

// Session sent a BLOCKED frame.
//   {
//     "stream_id": <The id of the stream which this data is for>,
//   }
EVENT_TYPE(QUIC_SESSION_BLOCKED_FRAME_SENT)

// Session received a GOAWAY frame.
//   {
//     "quic_error":          <quic::QuicErrorCode in the frame>,
//     "last_good_stream_id": <Last correctly received stream id by the server>,
//     "reason_phrase":       <Prose justifying go-away request>,
//   }
EVENT_TYPE(QUIC_SESSION_GOAWAY_FRAME_RECEIVED)

// Session sent a GOAWAY frame.
//   {
//     "quic_error":          <quic::QuicErrorCode in the frame>,
//     "last_good_stream_id": <Last correctly received stream id by the server>,
//     "reason_phrase":       <Prose justifying go-away request>,
//   }
EVENT_TYPE(QUIC_SESSION_GOAWAY_FRAME_SENT)

// Session received a PING frame.
EVENT_TYPE(QUIC_SESSION_PING_FRAME_RECEIVED)

// Session sent a PING frame.
EVENT_TYPE(QUIC_SESSION_PING_FRAME_SENT)

// Session sent an MTU discovery frame (PING on wire).
EVENT_TYPE(QUIC_SESSION_MTU_DISCOVERY_FRAME_SENT)

// Session received a STOP_WAITING frame.
//   {
//     "sent_info": <Details of packet sent by the peer>
//       {
//         "least_unacked": <Lowest sequence number of a packet sent by the peer
//                           for which it has not received an ACK>,
//       }
//   }
EVENT_TYPE(QUIC_SESSION_STOP_WAITING_FRAME_RECEIVED)

// Session sent an STOP_WAITING frame.
//   {
//     "sent_info": <Details of packet sent by the peer>
//       {
//         "least_unacked": <Lowest sequence number of a packet sent by the peer
//                           for which it has not received an ACK>,
//       }
//   }
EVENT_TYPE(QUIC_SESSION_STOP_WAITING_FRAME_SENT)

// Session received a RST_STREAM frame.
//   {
//     "offset": <Offset in the byte stream which triggered the reset>,
//     "quic_rst_stream_error": <quic::QuicRstStreamErrorCode in the frame>,
//     "stream_id": <The stream id which this frame refers to>,
//   }
EVENT_TYPE(QUIC_SESSION_RST_STREAM_FRAME_RECEIVED)

// Session sent a RST_STREAM frame.
//   {
//     "offset": <Offset in the byte stream which triggered the reset>,
//     "quic_rst_stream_error": <quic::QuicRstStreamErrorCode in the frame>,
//     "stream_id": <The stream id which this frame refers to>,
//   }
EVENT_TYPE(QUIC_SESSION_RST_STREAM_FRAME_SENT)

// Session received a CONNECTION_CLOSE frame.
//   {
//     "quic_error": <quic::QuicErrorCode in the frame>,
//     "details": <Human readable description>,
//   }
EVENT_TYPE(QUIC_SESSION_CONNECTION_CLOSE_FRAME_RECEIVED)

// Session received a CONNECTION_CLOSE frame.
//   {
//     "quic_error": <quic::QuicErrorCode in the frame>,
//     "details": <Human readable description>,
//   }
EVENT_TYPE(QUIC_SESSION_CONNECTION_CLOSE_FRAME_SENT)

// Session received a public reset packet.
//   {
//   }
EVENT_TYPE(QUIC_SESSION_PUBLIC_RESET_PACKET_RECEIVED)

// Session received a version negotiation packet.
//   {
//     "versions": <List of QUIC versions supported by the server>,
//   }
EVENT_TYPE(QUIC_SESSION_VERSION_NEGOTIATION_PACKET_RECEIVED)

// Session successfully negotiated QUIC version number.
//   {
//     "version": <String of QUIC version negotiated with the server>,
//   }
EVENT_TYPE(QUIC_SESSION_VERSION_NEGOTIATED)

// Session revived a QUIC packet packet via FEC.
//   {
//     "connection_id": <The 64-bit CONNECTION_ID for this connection, as a
//                       base-10 string>,
//     "public_flags": <The public flags set for this packet>,
//     "packet_sequence_number": <The packet's full 64-bit sequence number,
//                                as a base-10 string.>,
//     "private_flags": <The private flags set for this packet>,
//     "fec_group": <The FEC group of this packet>,
//   }
EVENT_TYPE(QUIC_SESSION_PACKET_HEADER_REVIVED)

// Session received a crypto handshake message.
//   {
//     "quic_crypto_handshake_message": <The human readable dump of the message
//                                       contents>
//   }
EVENT_TYPE(QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_RECEIVED)

// Session sent a crypto handshake message.
//   {
//     "quic_crypto_handshake_message": <The human readable dump of the message
//                                       contents>
//   }
EVENT_TYPE(QUIC_SESSION_CRYPTO_HANDSHAKE_MESSAGE_SENT)

// A QUIC connection received transport parameters.
//   {
//     "quic_transport_parameters": <Human readable view of the transport
//                                   parameters>
//   }
EVENT_TYPE(QUIC_SESSION_TRANSPORT_PARAMETERS_RECEIVED)

// A QUIC connection sent transport parameters.
//   {
//     "quic_transport_parameters": <Human readable view of the transport
//                                   parameters>
//   }
EVENT_TYPE(QUIC_SESSION_TRANSPORT_PARAMETERS_SENT)

// A QUIC connection resumed transport parameters for 0-RTT.
//   {
//     "quic_transport_parameters": <Human readable view of the transport
//                                   parameters>
//   }
EVENT_TYPE(QUIC_SESSION_TRANSPORT_PARAMETERS_RESUMED)

// A WebTransport client is alive.
EVENT_TYPE(QUIC_SESSION_WEBTRANSPORT_CLIENT_ALIVE)

// A WebTransport client state has changed.
//   {
//     "last_state": <The last client state>
//     "next_state": <The next client state>
//     "error": <Optionally, error codes and details when an error happened>
//   }
EVENT_TYPE(QUIC_SESSION_WEBTRANSPORT_CLIENT_STATE_CHANGED)

// A WebTransport session is ready.
//   {
//     "http_datagram_version": <Negotiated HTTP Datagram version>,
//     "webtransport_http3_version": <Negotiated WebTransport over HTTP/3
//      version>
//   }
EVENT_TYPE(QUIC_SESSION_WEBTRANSPORT_SESSION_READY)

// QUIC with TLS gets 0-RTT rejected.
EVENT_TYPE(QUIC_SESSION_ZERO_RTT_REJECTED)

// Records that the QUIC session received a default network change signal.
//   {
//     "new_default_network": <The new default network>
//   }
EVENT_TYPE(QUIC_SESSION_NETWORK_MADE_DEFAULT)

// Records that the QUIC session received a network disconnected signal.
//   {
//     "disconnected_network": <The network which was disconnected>
//   }
EVENT_TYPE(QUIC_SESSION_NETWORK_DISCONNECTED)

// Records that the QUIC session received a network connected signal.
//   {
//     "connected_network": <The network which was connected>
//   }
EVENT_TYPE(QUIC_SESSION_NETWORK_CONNECTED)

// Session was closed, either remotely or by the peer.
//   {
//     "quic_error": <quic::QuicErrorCode which caused the connection to be
//                    closed>,
//     "details": <The error details string in the connection close.>,
//     "from_peer":  <True if the peer closed the connection>
//   }
EVENT_TYPE(QUIC_SESSION_CLOSED)

// Records that QUIC connectivity probe finished on the following path:
//  {
//     "network": <ID of the network probed>
//     "peer address:" <Peer address probed>
//     "is_success:" <Whether the connectivity probe succeeded>
//  }
EVENT_TYPE(QUIC_SESSION_CONNECTIVITY_PROBING_FINISHED)

// Session sent a PATH_CHALLENGE frame.
//  {
//    "data": <The challenge data in base64 string>
//  }
EVENT_TYPE(QUIC_SESSION_PATH_CHALLENGE_FRAME_SENT)

// Session received a PATH_CHALLENGE frame.
//  {
//    "data": <The challenge data in base64 string>
//  }
EVENT_TYPE(QUIC_SESSION_PATH_CHALLENGE_FRAME_RECEIVED)

// Session sent a PATH_RESPONSE frame.
//  {
//    "data": <The response data in base64 string>
//  }
EVENT_TYPE(QUIC_SESSION_PATH_RESPONSE_FRAME_SENT)

// Session received a PATH_RESPONSE frame.
//  {
//    "data": <The response data in base64 string>
//  }
EVENT_TYPE(QUIC_SESSION_PATH_RESPONSE_FRAME_RECEIVED)

// Session sent a CRYPTO frame.
//  {
//    "encryption_level": <The quic::EncryptionLevel of the frame>,
//    "data_length": <The length of the CRYPTO frame data>,
//    "offset": <The offset of the CRYPTO frame>
//  }
EVENT_TYPE(QUIC_SESSION_CRYPTO_FRAME_SENT)

// Session received a CRYPTO frame.
//  {
//    "encryption_level": <The quic::EncryptionLevel of the frame>,
//    "data_length": <The length of the CRYPTO frame data>,
//    "offset": <The offset of the CRYPTO frame>
//  }
EVENT_TYPE(QUIC_SESSION_CRYPTO_FRAME_RECEIVED)

// Session sent a STOP_SENDING frame.
//  {
//    "stream_id": <The stream id>,
//    "rsr_stream_error_code": <quic::QuicRstStreamErrorCode in the frame>
//  }
EVENT_TYPE(QUIC_SESSION_STOP_SENDING_FRAME_SENT)

// Session received a STOP_SENDING frame.
//  {
//    "stream_id": <The stream id>,
//    "rsr_stream_error_code": <Tquic::QuicRstStreamErrorCode in the frame>
//  }
EVENT_TYPE(QUIC_SESSION_STOP_SENDING_FRAME_RECEIVED)

// Session sent a STREAMS_BLOCKED frame.
//  {
//    "stream_count": <The number of streams that the sender wishes to exceed>
//    "unidirectional": <boolean to indicate if the frame is for unidirectional
//    streams.>
//  }
EVENT_TYPE(QUIC_SESSION_STREAMS_BLOCKED_FRAME_SENT)

// Session received a STREAMS_BLOCKED frame.
//  {
//    "stream_count": <The number of streams that the sender wishes to exceed>
//    "unidirectional": <boolean to indicate if the frame is for unidirectional
//    streams.>
//  }
EVENT_TYPE(QUIC_SESSION_STREAMS_BLOCKED_FRAME_RECEIVED)

// Session sent a MAX_STREAMS frame.
//  {
//    "stream_count": <The number of streams that may be opened>
//    "unidirectional": <boolean to indicate if the frame is for unidirectional
//    streams.>
//  }
EVENT_TYPE(QUIC_SESSION_MAX_STREAMS_FRAME_SENT)

// Session received a MAX_STREAMS frame.
//  {
//    "stream_count": <The number of streams that may be opened>
//    "unidirectional": <boolean to indicate if the frame is for unidirectional
//    streams.>
//  }
EVENT_TYPE(QUIC_SESSION_MAX_STREAMS_FRAME_RECEIVED)

// Session sent a PADDING frame.
//  {
//    "num_padding_bytes": <The number of padding bytes>
//  }
EVENT_TYPE(QUIC_SESSION_PADDING_FRAME_SENT)

// Session received a PADDING frame.
//  {
//    "num_padding_bytes": <The number of padding bytes>
//  }
EVENT_TYPE(QUIC_SESSION_PADDING_FRAME_RECEIVED)

// Session sent a NEW_CONNECITON_ID frame.
//  {
//    "connection_id": <The new connection id>
//    "sequencer_number": <Connection id sequence number that specifies the
//    order that connection ids must be used in.>
//    "retire_prior_to": <retire prior to>
//  }
EVENT_TYPE(QUIC_SESSION_NEW_CONNECTION_ID_FRAME_SENT)

// Session received a NEW_CONNECITON_ID frame.
//  {
//    "connection_id": <The new connection id>
//    "sequence_number": <Connection id sequence number that specifies the
//    order that connection ids must be used in.>
//    "retire_prior_to": <retire prior to>
//  }
EVENT_TYPE(QUIC_SESSION_NEW_CONNECTION_ID_FRAME_RECEIVED)

// Session sent a NEW_TOKEN frame.
//  {
//    "token": <String representation of the token>
//  }
EVENT_TYPE(QUIC_SESSION_NEW_TOKEN_FRAME_SENT)

// Session received a NEW_TOKEN frame.
//  {
//    "token": <String representation of the token>
//  }
EVENT_TYPE(QUIC_SESSION_NEW_TOKEN_FRAME_RECEIVED)

// Session sent a RETIRE_CONNECTION_ID frame.
//  {
//    "sequence_number": <Connection id sequence number that specifies the
//    order that connection ids must be used in.>
//  }
EVENT_TYPE(QUIC_SESSION_RETIRE_CONNECTION_ID_FRAME_SENT)

// Session received a RETIRE_CONNECTION_ID frame.
//  {
//    "sequence_number": <Connection id sequence number that specifies the
//    order that connection ids must be used in.>
//  }
EVENT_TYPE(QUIC_SESSION_RETIRE_CONNECTION_ID_FRAME_RECEIVED)

// Session sent a MESSAGE frame.
//  {
//    "message_length": <the length of the message>
//  }
EVENT_TYPE(QUIC_SESSION_MESSAGE_FRAME_SENT)

// Session received a MESSAGE frame.
//  {
//    "message_length": <the length of the message>
//  }
EVENT_TYPE(QUIC_SESSION_MESSAGE_FRAME_RECEIVED)

// Session received a HANDSHAKE_DONE frame.
EVENT_TYPE(QUIC_SESSION_HANDSHAKE_DONE_FRAME_RECEIVED)

// Session received an ACCEPT_CH frame
// {
//   "origin": <the origin the accept_ch settings apply to>
//   "accept_ch": <the raw ACCEPT_CH data>
// }
EVENT_TYPE(QUIC_ACCEPT_CH_FRAME_RECEIVED)

// Session sent a coalesced QUIC packet.
// {
//   "info": <coalesced packet info>
// }
EVENT_TYPE(QUIC_SESSION_COALESCED_PACKET_SENT)

// Session buffered an undecryptable packet.
// {
//   "encryption_level": <the encryption level of the packet>
// }
EVENT_TYPE(QUIC_SESSION_BUFFERED_UNDECRYPTABLE_PACKET)

// Session dropped an undecryptable packet.
// {
//   "encryption_level": <the encryption level of the packet>
// }
EVENT_TYPE(QUIC_SESSION_DROPPED_UNDECRYPTABLE_PACKET)

// Session is attempting to process an undecryptable packet.
// {
//   "encryption_level": <the encryption level of the packet>
// }
EVENT_TYPE(QUIC_SESSION_ATTEMPTING_TO_PROCESS_UNDECRYPTABLE_PACKET)

// Session has updated to the next set of 1-RTT keys.
// {
//   "reason": <the reason the key update was triggered>
// }
EVENT_TYPE(QUIC_SESSION_KEY_UPDATE)

// Session received an ORIGIN frame
// {
//   "origins" : <list of received origins>
EVENT_TYPE(QUIC_SESSION_ORIGIN_FRAME_RECEIVED)

// ------------------------------------------------------------------------
// QuicHttpStream
// ------------------------------------------------------------------------

// Identifies the NetLogSource() for the QuicSession that handled the stream.
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for session that was used>,
//   }
EVENT_TYPE(HTTP_STREAM_REQUEST_BOUND_TO_QUIC_SESSION)

// ------------------------------------------------------------------------
// QuicChromiumClientStream
// ------------------------------------------------------------------------

// The stream is sending the request headers.
//   {
//     "headers": <The list of header:value pairs>
//   }
EVENT_TYPE(QUIC_CHROMIUM_CLIENT_STREAM_SEND_REQUEST_HEADERS)

// The stream has read a 103 Early Hints response headers.
//   {
//     "headers": <The list of header:value pairs>
//   }
EVENT_TYPE(QUIC_CHROMIUM_CLIENT_STREAM_READ_EARLY_HINTS_RESPONSE_HEADERS)

// The stream has read the response headers.
//   {
//     "headers": <The list of header:value pairs>
//   }
EVENT_TYPE(QUIC_CHROMIUM_CLIENT_STREAM_READ_RESPONSE_HEADERS)

// The stream has read the response trailers.
//   {
//     "headers": <The list of header:value pairs>
//   }
EVENT_TYPE(QUIC_CHROMIUM_CLIENT_STREAM_READ_RESPONSE_TRAILERS)

// ------------------------------------------------------------------------
// QuicConnectionMigration
// ------------------------------------------------------------------------
// Records the QUIC connection migration mode.
//  {
//     "connection_migration_mode": <The connection migration mode>
//  }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_MODE)

// Records that QUIC connection migration has been triggered.
//  {
//     "trigger": <The reason for the migration attempt>
//  }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_TRIGGERED)

// Records a failed QUIC connection migration attempt of the session
// identified by connection_id.
//  {
//     "connection_id": <Connection ID of the session>
//     "reason": <String of the failure reason>
//  }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_FAILURE)

// Records a successful QUIC connection migration attempt of the session
// identified by connection_id.
//  {
//     "connection_id": <Connection ID of the session>
//  }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_SUCCESS)

// Records that a QUIC connection migration attempt due to new network
// being connected.
// {
//     "connected_network": <The network we will try to migrate to>
// }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_ON_NETWORK_CONNECTED)

// Records that a QUIC connection migration attempt due to new network
// being marked as default network.
// {
//     "new_default_network": <The new default network>
// }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_ON_NETWORK_MADE_DEFAULT)

// Records that a QUIC connection migration attempt due to old network
// being disconnected.
// {
//     "disconnected_network": <The network which was disconnected>
// }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_ON_NETWORK_DISCONNECTED)

// Records that a QUIC connection migration attempt due to encountering
// packet write error on the current network.
// {
//     "network": <Current network where we encountered the write error>
// }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_ON_WRITE_ERROR)

// Records that no alternate networks were available after either the default
// network was or a we encountered a packet write error.
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_WAITING_FOR_NEW_NETWORK)

// Records that a QUIC connection migration attempt due to path
// degrading on the current network.
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_ON_PATH_DEGRADING)

// Records that a QUIC connection migration attempt due to efforts to
// migrate back to the default network.
// {
//     "retry_count": <Number of attempts to migrate back so far>
// }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_ON_MIGRATE_BACK)

// Records a QUIC connection migration failure after probing.
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_FAILURE_AFTER_PROBING)

// Records a QUIC connection migration success after probing.
// {
//     "migrate_to_network": <The network the probe was successful for>
// }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_SUCCESS_AFTER_PROBING)

// Records a QUIC connection migration timeout while waiting for a new network.
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_FAILURE_WAITING_FOR_NETWORK)

// Records that a new network was found before QUIC connection migration timed
// out.
// {
//     "network": <The network that was found and we will try to migrate to>
// }
EVENT_TYPE(QUIC_CONNECTION_MIGRATION_SUCCESS_WAITING_FOR_NETWORK)

// ------------------------------------------------------------------------
// QuicConnectivityProbingManager
// ------------------------------------------------------------------------

// Records that QUIC connectivity probing manager starts probing for the
// following path:
//  {
//     "network": <ID of the network being probed>
//     "peer_address": <Peer address being probed>
//     "initial_timeout_ms": <Initial timeout in milliseconds>
//  }
EVENT_TYPE(QUIC_CONNECTIVITY_PROBING_MANAGER_START_PROBING)

// Records that QUIC connectivity probing manager cancels probing for the
// following path:
//  {
//     "network": <ID of the network being probed>
//     "peer_address": <Peer address being probed>
//  }
EVENT_TYPE(QUIC_CONNECTIVITY_PROBING_MANAGER_CANCEL_PROBING)

// Records that QUIC connectivity probing manager sends a probe.
//  {
//     "retry_count": <Number of the trial to send probe>
//  }
EVENT_TYPE(QUIC_CONNECTIVITY_PROBING_MANAGER_PROBE_SENT)

// Records that QUIC connectivity probing manager receives a probe on the
// following path:
//  {
//     "network": <ID of the network being probed>
//     "self_address": <Self address on the probed path>
//     "peer_address": <Peer address on the probed path>
//  }
EVENT_TYPE(QUIC_CONNECTIVITY_PROBING_MANAGER_PROBE_RECEIVED)

// Records that QUIC connectivity probing manager receives STATLESS_RESET on the
// following path:
//  {
//     "network": <ID of the network being probed>
//     "self_address": <Self address on the probed path>
//     "peer_address": <Peer address on the probed path>
//  }
EVENT_TYPE(QUIC_CONNECTIVITY_PROBING_MANAGER_STATELESS_RESET_RECEIVED)

// ------------------------------------------------------------------------
// QuicPortMigration
// ------------------------------------------------------------------------

// Records that QUIC port migration has been triggered.
EVENT_TYPE(QUIC_PORT_MIGRATION_TRIGGERED)

// Records a failed QUIC port migration attempt of the session identified
// by connection_id.
//  {
//     "connection_id": <Connection ID of the session>
//     "reason": <String of the failure reason>
//  }
EVENT_TYPE(QUIC_PORT_MIGRATION_FAILURE)

// Records a successful QUIC port migration attempt of the session
// identified by connection_id.
//  {
//     "connection_id": <Connection ID of the session>
//  }
EVENT_TYPE(QUIC_PORT_MIGRATION_SUCCESS)

// ------------------------------------------------------------------------
// QuicServerPreferredAddress
// ------------------------------------------------------------------------

// Records that client has received server preferred address.
EVENT_TYPE(QUIC_ON_SERVER_PREFERRED_ADDRESS_AVAILABLE)

// Records that client starts to validate received server preferred address.
EVENT_TYPE(QUIC_START_VALIDATING_SERVER_PREFERRED_ADDRESS)

// Records client fails to migrate to server preferred address of the session
// identified by connection_id.
//  {
//     "connection_id": <Connection ID of the session>
//     "reason": <String of the failure reason>
//  }
EVENT_TYPE(QUIC_FAILED_TO_VALIDATE_SERVER_PREFERRED_ADDRESS)

// Records client successfully migrates to server preferred address of the
// session identified by connection_id.
//  {
//     "connection_id": <Connection ID of the session>
//  }
EVENT_TYPE(QUIC_SUCCESSFULLY_MIGRATED_TO_SERVER_PREFERRED_ADDRESS)

// A UDP socket error occurred while trying to read a QUIC packet.
// The following parameters are attached to the event:
//   {
//     "net_error": <Integer code for the specific error type>,
//   }
EVENT_TYPE(QUIC_READ_ERROR)

// ------------------------------------------------------------------------
// HttpStreamParser
// ------------------------------------------------------------------------

// Measures the time to read HTTP response headers from the server.
EVENT_TYPE(HTTP_STREAM_PARSER_READ_HEADERS)

// ------------------------------------------------------------------------
// SOCKS5ClientSocket
// ------------------------------------------------------------------------

// The time spent sending the "greeting" to the SOCKS server.
EVENT_TYPE(SOCKS5_GREET_WRITE)

// The time spent waiting for the "greeting" response from the SOCKS server.
EVENT_TYPE(SOCKS5_GREET_READ)

// The time spent sending the CONNECT request to the SOCKS server.
EVENT_TYPE(SOCKS5_HANDSHAKE_WRITE)

// The time spent waiting for the response to the CONNECT request.
EVENT_TYPE(SOCKS5_HANDSHAKE_READ)

// ------------------------------------------------------------------------
// HTTP Authentication
// ------------------------------------------------------------------------
//
// Structure of common GSSAPI / SSPI values.
// -----------------------------------------
// For convenience some structured GSSAPI/SSPI values are serialized
// consistently across different events. They are explained below.
//
// ** GSSAPI Status:
//
// A major/minor status code returned by a GSSAPI function. The major status
// code indicates the GSSAPI level error, while the minor code provides a
// mechanism specific error code if a specific GSSAPI mechanism was involved in
// the error.
//
// The status value has the following structure:
//   {
//     "function": <name of GSSAPI function that returned the error>
//     "major_status": {
//       "status" : <status value as a number>,
//       "message": [
//          <list of strings hopefully explaining what that number means>
//       ]
//     },
//     "minor_status": {
//       "status" : <status value as a number>,
//       "message": [
//          <list of strings hopefully explaining what that number means>
//       ]
//     }
//   }
//
// ** OID:
//
// An ASN.1 OID that's used for GSSAPI is serialized thusly:
//   {
//     "oid":    <symbolic name of OID if it is known>
//     "length": <length in bytes of serialized OID>,
//     "bytes":  <serialized bytes of OID encoded via NetLogBinaryValue()>
//   }
//
// ** GSS Display Name:
//
// A serialization of GSSAPI principal name to something that can be consumed by
// humans. If the encoding of the string is not UTF-8 (since there's no
// requirement that they use any specific encoding) the field is serialized
// using NetLogBinaryValue().
//   {
//     "name" : <GSSAPI principal name>
//     "type" : <OID indicating type of name. See OID above.>
//     "error": <If the display name lookup operation failed, then this field
//               contains the error in the form of a GSSAPI Status.>
//   }
//
// ** GSSAPI Context Flags
//
// Bitmask indicating properties of the negotiated security context. Values may
// be only advisory if the "open" flag of the enclosing security context is
// True. I.e. flags are not final until the security context is closed.
//
//   {
//     "flags"     : <Flags. See RFC 2744 Section 5.19 for meanings. Flag
//                    bitmasks can be found in RFC 2744 Appendix A.>
//     "delegated" : <True if credentials were delegated to the target.>
//     "mutual"    : <True if mutual authentication was successful.>
//   }
//
// ** GSSAPI Context Description
//
// Properties of the GSSAPI security context being negotiated or that was
// negotiated.
//
//   {
//     "source"   : <GSS Display Name for the source of the authentication
//                   attempt. In practice this is always the user's identity.>
//     "target"   : <GSS Display Name for the target of the authentication
//                   attempt. This the target server or proxy service
//                   principal.>
//     "open"     : <Boolean indicating whether the context is |open|, which
//                   means that the handshake is still in progress. In
//                   particular, the flags, lifetime, and mechanism fields are
//                   not considered final until |open| is false.
//     "lifetime" : <A decimal string indicating the lifetime in seconds of the
//                   authentication context. The identity as established by this
//                   handshake is only valid for this long since the time at
//                   which it was established.>
//     "mechanism": <OID indicating inner authentication mechanism.>
//     "flags"    : <GSSAPI Context Flags.>
//   }
//
// ** SSPI SECURITY_STATUS
//
// SSPI functions invoked during Negotiate authentication on Windows return
// SECURITY_STATUS values. These values are documented alongside the functions
// that return them. Of these //net uses AcquireCredentialsHandle, and
// InitializeSecurityContext.
//
//   {
//     "net_error" : <net::Error value corresponding to the |security_status|
//                    value.>
//     "security_status": <The |SECURITY_STATUS| value indicating the result of
//                    the operation.>
//   }
//
// ** SSPI Context Flags
//
// Bitmask indicating properties of the negotiated security context. Values may
// be only advisory if the |open| flag of the enclosing security context is
// True. I.e. flags are not final until the security context is closed.
//
//   {
//     "flags"     : <Bitmask in hexadecimal. See documentation for
//                    QueryContextAttributes for a description of the flags.>
//     "delegated" : <True if credentials were delegated to the target.>
//     "mutual"    : <True if mutual authentication was successful.>
//   }
//
// ** SSPI Context Description
//
// Properties of the SSPI security context being negotiated or that was
// negotiated.
//
//   {
//     "source"    : <Source security principal. I.e. the user's identity.>
//     "target"    : <Target server's security principal name.>
//     "open"      : <True if the handshake is complete. Does not imply success
//                    or failure.>
//     "mechanism" : <SSPI security package name for the selected mechanism.>
//     "flags"     : <SSPI Context Flags.>
//   }

// Lifetime event for HttpAuthController.
//
// The BEGIN phase has the following parameters:
//  {
//      "target": <Either "proxy" or "server">,
//      "url": <URL of authentication target>
//  }
EVENT_TYPE(AUTH_CONTROLLER)

// Records on the caller's NetLog to indicate the HttpAuthController that's
// servicing the request.
//  {
//      "source_dependency": <Source ID of HttpAuthController>
//  }
EVENT_TYPE(AUTH_BOUND_TO_CONTROLLER)

// Record the result of attempting to create a HttpAuthHandler.
//
//  {
//      "scheme": <scheme>
//      "net_error": <Net Error. Only present in case of error.>
//      "challenge": <challenge string, if NetLogCaptureIncludesSensitive>
//      "origin": <the origin of the server or proxy which issued the challenge>
//      "allows_default_credentials": <whether the default credentials may be
//                                     used for the origin>
//  }
EVENT_TYPE(AUTH_HANDLER_CREATE_RESULT)

// Record the initialization of an HttpAuthHandler derivative.
//
// The END phase has the following parameters:
//  {
//       "succeeded": <bool indicating whether the initialization succeeded>
//       "allows_default_credentials": whether the default credentials may be
//                                     used for the `origin` passed into
//                                    `InitFromChallenge`.
//  }
EVENT_TYPE(AUTH_HANDLER_INIT)

// Records the invocation and completion of a single token generation operation.
//
// The END phase has the following parameters:
//  {
//       "net_error": <Net Error. Only present in case of error.>
//  }
EVENT_TYPE(AUTH_GENERATE_TOKEN)

// Records the invocation and completion of HandleAuthChallenge operation.
//
// Parameters:
//  {
//       "authorization_result": <One of "accept", "reject", "stale", "invalid",
//                                "different_realm" depending on the outcome of
//                                the handling the challenge>
//  }
EVENT_TYPE(AUTH_HANDLE_CHALLENGE)

// An attempt was made to load an authentication library.
//
// If the request succeeded, the parameters are:
//   {
//     "library_name": <Name of library>
//   }
// Otherwise, the parameters are:
//   {
//     "library_name": <Name of library>
//     "load_error": <An error string>
//   }
EVENT_TYPE(AUTH_LIBRARY_LOAD)

// A required method was not found while attempting to load an authentication
// library.
//
// Parameters are:
//   {
//     "library_name": <Name of the library where the method lookup failed>
//     "method": <Name of method that was not found>
//   }
EVENT_TYPE(AUTH_LIBRARY_BIND_FAILED)

// Construction of the GSSAPI service principal name.
//
// Parameters:
//   {
//     "spn": <Service principal name as a string>
//     "status":  <GSSAPI Status. See GSSAPI Status above. This is field is only
//                 logged if the operation failed.>
//   }
EVENT_TYPE(AUTH_LIBRARY_IMPORT_NAME)

// Invocation of SSPI AcquireCredentialsHandle.
//
// The END phase has the following parameters:
//   {
//     "domain": <Domain of user>
//     "user"  : <Username>
//     "status": <SSPI SECURITY_STATUS value.>
//   }
//
// Note that "domain" and "user" are only present if the authentication
// handshake is using explicit credentials. In the case of ambient credentials
// the user identity is not known until the handshake is complete.
EVENT_TYPE(AUTH_LIBRARY_ACQUIRE_CREDS)

// Initialize security context.
//
// This operation involves invoking an external library which may perform disk,
// IPC, and network IO as a part of its work.
//
// On Windows, the BEGIN phase has the following parameters:
//   {
//     "spn": <Service Principle Name>,
//     "context_flags": <Integer with bitfield value>
//   }
//
// The END phase has the following parameters.
//
// On Posix platforms:
//   {
//     "context": <GSSAPI Context Description>,
//     "status" : <GSSAPI Status if the operation failed>
//   }
//
// On Windows:
//   {
//     "context": <SSPI Context Description>
//     "status" : <SSPI SECURITY_STATUS>
//   }
EVENT_TYPE(AUTH_LIBRARY_INIT_SEC_CTX)

// The channel bindings generated for the connection.
//  {
//     "token": <Hex encoded RFC 5929 'tls-server-endpoint' channel binding
//               token. Could be empty if one could not be generated (e.g.
//               because the underlying channel was not TLS.)>
//  }
EVENT_TYPE(AUTH_CHANNEL_BINDINGS)

// ------------------------------------------------------------------------
// Global events
// ------------------------------------------------------------------------
// These are events which are not grouped by source id, as they have no
// context.

// This event is emitted whenever NetworkChangeNotifier determines that an
// active network adapter's IP address has changed.
EVENT_TYPE(NETWORK_IP_ADDRESSES_CHANGED)

// This event is emitted whenever NetworkChangeNotifier determines that an
// active network adapter's connectivity status has changed.
//   {
//     "new_connection_type": <Type of the new connection>
//   }
EVENT_TYPE(NETWORK_CONNECTIVITY_CHANGED)

// This event is emitted whenever NetworkChangeNotifier determines that a change
// occurs to the host computer's hardware or software that affects the route
// network packets take to any network server.
//   {
//     "new_connection_type": <Type of the new connection>
//   }
EVENT_TYPE(NETWORK_CHANGED)

// This event is emitted whenever the macOS's dynamic store entries of network
// interface related keys (SCEntNetInterface, SCEntNetIPv4, and SCEntNetIPv6)
// has been changed.
//   {
//     "result":              <Whether to notify as IP address changed>,
//     "net_ipv4_key":        <Boolean indicating whether SCEntNetIPv4 entry has
//                             been changed>,
//     "net_ipv6_key":        <Boolean indicating whether SCEntNetIPv6 entry has
//                             been changed>,
//     "net_interface_key":   <Boolean indicating whether SCEntNetInterface
//                             entry has been changed>,
//     "reduce_notification": <Boolean indicating whether
//                             ReduceIPAddressChangeNotification feature is
//                             enabled>,
//     "old_ipv4_interface":  <The IPv4 primary interface name obtained before
//                             the dynamic store entry change event>,
//     "old_ipv6_interface":  <The IPv6 primary interface name obtained before
//                             the dynamic store entry change event>,
//     "new_ipv4_interface":  <The IPv4 primary interface name obtained after
//                             the dynamic store entry change event>,
//     "new_ipv6_interface":  <The IPv6 primary interface name obtained after
//                             the dynamic store entry change event>,
//     "old_interfaces":      <The list of network interfaces obtained before
//                             the dynamic store entry change event>,
//     "new_interfaces":      <The list of network interfaces obtained after
//                             the dynamic store entry change event>
//   }
EVENT_TYPE(NETWORK_MAC_OS_CONFIG_CHANGED)

// This event is emitted whenever DnsClient receives a new DnsConfig or
// DnsConfigOverrides.
//   {
//     "nameservers":                <List of name server IPs>,
//     "search":                     <List of domain suffixes>,
//     "num_hosts":                  <Number of entries in the HOSTS file>,
//     <other>:                      <See DnsConfig>
//   }
EVENT_TYPE(DNS_CONFIG_CHANGED)

// This event is emitted whenever NetworkChangeNotifier determines that a
// network has connected and network-specific information is available
// (i.e. the handles::NetworkHandle of the network is known).
//   {
//     "changed_network_handle":        <Network handle>
//     "changed_network_type":          <Type of network>
//     "default_active_network_handle": <Network handle of default network>
//     "current_active_networks":       <Mapping from network handle to network
//                                       type, containing entries for all active
//                                       networks.>
//   }
EVENT_TYPE(SPECIFIC_NETWORK_CONNECTED)

// This event is emitted whenever NetworkChangeNotifier determines that a
// network has disconnected and network-specific information is available
// (i.e. the handles::NetworkHandle of the network is known).
//   {
//     "changed_network_handle":        <Network handle>
//     "changed_network_type":          <Type of network>
//     "default_active_network_handle": <Network handle of default network>
//     "current_active_networks":       <Mapping from network handle to network
//                                       type, containing entries for all active
//                                       networks.>
//   }
EVENT_TYPE(SPECIFIC_NETWORK_DISCONNECTED)

// This event is emitted whenever NetworkChangeNotifier determines that a
// network is soon to disconnect and network-specific information is available
// (i.e. the handles::NetworkHandle of the network is known).
//   {
//     "changed_network_handle":        <Network handle>
//     "changed_network_type":          <Type of network>
//     "default_active_network_handle": <Network handle of default network>
//     "current_active_networks":       <Mapping from network handle to network
//                                       type, containing entries for all active
//                                       networks.>
//   }
EVENT_TYPE(SPECIFIC_NETWORK_SOON_TO_DISCONNECT)

// This event is emitted whenever NetworkChangeNotifier determines that a
// network has become the default and network-specific information is available
// (i.e. the handles::NetworkHandle of the network is known).
//   {
//     "changed_network_handle":        <Network handle>
//     "changed_network_type":          <Type of network>
//     "default_active_network_handle": <Network handle of default network>
//     "current_active_networks":       <Mapping from network handle to network
//                                       type, containing entries for all active
//                                       networks.>
//   }
EVENT_TYPE(SPECIFIC_NETWORK_MADE_DEFAULT)

// This event is emitted whenever CertDatabase determines that the certificate
// database has changed.
EVENT_TYPE(CERTIFICATE_DATABASE_TRUST_STORE_CHANGED)
EVENT_TYPE(CERTIFICATE_DATABASE_CLIENT_CERT_STORE_CHANGED)

// This event is logged when a request to conditionally clear the cached client
// certificate for a specific host has been received. It contains the following
// parameters:
// {
//    "host": <Serialized scheme/host/port of the request>,
//    "certificates": <A list of PEM encoded certificates, the first one
//                    being the new client certificate and the remaining
//                    being intermediate certificates. May be an empty
//                    list if no client certificate should be cached.>,
//    "is_cleared": <boolean>,
// }
EVENT_TYPE(CLEAR_CACHED_CLIENT_CERT)

// This event is logged when a request to conditionally clear the cached client
// certificate matching a given certificate has been received. It contains the
// following parameters:
// {
//    "hosts": <A list of serialized scheme/host/port which had a matching
//             cached client certificate.>,
//    "certificates": <A list of PEM encoded certificates, the first one
//                    being the client certificate being matched and the
//                    remaining being its certificate chain.>,
// }
EVENT_TYPE(CLEAR_MATCHING_CACHED_CLIENT_CERT)

// ------------------------------------------------------------------------
// Exponential back-off throttling events
// ------------------------------------------------------------------------

// Emitted when back-off is disabled for a given host, or the first time
// a localhost URL is used (back-off is always disabled for localhost).
//   {
//     "host": <The hostname back-off was disabled for>
//   }
EVENT_TYPE(THROTTLING_DISABLED_FOR_HOST)

// Emitted when a request is denied due to exponential back-off throttling.
//   {
//     "url":              <URL that was being requested>,
//     "num_failures":     <Failure count for the URL>,
//     "release_after_ms": <Number of milliseconds until URL will be unblocked>
//   }
EVENT_TYPE(THROTTLING_REJECTED_REQUEST)

// ------------------------------------------------------------------------
// DnsTransaction
// ------------------------------------------------------------------------

// The start/end of a DnsTransaction.
//
// The BEGIN phase contains the following parameters:
//
// {
//   "hostname": <The hostname it is trying to resolve>,
//   "query_type": <Type of the query>,
// }
//
// The END phase contains the following parameters:
//
// {
//   "net_error": <The net error code for the failure, if any>,
// }
EVENT_TYPE(DNS_TRANSACTION)

// The start/end of a DnsTransaction query for a fully-qualified domain name.
//
// The BEGIN phase contains the following parameters:
//
// {
//   "qname": <The fully-qualified domain name it is trying to resolve>,
// }
//
// The END phase contains the following parameters:
//
// {
//   "net_error": <The net error code for the failure, if any>,
// }
EVENT_TYPE(DNS_TRANSACTION_QUERY)

// This event is created when DnsTransaction creates a new UDP socket and
// tries to resolve the fully-qualified name.
//
// It has a single parameter:
//
//   {
//     "source_dependency": <Source id of the UDP socket created for the
//                           attempt>,
//   }
EVENT_TYPE(DNS_TRANSACTION_ATTEMPT)

// This event is created when DnsTransaction creates a new TCP socket and
// tries to resolve the fully-qualified name.
//
// It has a single parameter:
//
//   {
//     "source_dependency": <Source id of the TCP socket created for the
//                           attempt>,
//   }
EVENT_TYPE(DNS_TRANSACTION_TCP_ATTEMPT)

// This event is created when DnsTransaction creates a new DoH request and
// tries to resolve the fully-qualified name.
//
// It has a single parameter:
//
//   {
//     "source_dependency": <Source id of the DoH request created for the
//                           attempt>,
//   }
EVENT_TYPE(DNS_TRANSACTION_HTTPS_ATTEMPT)

// This event is created when DnsTransaction receives a matching response.
//
// It has the following parameters:
//
//   {
//     "rcode": <rcode in the received response>,
//     "answer_count": <answer_count in the received response>,
//     "additional_answer_count": <additional_answer_count in the received
//                                 response>,
//     "source_dependency": <Source id of the UDP socket that received the
//                           response>,
//     "response_buffer": <Raw buffer of the received response>,
//   }
EVENT_TYPE(DNS_TRANSACTION_RESPONSE)

// The start/end of a DoH request.
//
// The BEGIN phase contains the following parameters:
//
// {
//   "hostname": <The hostname it is trying to resolve>,
//   "query_type": <Type of the query>,
// }
//
// The END phase contains the following parameters:
//
// {
//   "net_error": <The net error code for the failure, if any>,
// }
EVENT_TYPE(DOH_URL_REQUEST)

// ------------------------------------------------------------------------
// CertVerifier
// ------------------------------------------------------------------------

// This event is created when we start a CertVerifier request.
EVENT_TYPE(CERT_VERIFIER_REQUEST)

// This event is created when we start a CertVerifier job.
// The BEGIN phase event parameters are:
// {
//   "certificates": <A list of PEM encoded certificates, the first one
//                    being the certificate to verify and the remaining
//                    being intermediate certificates to assist path
//                    building.>
//   "ocsp_response": <Optionally, a PEM encoded stapled OCSP response.>
//   "sct_list": <Optionally, a PEM encoded SignedCertificateTimestampList.>
//   "host": <The hostname verification is being performed for.>
//   "verifier_flags": <The CertVerifier::VerifyFlags.>
// }
//
// The END phase event parameters are:
//   {
//     "cert_status": <Bitmask of CERT_STATUS_*
//                     from net/base/cert_status_flags.h>
//     "has_md2": <True if a certificate in the certificate chain is signed with
//                 a MD2 signature.>
//     "has_md4": <True if a certificate in the certificate chain is signed with
//                 a MD4 signature.>
//     "has_md5": <True if a certificate in the certificate chain is signed with
//                 a MD5 signature.>
//     "is_issued_by_additional_trust_anchor": <True if the root CA used for
//                                              this verification came from the
//                                              list of additional trust
//                                              anchors.>
//     "is_issued_by_known_root": <True if we recognise the root CA as a
//                                 standard root.  If it isn't then it's
//                                 probably the case that this certificate
//                                 was generated by a MITM proxy whose root
//                                 has been installed locally. This is
//                                 meaningless if the certificate was not
//                                 trusted.>
//     "net_error": <net error code returned from the verification.>
//     "public_key_hashes": <If the certificate was successfully verified then
//                           this contains the hashes, in several hash
//                           algorithms, of the SubjectPublicKeyInfos of the
//                           chain.>
//     "verified_cert": <The certificate chain that was constructed
//                       during verification. Note that though the verified
//                       certificate will match the originally supplied
//                       certificate, the intermediate certificates stored
//                       within may be substantially different. In the event
//                       of a verification failure, this will contain the
//                       chain as supplied by the server. This may be NULL
//                       if running within the sandbox.>
//   }
EVENT_TYPE(CERT_VERIFIER_JOB)

// This event is created when a CertVerifier request attaches to a job.
//
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for the job we are bound to>,
//   }
EVENT_TYPE(CERT_VERIFIER_REQUEST_BOUND_TO_JOB)

// This event is created when a CertVerifier task is about to be posted.
EVENT_TYPE(CERT_VERIFIER_TASK)

// This event is created when a CertVerifier task is created for a job.
//
// The event parameters are:
//   {
//      "source_dependency": <Source identifier for the job we are bound to>,
//   }
EVENT_TYPE(CERT_VERIFIER_TASK_BOUND)

// This event is created when a CertVerifyProc instance is created.
EVENT_TYPE(CERT_VERIFY_PROC_CREATED)

// This event is created when CertVerifyProc is verifying a certificate.
// The BEGIN phase event parameters are:
// {
//   "certificates": <A list of PEM encoded certificates, the first one
//                    being the certificate to verify and the remaining
//                    being intermediate certificates to assist path
//                    building.>
//   "crlset_sequence": <Sequence number of the CRLSet.>
//   "crlset_is_expired": <Boolean indicating whether the CRLSet is considered
//                         expired.>
//   "ocsp_response": <Optionally, a PEM encoded stapled OCSP response.>
//   "sct_list": <Optionally, a PEM encoded SignedCertificateTimestampList.>
//   "host": <The hostname verification is being performed for.>
//   "verify_flags": <The CertVerifyProc::VerifyFlags.>
// }
//
// The END phase event parameters are:
//   {
//     "cert_status": <Bitmask of CERT_STATUS_*
//                     from net/base/cert_status_flags.h>
//     "has_md2": <True if a certificate in the certificate chain is signed with
//                 a MD2 signature.>
//     "has_md4": <True if a certificate in the certificate chain is signed with
//                 a MD4 signature.>
//     "has_md5": <True if a certificate in the certificate chain is signed with
//                 a MD5 signature.>
//     "is_issued_by_additional_trust_anchor": <True if the root CA used for
//                                              this verification came from the
//                                              list of additional trust
//                                              anchors.>
//     "is_issued_by_known_root": <True if we recognise the root CA as a
//                                 standard root.  If it isn't then it's
//                                 probably the case that this certificate
//                                 was generated by a MITM proxy whose root
//                                 has been installed locally. This is
//                                 meaningless if the certificate was not
//                                 trusted.>
//     "net_error": <net error code returned from the verification.>
//     "public_key_hashes": <If the certificate was successfully verified then
//                           this contains the hashes, in several hash
//                           algorithms, of the SubjectPublicKeyInfos of the
//                           chain.>
//     "verified_cert": <The certificate chain that was constructed
//                       during verification. Note that though the verified
//                       certificate will match the originally supplied
//                       certificate, the intermediate certificates stored
//                       within may be substantially different. In the event
//                       of a verification failure, this will contain the
//                       chain as supplied by the server. This may be NULL
//                       if running within the sandbox.>
//   }
EVENT_TYPE(CERT_VERIFY_PROC)

// This event is created for the target cert passed into CertVerifyProcBulitin.
// The event parameters are:
//   {
//      "certificate": <The PEM encoded certificate.>
//      "errors": <Optionally, a string describing any errors or warnings
//                 encountered while parsing the certificate.>
//   }
EVENT_TYPE(CERT_VERIFY_PROC_TARGET_CERT)

// This event is created for each additional certificate passed into
// CertVerifyProcBuiltin.
// The parameters are the same as for CERT_VERIFY_PROC_TARGET_CERT.
EVENT_TYPE(CERT_VERIFY_PROC_INPUT_CERT)

// This event is created to log the Chrome Root Store version used
// by CertVerifyProcBuiltin.
// The event parameters are:
//   {
//      "version_major": <The major version of the Chrome Root Store>
//   }
EVENT_TYPE(CERT_VERIFY_PROC_CHROME_ROOT_STORE_VERSION)

// This event is created for each additional certificate added to
// CertVerifyProcBuiltin.
// The event parameters are:
//   {
//      "certificate": <The PEM encoded certificate.>
//      "spki": <The SPKI that this applies to>
//      "trust": <The trust setting used for this certificate.>
//      "errors": <Optionally, a string describing any errors or warnings
//                 encountered while parsing the certificate.>
//   }
// Only one of certificate or spki will be provided, never both.
EVENT_TYPE(CERT_VERIFY_PROC_ADDITIONAL_CERT)

// This event is created for each path building attempt performed by
// CertVerifyProcBuiltin.
// The BEGIN phase contains the following information:
// {
//      "digest_policy": <Specifies which digest methods are accepted in this
//                        attempt.>
//      "is_ev_attempt": <True if this is an EV verification attempt.>
//      "is_network_time_attempt": <True if this attempt used the network time.>
//      "network_time_value": <Int - time in milliseconds since the unix epoch,
//                             only populated if is_network_time_attempt is
//                             true.>
// }
//
// The END phase contains the following information:
// {
//      "best_result_index": <Index of the best path that was found.>
//      "has_valid_path": <True if one of the paths found was valid and
//                         trusted.>
//      "exceeded_iteration_limit": <True if the iteration limit was exceeded
//                                   during path building.>
//      "exceeded_deadline": <True if the time limit was exceeded during path
//                            building.>
// }
EVENT_TYPE(CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT)

// This event is created for each path built during a
// CERT_VERIFY_PROC_PATH_BUILD_ATTEMPT.
//
// The event parameters are:
//   {
//      "certificates": <The certificate chain that was built.>
//      "is_valid": <True if the chain is valid and trusted.>
//      "last_cert_trust": <The trust setting found for the last cert in
//                          the chain.>
//      "errors": <Optionally, a string describing any errors or warnings
//                 encountered while building or verifying the path.>
//   }
EVENT_TYPE(CERT_VERIFY_PROC_PATH_BUILT)

// This event is created whenever a debugging message is sent from the path
// builder.
// parameters:
// {
//    "path_builder_debug": <String - message sent from the path builder>
// }
EVENT_TYPE(CERT_VERIFY_PROC_PATH_BUILDER_DEBUG)

// -----------------------------------------------------------------------------
// FTP events.
// -----------------------------------------------------------------------------

// This event is created when an FTP command is sent. It contains following
// parameters:
//   {
//     "command": <String - the command sent to remote server>
//   }
EVENT_TYPE(FTP_COMMAND_SENT)

// This event is created when FTP control connection is made. It contains
// following parameters:
//   {
//     "source_dependency": <id of log for control connection socket>
//   }
EVENT_TYPE(FTP_CONTROL_CONNECTION)

// This event is created when FTP data connection is made. It contains
// following parameters:
//   {
//     "source_dependency": <id of log for data connection socket>
//   }
EVENT_TYPE(FTP_DATA_CONNECTION)

// This event is created when FTP control connection response is processed.
// It contains following parameters:
//   {
//     "lines": <list of strings - each representing a line of the response>
//     "status_code": <numeric status code of the response>
//   }
EVENT_TYPE(FTP_CONTROL_RESPONSE)

// -----------------------------------------------------------------------------
// Simple Cache events.
// -----------------------------------------------------------------------------

// This event lasts the lifetime of a Simple Cache entry.
// It contains the following parameter:
//   {
//     "entry_hash": <hash of the entry, formatted as a hex string>
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY)

// This event is created when the entry's key is set.
// It contains the following parameter:
//   {
//     "key": <key of the entry>
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_SET_KEY)

// This event is created when OpenEntry is called.  It has no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_OPEN_CALL)

// This event is created when the Simple Cache actually begins opening the
// cache entry.  It has no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_OPEN_BEGIN)

// This event is created when the Simple Cache finishes the OpenEntry call.
// It contains the following parameter:
// {
//   "net_error": <net error code returned from the call>
// }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_OPEN_END)

// This event is created when CreateEntry is called.  It has no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CREATE_CALL)

// This event is created when the Simple Cache optimistically returns a result
// from a CreateEntry call before it performs the create operation.
// It contains the following parameter:
// {
//   "net_error": <net error code returned from the call>
// }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CREATE_OPTIMISTIC)

// This event is created when the Simple Cache actually begins creating the
// cache entry.  It has no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CREATE_BEGIN)

// This event is created when the Simple Cache finishes the CreateEntry call.
// It contains the following parameter:
// {
//   "net_error": <net error code returned from the call>
// }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CREATE_END)

// This event is created when OpenOrCreateEntry is called.  It has no
// parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_CALL)

// This event is created when the Simple Cache actually begins open/create of
// the cache entry.  It has no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_BEGIN)

// This event is created when the Simple Cache finishes the OpenOrCreateEntry
// call. It contains the following parameter:
// {
//   "net_error": <net error code returned from the call>
// }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_OPEN_OR_CREATE_END)

// This event is created when ReadData is called.
// It contains the following parameters:
//   {
//     "index": <Index being read/written>,
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_READ_CALL)

// This event is created when the Simple Cache actually begins reading data
// from the cache entry.
// It contains the following parameters:
//   {
//     "index": <Index being read/written>,
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_READ_BEGIN)

// This event is created when the Simple Cache finishes a ReadData call.
// It contains the following parameters:
//   {
//     "bytes_copied": <Number of bytes copied.  Not present on error>,
//     "net_error": <Network error code.  Only present on error>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_READ_END)

// This event is created when the Simple Cache begins to verify the checksum of
// cached data it has just read.  It occurs before READ_END, and contains no
// parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CHECKSUM_BEGIN)

// This event is created when the Simple Cache finishes verifying the checksum
// of cached data.  It occurs after CHECKSUM_BEGIN but before READ_END, and
// contains one parameter:
// {
//   "net_error": <net error code returned from the internal checksum call>
// }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CHECKSUM_END)

// This event is created when WriteData is called.
// It contains the following parameters:
//   {
//     "index": <Index being read/written>,
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//     "truncate": <If present for a write, the truncate flag is set to true.
//                  Not present in reads or writes where it is false>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_WRITE_CALL)

// This event is created when the Simple Cache optimistically returns a result
// from a WriteData call before it performs the write operation.
// It contains the following parameters:
//   {
//     "bytes_copied": <Number of bytes copied.  Not present on error>,
//     "net_error": <Network error code.  Only present on error>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_WRITE_OPTIMISTIC)

// This event is created when the Simple Cache actually begins writing data to
// the cache entry.
// It contains the following parameters:
//   {
//     "index": <Index being read/written>,
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//     "truncate": <If present for a write, the truncate flag is set to true.
//                  Not present in reads or writes where it is false>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_WRITE_BEGIN)

// This event is created when the Simple Cache finishes a WriteData call.
// It contains the following parameters:
//   {
//     "bytes_copied": <Number of bytes copied.  Not present on error>,
//     "net_error": <Network error code.  Only present on error>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_WRITE_END)

// This event is created when ReadSparseData is called.
// It contains the following parameters:
//   {
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//     "truncate": <If present for a write, the truncate flag is set to true.
//                  Not present in reads or writes where it is false>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_READ_SPARSE_CALL)

// This event is created when the Simple Cache actually begins reading sparse
// data from the cache entry.
// It contains the following parameters:
//   {
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//     "truncate": <If present for a write, the truncate flag is set to true.
//                  Not present in reads or writes where it is false>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_READ_SPARSE_BEGIN)

// This event is created when the Simple Cache finishes a ReadSparseData call.
// It contains the following parameters:
//   {
//     "bytes_copied": <Number of bytes copied.  Not present on error>,
//     "net_error": <Network error code.  Only present on error>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_READ_SPARSE_END)

// This event is created when WriteSparseData is called.
// It contains the following parameters:
//   {
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_WRITE_SPARSE_CALL)

// This event is created when the Simple Cache actually begins writing sparse
// data to the cache entry.
// It contains the following parameters:
//   {
//     "offset": <Offset being read/written>,
//     "buf_len": <Length of buffer being read to/written from>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_WRITE_SPARSE_BEGIN)

// This event is created when the Simple Cache finishes a WriteSparseData call.
// It contains the following parameters:
//   {
//     "bytes_copied": <Number of bytes copied.  Not present on error>,
//     "net_error": <Network error code.  Only present on error>,
//   }
EVENT_TYPE(SIMPLE_CACHE_ENTRY_WRITE_SPARSE_END)

// This event is created when DoomEntry is called.  It contains no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_DOOM_CALL)

// This event is created when the Simple Cache actually starts dooming a cache
// entry.  It contains no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_DOOM_BEGIN)

// This event is created when the Simple Cache finishes dooming an entry.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_DOOM_END)

// This event is created when CloseEntry is called.  It contains no parameters.
// A Close call may not result in CLOSE_BEGIN and CLOSE_END if there are still
// more references to the entry remaining.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CLOSE_CALL)

// This event is created when the Simple Cache actually starts closing a cache
// entry.  It contains no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CLOSE_BEGIN)

// This event is created when the Simple Cache finishes a CloseEntry call.  It
// contains no parameters.
EVENT_TYPE(SIMPLE_CACHE_ENTRY_CLOSE_END)

// Marks start of UploadDataStream that is logged on initialization.
// The END phase contains the following parameters:
// {
//   "net_error": <Result of the initialization step>,
//   "total_size": <Shows total content length>,
//   "is_chunked": <Shows whether data is chunked or not>
// }
EVENT_TYPE(UPLOAD_DATA_STREAM_INIT)

// The start/end of UploadDataStream::Read method.
//
// The BEGIN phase contains the following information:
// {
//   "current_position": <Shows current read position>,
// }
//
// The END phase contains the following information:
// {
//   "result": <Result of reading. Result > 0 is bytes read. Result == 0 means
//              the end of file. Result < 0 means an error.>
// }
EVENT_TYPE(UPLOAD_DATA_STREAM_READ)

// -----------------------------------------------------------------------------
// ResourceScheduler related events
// -----------------------------------------------------------------------------

// The ResourceScheduler has started a previously blocked request.  Parameters:
// {
//   "trigger": <Trigger for evaluation that caused request start>
// }
EVENT_TYPE(RESOURCE_SCHEDULER_REQUEST_STARTED)

// -----------------------------------------------------------------------------
// Auxiliary network service in-memory HTTP cache related events
// -----------------------------------------------------------------------------

// These event are emitted when HTTP response headers are served from the
// in-memory cache.
// The following parameters are attached:
//   {
//     "headers": <The list of header:value pairs>,
//   }
EVENT_TYPE(IN_MEMORY_CACHE_READ_REQUEST_HEADERS)
EVENT_TYPE(IN_MEMORY_CACHE_READ_RESPONSE_HEADERS)

// This event is emitted when response content are read from the in-memory
// cache.
// The following parameters are attached:
//   {
//     "byte_count": <Number of bytes that were just sent>,
//     "bytes": <The exact bytes sent, Base64 encoded>,
//   }
EVENT_TYPE(IN_MEMORY_CACHE_BYTES_READ)

// -----------------------------------------------------------------------------
// Network Quality Estimator related events
// -----------------------------------------------------------------------------

// This event is emitted whenever NetworkQualityEstimator determines that the
// quality of the network has changed.
// parameters:
//  {
//    "http_rtt": <Current estimate of the HTTP RTT>,
//    "transport_rtt": <Current estimate of the transport RTT>,
//    "downstream_throughput": <Current estimate of the downstream throughput>,
//    "effective_connection_type": <Current estimate of the effective connection
//                                  type>,
//  }
EVENT_TYPE(NETWORK_QUALITY_CHANGED)

// -----------------------------------------------------------------------------
// Http Server Properties Manager related events
// -----------------------------------------------------------------------------

// This event is emitted when HttpServerPropertiesManager initialization starts
// and finishes.
EVENT_TYPE(HTTP_SERVER_PROPERTIES_INITIALIZATION)

// This event is emitted when HttpServerPropertiesManager is updating in-memory
// version of HttpServerProperties from the serialized version from perfs/disk.
// parameters:
//  {
//    "servers": <List of servers and their protocol usage information>,
//    "supports_quic": <Local IP addresses that used QUIC>,
//    "version": <The version number>,
//  }
EVENT_TYPE(HTTP_SERVER_PROPERTIES_UPDATE_CACHE)

// This event is emitted when HttpServerPropertiesManager is persisting
// in-memory version of HttpServerProperties to prefs/disk.
// parameters:
//  {
//    "servers": <List of servers and their protocol usage information>,
//    "supports_quic": <Local IP addresses that used QUIC>,
//    "version": <The version number>,
//  }
EVENT_TYPE(HTTP_SERVER_PROPERTIES_UPDATE_PREFS)

// -----------------------------------------------------------------------------
// HostCachePersistenceManager related events
// -----------------------------------------------------------------------------

// The start/end of getting the persisted HostCache value and restoring it.
// The END phase contains the following parameters:
//  {
//    "success": <Whether the persisted HostCache was restored successfully>,
//  }
EVENT_TYPE(HOST_CACHE_PREF_READ)

// This event is created when the HostCachePersistenceManager writes the cache
// contents to prefs.
EVENT_TYPE(HOST_CACHE_PREF_WRITE)

// This event is created when the HostCachePersistenceManager starts the timer
// for writing a cache change to prefs.
EVENT_TYPE(HOST_CACHE_PERSISTENCE_START_TIMER)

// -----------------------------------------------------------------------------
// DHCP-based WPAD (Windows)
// -----------------------------------------------------------------------------

// The start/end of running DHCP based WPAD.
//
// The start event contains no parameters, whereas the END event describes
// which of the "adapter fetchers" was used:
//  {
//    "fetcher_index": <Index of the fetcher that "won" the race, or -1 if no
//                     fetcher won>,
//    "net_error": <The network error code for the overall result of DHCP
//                  based auto-discovery>,
//  }
EVENT_TYPE(WPAD_DHCP_WIN_FETCH)

// The start/end of getting the list of network adapters.
//
// The END event describes all the adapters that were enumerated, as well
// as how long it took to do the various thread-hops (from origin to worker
// thread, and then worker thread back to origin thread):
//  {
//    "adapters": <List describing each adapter (its name, flags, and
//                 status)>,
//    "origin_to_worker_thread_hop_dt": <The time in milliseconds it took
//                                       for the worker thread task to get
//                                       scheduled>,
//    "worker_to_origin_thread_hop_dt": <The time in milliseconds it took
//                                       for the reply task from worker
//                                       thread to get scheduled>,
//    "worker_dt": <The time in milliseconds it took to enumerate network
//                  adapters on the worker thread>,
//    "error": <The result code returned by iphlpapi!GetAdaptersAddresses>
//  }
EVENT_TYPE(WPAD_DHCP_WIN_GET_ADAPTERS)

// This event logs when one of the "adapter fetchers" completed. (Fetchers
// may not complete in the order that they were started):
//  {
//    "fetcher_index": <Index of the fetcher that completed>,
//    "net_error": <The network error code returned by the fetcher>,
//  }
EVENT_TYPE(WPAD_DHCP_WIN_ON_FETCHER_DONE)

// This event is logged when a timer is started to timeout remaining
// adapter fetchers. The event has no parameters.
EVENT_TYPE(WPAD_DHCP_WIN_START_WAIT_TIMER)

// This event is emitted if the wait timer for remaining fetchers fires. It
// has no parameters.
EVENT_TYPE(WPAD_DHCP_WIN_ON_WAIT_TIMER)

// -----------------------------------------------------------------------------
// CookieStore related events
// -----------------------------------------------------------------------------

// Event emitted on store creation/deletion
//  {
//    "persistent_store": <Whether there is an attached persistent store>,
//  }
EVENT_TYPE(COOKIE_STORE_ALIVE)

// Event emitted on cookie addition
//  {
//    "name": <Name of the cookie added>
//    "value": <Value of the cookie added>
//    "domain": <Domain of the cookie added>
//    "path": <Path of the cookie added>
//    "is_persistent": <Whether or not the cookie is persistent>
//    "sync_requested": <Whether sync to the backing store was requested>
//  }
EVENT_TYPE(COOKIE_STORE_COOKIE_ADDED)

// Event emitted on cookie deletion
//  {
//    "name": <Name of the cookie added>
//    "value": <Value of the cookie added>
//    "domain": <Domain of the cookie added>
//    "path": <Path of the cookie added>
//    "deletion_cause": <Reason the cookie was deleted>
//    "httponly": <httponly field of the cookie>
//    "secure": <If the cookie is a secure cookie>
//    "priority": <priority of the cookie>
//    "samesite": <SameSite setting for the cookie>
//    "is_persistent": <Whether or not the cookie is persistent>
//    "sync_requested": <Whether sync to the backing store was requested>
//  }
EVENT_TYPE(COOKIE_STORE_COOKIE_DELETED)

// Event emitted on rejection of a cookie addition because of a conflict
// with a secure cookie that would have been deleted.
//  {
//    "name": <Name of the cookies>
//    "domain": <Domain of the cookies>
//    "oldpath": <Path of the cookie that would have been deleted>
//    "newpath": <Path of the cookie that would have been added>
//    "oldvalue": <Value of the cookie that would have been deleted>
//    "newvalue": <Value of the cookie that would have been added>
//  }
EVENT_TYPE(COOKIE_STORE_COOKIE_REJECTED_SECURE)

// Event emitted on rejection of a cookie addition because of a conflict
// with an httponly cookie.
//  {
//    "name": <Name of the cookies>
//    "domain": <Domain of the cookies>
//    "path": <Path of the cookies>
//    "oldvalue": <Value of the cookie that would have been deleted>
//    "newvalue": <Value of the cookie that would have been added>
//  }
EVENT_TYPE(COOKIE_STORE_COOKIE_REJECTED_HTTPONLY)

// Event emitted on preservation of a cookie that would have been
// overwritten, because cookie addition failed due to a conflict with a secure
// cookie.
//  {
//    "name": <Name of the cookies>
//    "domain": <Domain of the preserved and new cookies>
//    "path": <Path of the preserved and new cookies>
//    "securecookiedomain": <Domain of the secure cookie causing preservation>
//    "securecookiepath": <Path of the secure cookie causing preservation>
//    "preservedvalue": <Value of the preserved cookie>
//    "discardedvalue": <Value of the new cookie whose addition failed>
//  }
EVENT_TYPE(COOKIE_STORE_COOKIE_PRESERVED_SKIPPED_SECURE)

// Event emitted on setting store session persistence
//  {
//    "persistence" : <Session persistence setting for the store>
//  }
EVENT_TYPE(COOKIE_STORE_SESSION_PERSISTENCE)

// Event emitted when a particular origin is removed from the persistent
// store on shutdown.
//  {
//    "origin": <Origin being filtered>
//    "is_https": <Secure status of origin>
//  }
EVENT_TYPE(COOKIE_PERSISTENT_STORE_ORIGIN_FILTERED)

// Event emitted when the persistent database load is started and completed.
//  {
//  }
EVENT_TYPE(COOKIE_PERSISTENT_STORE_LOAD)

// Event emitted when load for a particular key is started.
//  {
//    "key": <Key to be loaded>
//  }
EVENT_TYPE(COOKIE_PERSISTENT_STORE_KEY_LOAD_STARTED)

// Event emitted when load for a particular key is completed.
//  {
//    "key": <Key to be loaded>
//  }
EVENT_TYPE(COOKIE_PERSISTENT_STORE_KEY_LOAD_COMPLETED)

// Event emitted when a persistent store has been closed.
//  {
//    "type": <Classname of persistent cookie store>
//  }
EVENT_TYPE(COOKIE_PERSISTENT_STORE_CLOSED)

// Event emitted when getting cookies is blocked by a NetworkDelegate.
//  {
//  }
EVENT_TYPE(COOKIE_GET_BLOCKED_BY_NETWORK_DELEGATE)

// Event emitted when setting cookies is blocked by a NetworkDelegate.
//  {
//  }
EVENT_TYPE(COOKIE_SET_BLOCKED_BY_NETWORK_DELEGATE)

// Event emitted when a cookie is received but not stored, or when a cookie is
// not sent to an associated domain.
//  {
//    "status": <Exclusion flags>,
//    "name": <Name of the cookie>,
//    "domain": <Domain of the cookie>,
//    "path": <Path of the cookie>,
//    "partition_key": <partition key of the cookie, if any>
//    "operation": <Operation, either "send" or "store">
//  }
EVENT_TYPE(COOKIE_INCLUSION_STATUS)

// -----------------------------------------------------------------------------
// HTTP/3 events.
// -----------------------------------------------------------------------------

// Event emitted when locally-initiated HTTP/3 control stream is created.
//  {
//    "stream_id": <The stream id of locally created control stream>
//  }
EVENT_TYPE(HTTP3_LOCAL_CONTROL_STREAM_CREATED)

// Event emitted when locally-initiated QPACK encoder stream is created.
//  {
//    "stream_id": <The stream id of locally created QPACK encoder stream>
//  }
EVENT_TYPE(HTTP3_LOCAL_QPACK_ENCODER_STREAM_CREATED)

// Event emitted when locally-initiated QPACK decoder stream is created.
//  {
//    "stream_id": <The stream id of locally created QPACK decoder stream>
//  }
EVENT_TYPE(HTTP3_LOCAL_QPACK_DECODER_STREAM_CREATED)

// Event emitted when peer created HTTP/3 control stream type is received.
//  {
//    "stream_id": <The stream id of peer created control stream>
//  }
EVENT_TYPE(HTTP3_PEER_CONTROL_STREAM_CREATED)

// Event emitted when peer created QPACK encoder stream type is received.
//  {
//    "stream_id": <The stream id of the peer created QPACK encoder stream>
//  }
EVENT_TYPE(HTTP3_PEER_QPACK_ENCODER_STREAM_CREATED)

// Event emitted when peer created QPACK decoder stream type is received.
//  {
//    "stream_id": <The stream id of the peer created QPACK decoder stream>
//  }
EVENT_TYPE(HTTP3_PEER_QPACK_DECODER_STREAM_CREATED)

// Event emitted when an HTTP/3 CANCEL_PUSH frame is received.
//  {
//    "push_id": <The push_id field of the CANCEL_PUSH frame>
//  }
EVENT_TYPE(HTTP3_CANCEL_PUSH_RECEIVED)

// Event emitted when SETTINGS frame is received.
// A list of settings will be logged by
// <setting identifier>: <setting value>
EVENT_TYPE(HTTP3_SETTINGS_RECEIVED)

// Event emitted when an HTTP/3 GOAWAY frame is received.
//  {
//    "stream_id": <The stream_id field of the GOAWAY frame>
//  }
EVENT_TYPE(HTTP3_GOAWAY_RECEIVED)

// Event emitted when an HTTP/3 MAX_PUSH_ID frame is received.
//  {
//    "push_id": <The push_id field of the MAX_PUSH_ID frame>
//  }
EVENT_TYPE(HTTP3_MAX_PUSH_ID_RECEIVED)

// Event emitted when an HTTP/3 PRIORITY_UPDATE frame is received.
//  {
//    "type": <The prioritized element type field of the PRIORITY_UPDATE frame>
//    "prioritized_element_id": <The prioritized_element_id field of the
//                               PRIORITY_UPDATE frame>
//    "priority_field_value": <The priority_field_value field of the
//                             PRIORITY_UPDATE frame>
//  }
EVENT_TYPE(HTTP3_PRIORITY_UPDATE_RECEIVED)

// Event emitted when an HTTP/3 DATA frame header is received.
//  {
//    "stream_id": <The ID of the stream on which the DATA frame is received>
//    "payload_length": <The length of DATA frame payload>
//  }
EVENT_TYPE(HTTP3_DATA_FRAME_RECEIVED)

// Event emitted when the receipt of an HTTP/3 HEADERS frame is complete.
//  {
//    "stream_id": <The ID of the stream on which the HEADERS frame is received>
//    "payload_length": <The total number of payload bytes received>
//  }
EVENT_TYPE(HTTP3_HEADERS_RECEIVED)

// Event emitted when headers received in an HTTP/3 HEADERS frame are decoded.
//  {
//    "stream_id": <The ID of the stream on which the HEADERS frame had been
//                  received>
//    "headers": <A dictionary of the decoded headers>
//  }
EVENT_TYPE(HTTP3_HEADERS_DECODED)

// Event emitted when the frame header of an HTTP/3 frame of unknown type is
// received.
//  {
//    "stream_id": <The ID of the stream on which the frame is received>
//    "frame_type": <The frame type>
//    "payload_length": <The length of the frame payload>
//  }
EVENT_TYPE(HTTP3_UNKNOWN_FRAME_RECEIVED)

// Event emitted when an HTTP/3 SETTINGS frame is sent.
// A list of settings will be logged by
// <setting identifier>: <setting value>
EVENT_TYPE(HTTP3_SETTINGS_SENT)

// Event emitted when an HTTP/3 SETTINGS frame is resumed for 0-RTT.
// A list of settings will be logged by
// <setting identifier>: <setting value>
EVENT_TYPE(HTTP3_SETTINGS_RESUMED)

// Event emitted when an HTTP/3 GOAWAY frame is sent.
//  {
//    "stream_id": <The stream_id field of the GOAWAY frame>
//  }
EVENT_TYPE(HTTP3_GOAWAY_SENT)

// Event emitted when an HTTP/3 MAX_PUSH_ID frame is sent.
//  {
//    "push_id": <The push_id field of the MAX_PUSH_ID frame>
//  }
EVENT_TYPE(HTTP3_MAX_PUSH_ID_SENT)

// Event emitted when an HTTP/3 PRIORITY_UPDATE frame is sent.
//  {
//    "type": <The prioritized element type field of the PRIORITY_UPDATE frame>
//    "prioritized_element_id": <The prioritized_element_id field of the
//                               PRIORITY_UPDATE frame>
//    "priority_field_value": <The priority_field_value field of the
//                             PRIORITY_UPDATE frame>
//  }
EVENT_TYPE(HTTP3_PRIORITY_UPDATE_SENT)

// Event emitted when an HTTP/3 DATA frame is sent.
//  {
//    "stream_id": <The ID of the stream on which the frame is sent>
//    "payload_length": <The total payload length of the frame>
//  }
EVENT_TYPE(HTTP3_DATA_SENT)

// Event emitted when an HTTP/3 HEADERS frame is sent.
//  {
//    "stream_id": <The ID of the stream on which the frame is sent>
//    "headers": <A dictionary of the headers sent>
//  }
EVENT_TYPE(HTTP3_HEADERS_SENT)

// Event emitted when an HTTP/3 PUSH_PROMISE frame is sent.
//  {
//    "stream_id": <The ID of the stream on which the frame is sent>
//    "push_id": <The push_id field of the PUSH_PROMISE frame>
//    "headers": <A dictionary of the headers sent>
//  }
EVENT_TYPE(HTTP3_PUSH_PROMISE_SENT)

// -----------------------------------------------------------------------------
// Trust Tokens-related events
// -----------------------------------------------------------------------------

// Event emitted when a request with an associated Trust Tokens operation
// reaches the net stack (TrustTokenRequestHelperFactory).
//
// The BEGIN event contains the operation type:
// {
//  "Operation type (mojom.TrustTokenOperationType)": <The operation type>
// }
//
// The END event contains a human-readable explanation of whether creating a
// Trust Tokens helper (i.e., moving on to the bulk of the operation-specific
// logic) succeeded or failed:
// {
//  "outcome": <human-readable explanation of the outcome>
// }
EVENT_TYPE(TRUST_TOKEN_OPERATION_REQUESTED)

// For each of the BEGIN/FINALIZE event type pairs below:
//
// The BEGIN event type's BEGIN event is emitted when a request with an
// associated Trust Tokens operation starts the "Begin" (outbound) phase of the
// operation. Its END event contains a description of why the outbound phase of
// the operation succeeded or failed:
// {
//  "outcome": <human-readable explanation of the outcome>
// }
//
// The FINALIZE event type is the analogue for the "Finalize" (inbound) phase of
// the operation.
//
// BEGIN_ISSUANCE's BEGIN event is the only event containing a dynamic parameter
// populated with data from an operation (rather than just a success-or-error
// description). On success, it contains the number of signed tokens that the
// issuance operation yielded.
// {
//  "outcome": <human-readable explanation of the outcome>,
//  "# tokens obtained": <number of tokens>
// }
EVENT_TYPE(TRUST_TOKEN_OPERATION_BEGIN_ISSUANCE)
EVENT_TYPE(TRUST_TOKEN_OPERATION_FINALIZE_ISSUANCE)

EVENT_TYPE(TRUST_TOKEN_OPERATION_BEGIN_REDEMPTION)
EVENT_TYPE(TRUST_TOKEN_OPERATION_FINALIZE_REDEMPTION)

EVENT_TYPE(TRUST_TOKEN_OPERATION_BEGIN_SIGNING)

// -----------------------------------------------------------------------------
// CORS preflight events
// -----------------------------------------------------------------------------

// The start/end of CORS preflight request. It corresponds with the lifetime of
// CorsURLLoader.
//
// The BEGIN phase contains the following parameters.
// See network.mojom.ResourceRequest for details.
//
//  {
//    "cors_preflight_policy" : <A policy to decide if CORS-preflight fetch
//                              should be performed>,
//    "headers" : <The list of header:value pairs>,
//    "is_revalidating": <Boolean indicating whether request is revalidating>,
//    "method": <The method ("POST" or "GET" or "HEAD" etc...)>,
//    "url": <The URL to create a request for>
//  }
EVENT_TYPE(CORS_REQUEST)

// This event is logged when CorsURLLoader judges if preflight request is
// required. If required, the reason of judgement is recorded next.
// It contains the following parameter:
//  {
//    "preflight_required": <Boolean indicating whether preflight request is
//                          required>,
//    "preflight_required_reason": <The reason why preflight is required>
//  }
EVENT_TYPE(CHECK_CORS_PREFLIGHT_REQUIRED)

// This event is logged when PreflightController checks preflight cache.
// It contains the following parameter:
//  {
//    "status": <The result of cache checking. "hit-and-pass",
//              "hit-and-fail", "miss" or "stale">
//  }
EVENT_TYPE(CHECK_CORS_PREFLIGHT_CACHE)

// This event is logged when PreflightController gets CORS preflight result
// from preflight request.
// It contains the following parameter:
//  {
//    "access-control-allow-headers": <List of headers given in
//                                    `Access-Control-Allow-Headers`>,
//    "access-control-allow-methods": <List of methods given in
//                                    `Access-Control-Allow-Methods`>
//  }
EVENT_TYPE(CORS_PREFLIGHT_RESULT)

// This event is logged when PreflightController detects a failed CORS
// preflight.
//
// It contains the following parameters:
//  {
//    "error: <A string representing the network error for the failure.
//            "ERR_FAILED" for CORS errors.>,
//    "cors-error": <Optional. An integer representing the more granular reason
//                  for the CORS error, if any. Values map to
//                  `network::mojom::CorsError`.>,
//    "failed-parameter": <Optional, absent if `cors-error` is absent. A string
//                        representing the parameter that failed validation,
//                        e.g. a forbidden header.>,
//  }
EVENT_TYPE(CORS_PREFLIGHT_ERROR)

// This event identifies the NetLogSource() for a URLRequest of the preflight
// request.
EVENT_TYPE(CORS_PREFLIGHT_URL_REQUEST)

// This event is logged when PreflightController gets CORS preflight result
// from preflight cache.
// The parameters are the same as for CORS_PREFLIGHT_RESULT.
EVENT_TYPE(CORS_PREFLIGHT_CACHED_RESULT)

// ------------------------------------------------------------------------
// Private Network Access
// ------------------------------------------------------------------------

// This event is logged when a new connection is checked against Private
// Network Access rules.
//
// It contains the following parameters:
//  {
//    "client_address_space": <the IP address space of the request client>,
//    "resource_address_space": <the IP address space of the remote endpoint>,
//    "result": <the result of the check>,
//  }
//
// If the result is "unexpected-private-network", then the request is
// interrupted and a preflight request is retried, this time with PNA headers
// attached. If this second connection fails the check again, the request is
// failed.
EVENT_TYPE(PRIVATE_NETWORK_ACCESS_CHECK)

// ------------------------------------------------------------------------
// Initiator
// ------------------------------------------------------------------------

// This event is logged to indicate the initiator of the network event.
// The event contains the following parameters:
//   {
//     "source_dependency": <Source identifier for the attached event>
//   }
EVENT_TYPE(CREATED_BY)

// This event is logged when a URLRequestHttpJob's privacy mode is computed. It
// contains the following parameter:
// {
//    "privacy_mode": <Privacy mode associated with the request>,
// }
EVENT_TYPE(COMPUTED_PRIVACY_MODE)

// ------------------------------------------------------------------------
// WebSocket
// ------------------------------------------------------------------------

// This event is logged when an error occurs during WebSocket handshake. It
// contains the following parameters:
// {
//    "net_error": <The number showing network error type>,
//    "message": <Failure message>,
// }
EVENT_TYPE(WEBSOCKET_UPGRADE_FAILURE)

// This event is logged when the WebSocket read buffer size is changed. It
// contains the following parameters:
// {
//    "read_buffer_size_in_bytes": <New read buffer size in bytes>,
// }
EVENT_TYPE(WEBSOCKET_READ_BUFFER_SIZE_CHANGED)

// This event is logged to show the received frame header information. It
// contains the following parameters:
// {
//    "final": <Whether it is the last fragment in a message>,
//    "reserved1": <Whether any extension is defined>,
//    "reserved2": <Whether any extension is defined>,
//    "reserved3": <Whether any extension is defined>,
//    "opcode": <Opcode in the frame header>,
//    "masked": <Whether the message is encoded>,
//    "payload_length": <Payload length in the frame header>,
// }
EVENT_TYPE(WEBSOCKET_RECV_FRAME_HEADER)

// This event is logged to show the sent frame header information. It
// contains the following parameters:
// {
//    "final": <Whether it is the last fragment in a message>,
//    "reserved1": <Whether any extension is defined>,
//    "reserved2": <Whether any extension is defined>,
//    "reserved3": <Whether any extension is defined>,
//    "opcode": <Opcode in the frame header>,
//    "masked": <Whether the message is encoded>,
//    "payload_length": <Payload length in the frame header>,
// }
EVENT_TYPE(WEBSOCKET_SENT_FRAME_HEADER)

// This event is logged when the browser closes the connection instead of the
// server.
EVENT_TYPE(WEBSOCKET_CLOSE_TIMEOUT)

// This event is logged when the WebSocket frame is wrong or weird and the
// browser closes the connection. It contains the following parameters:
// {
//    "code": <WebSocket close code based on
//    https://datatracker.ietf.org/doc/html/rfc6455#section-7.1.5>,
//    "reason":<WebSocket close reason based on
//    https://datatracker.ietf.org/doc/html/rfc6455#section-7.1.6>,
//    "internal_reason": <Detailed reason>,
// }
EVENT_TYPE(WEBSOCKET_INVALID_FRAME)

// This event is logged at TransportSecurityState::ShouldUpgradeToSSL.
// The following parameters are attached:
//   {
//     "host": <host name>
//     "get_sts_state_result": <boolean>,
//     "should_upgrade_to_ssl": <boolean>,
//     "host_found_in_hsts_bypass_list": <boolean>,
//   }
EVENT_TYPE(TRANSPORT_SECURITY_STATE_SHOULD_UPGRADE_TO_SSL)

// ------------------------------------------------------------------------
// Oblivious HTTP
// ------------------------------------------------------------------------

// OBLIVIOUS_HTTP_REQUEST measures the time between when Oblivoius HTTP
// has begun and when Oblivious HTTP has ended (either success or failure).
// The following parameters are attached:
//   {
//     "net_error": <The net error code integer, can be a failure or a success>,
//     "outer_response_error_code": <The HTTP error code of the outer relay HTTP
//     response, set iff the result fails because the outer HTTP response status
//     code is not HTTP_OK>,
//     "inner_response_code": <The HTTP code of the inner gateway HTTP response,
//     parsed out from the binary HTTP structure>
//   }
EVENT_TYPE(OBLIVIOUS_HTTP_REQUEST)

// OBLIVIOUS_HTTP_REQUEST_DATA logs either just the byte count of the request or
// the entire request (depending on capture settings), before encryption.
EVENT_TYPE(OBLIVIOUS_HTTP_REQUEST_DATA)

// OBLIVIOUS_HTTP_RESPONSE_DATA logs either just the byte count of the response
// or the entire response (depending on capture settings), after decryption.
EVENT_TYPE(OBLIVIOUS_HTTP_RESPONSE_DATA)

// OBLIVIOUS_HTTP_RESPONSE_HEADERS logs headers of the response, after
// decryption.
EVENT_TYPE(OBLIVIOUS_HTTP_RESPONSE_HEADERS)

// This event is logged when First-Party Sets metadata is requested/received.
// The following parameters are attached to the "end" event:
//   {
//     "cache_filter": <string>,
//     "frame_entry": <string>,
//     "top_frame_entry": <string>,
//   }
EVENT_TYPE(FIRST_PARTY_SETS_METADATA)
