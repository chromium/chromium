// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A base class for the toy client, which connects to a specified port and sends
// QUIC request to that endpoint.

#ifndef NET_THIRD_PARTY_QUIC_TOOLS_QUIC_CLIENT_BASE_H_
#define NET_THIRD_PARTY_QUIC_TOOLS_QUIC_CLIENT_BASE_H_

#include <string>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quic/core/http/quic_client_push_promise_index.h"
#include "net/third_party/quic/core/http/quic_spdy_client_session.h"
#include "net/third_party/quic/core/http/quic_spdy_client_stream.h"
#include "net/third_party/quic/core/quic_config.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

class ProofVerifier;
class QuicServerId;

// QuicClientBase handles establishing a connection to the passed in
// server id, including ensuring that it supports the passed in versions
// and config.
// Subclasses derived from this class are responsible for creating the
// actual QuicSession instance, as well as defining functions that
// create and run the underlying network transport.
class QuicClientBase {
 public:
  // An interface to various network events that the QuicClient will need to
  // interact with.
  class NetworkHelper {
   public:
    virtual ~NetworkHelper();

    // Runs one iteration of the event loop.
    virtual void RunEventLoop() = 0;

    // Used during initialization: creates the UDP socket FD, sets socket
    // options, and binds the socket to our address.
    virtual bool CreateUDPSocketAndBind(QuicSocketAddress server_address,
                                        QuicIpAddress bind_to_address,
                                        int bind_to_port) = 0;

    // Unregister and close all open UDP sockets.
    virtual void CleanUpAllUDPSockets() = 0;

    // If the client has at least one UDP socket, return address of the latest
    // created one. Otherwise, return an empty socket address.
    virtual QuicSocketAddress GetLatestClientAddress() const = 0;

    // Creates a packet writer to be used for the next connection.
    virtual QuicPacketWriter* CreateQuicPacketWriter() = 0;
  };

  QuicClientBase(const QuicServerId& server_id,
                 const ParsedQuicVersionVector& supported_versions,
                 const QuicConfig& config,
                 QuicConnectionHelperInterface* helper,
                 QuicAlarmFactory* alarm_factory,
                 std::unique_ptr<NetworkHelper> network_helper,
                 std::unique_ptr<ProofVerifier> proof_verifier);
  QuicClientBase(const QuicClientBase&) = delete;
  QuicClientBase& operator=(const QuicClientBase&) = delete;

  virtual ~QuicClientBase();

  // Initializes the client to create a connection. Should be called exactly
  // once before calling StartConnect or Connect. Returns true if the
  // initialization succeeds, false otherwise.
  virtual bool Initialize();

  // "Connect" to the QUIC server, including performing synchronous crypto
  // handshake.
  bool Connect();

  // Start the crypto handshake.  This can be done in place of the synchronous
  // Connect(), but callers are responsible for making sure the crypto handshake
  // completes.
  void StartConnect();

  // Calls session()->Initialize(). Subclasses may override this if any extra
  // initialization needs to be done. Subclasses should expect that session()
  // is non-null and valid.
  virtual void InitializeSession();

  // Disconnects from the QUIC server.
  void Disconnect();

  // Returns true if the crypto handshake has yet to establish encryption.
  // Returns false if encryption is active (even if the server hasn't confirmed
  // the handshake) or if the connection has been closed.
  bool EncryptionBeingEstablished();

  // Wait for events until the stream with the given ID is closed.
  void WaitForStreamToClose(QuicStreamId id);

  // Wait for events until the handshake is confirmed.
  // Returns true if the crypto handshake succeeds, false otherwise.
  bool WaitForCryptoHandshakeConfirmed() WARN_UNUSED_RESULT;

  // Wait up to 50ms, and handle any events which occur.
  // Returns true if there are any outstanding requests.
  bool WaitForEvents();

  // Migrate to a new socket (new_host) during an active connection.
  bool MigrateSocket(const QuicIpAddress& new_host);

  // Migrate to a new socket (new_host, port) during an active connection.
  bool MigrateSocketWithSpecifiedPort(const QuicIpAddress& new_host, int port);

  QuicSession* session();

  bool connected() const;
  bool goaway_received() const;

  const QuicServerId& server_id() const { return server_id_; }

  // This should only be set before the initial Connect()
  void set_server_id(const QuicServerId& server_id) { server_id_ = server_id; }

  void SetUserAgentID(const QuicString& user_agent_id) {
    crypto_config_.set_user_agent_id(user_agent_id);
  }

  // SetChannelIDSource sets a ChannelIDSource that will be called, when the
  // server supports channel IDs, to obtain a channel ID for signing a message
  // proving possession of the channel ID. This object takes ownership of
  // |source|.
  void SetChannelIDSource(ChannelIDSource* source) {
    crypto_config_.SetChannelIDSource(source);
  }

  // UseTokenBinding enables token binding negotiation in the client.  This
  // should only be called before the initial Connect().  The client will still
  // need to check that token binding is negotiated with the server, and add
  // token binding headers to requests if so.  server, and add token binding
  // headers to requests if so.  The negotiated token binding parameters can be
  // found on the QuicCryptoNegotiatedParameters object in
  // token_binding_key_param.
  void UseTokenBinding() {
    crypto_config_.tb_key_params = QuicTagVector{kTB10};
  }

  const ParsedQuicVersionVector& supported_versions() const {
    return supported_versions_;
  }

  void SetSupportedVersions(const ParsedQuicVersionVector& versions) {
    supported_versions_ = versions;
  }

  QuicConfig* config() { return &config_; }

  QuicCryptoClientConfig* crypto_config() { return &crypto_config_; }

  // Change the initial maximum packet size of the connection.  Has to be called
  // before Connect()/StartConnect() in order to have any effect.
  void set_initial_max_packet_length(QuicByteCount initial_max_packet_length) {
    initial_max_packet_length_ = initial_max_packet_length;
  }

  int num_stateless_rejects_received() const {
    return num_stateless_rejects_received_;
  }

  // The number of client hellos sent, taking stateless rejects into
  // account.  In the case of a stateless reject, the initial
  // connection object may be torn down and a new one created.  The
  // user cannot rely upon the latest connection object to get the
  // total number of client hellos sent, and should use this function
  // instead.
  int GetNumSentClientHellos();

  // Gather the stats for the last session and update the stats for the overall
  // connection.
  void UpdateStats();

  // The number of server config updates received.  We assume no
  // updates can be sent during a previously, statelessly rejected
  // connection, so only the latest session is taken into account.
  int GetNumReceivedServerConfigUpdates();

  // Returns any errors that occurred at the connection-level (as
  // opposed to the session-level).  When a stateless reject occurs,
  // the error of the last session may not reflect the overall state
  // of the connection.
  QuicErrorCode connection_error() const;
  void set_connection_error(QuicErrorCode connection_error) {
    connection_error_ = connection_error;
  }

  bool connected_or_attempting_connect() const {
    return connected_or_attempting_connect_;
  }
  void set_connected_or_attempting_connect(
      bool connected_or_attempting_connect) {
    connected_or_attempting_connect_ = connected_or_attempting_connect;
  }

  QuicPacketWriter* writer() { return writer_.get(); }
  void set_writer(QuicPacketWriter* writer) {
    if (writer_.get() != writer) {
      writer_.reset(writer);
    }
  }
  void reset_writer() { writer_.reset(); }

  ProofVerifier* proof_verifier() const;

  void set_bind_to_address(QuicIpAddress address) {
    bind_to_address_ = address;
  }

  QuicIpAddress bind_to_address() const { return bind_to_address_; }

  void set_local_port(int local_port) { local_port_ = local_port; }

  int local_port() const { return local_port_; }

  const QuicSocketAddress& server_address() const { return server_address_; }

  void set_server_address(const QuicSocketAddress& server_address) {
    server_address_ = server_address;
  }

  QuicConnectionHelperInterface* helper() { return helper_.get(); }

  NetworkHelper* network_helper();
  const NetworkHelper* network_helper() const;

  bool initialized() const { return initialized_; }

  void SetPreSharedKey(QuicStringPiece key) {
    crypto_config_.set_pre_shared_key(key);
  }

 protected:
  // TODO(rch): Move GetNumSentClientHellosFromSession and
  // GetNumReceivedServerConfigUpdatesFromSession into a new/better
  // QuicSpdyClientSession class. The current inherits dependencies from
  // Spdy. When that happens this class and all its subclasses should
  // work with QuicSpdyClientSession instead of QuicSession.
  // That will obviate the need for the pure virtual functions below.

  // Extract the number of sent client hellos from the session.
  virtual int GetNumSentClientHellosFromSession() = 0;

  // The number of server config updates received.  We assume no
  // updates can be sent during a previously, statelessly rejected
  // connection, so only the latest session is taken into account.
  virtual int GetNumReceivedServerConfigUpdatesFromSession() = 0;

  // If this client supports buffering data, resend it.
  virtual void ResendSavedData() = 0;

  // If this client supports buffering data, clear it.
  virtual void ClearDataToResend() = 0;

  // Takes ownership of |connection|. If you override this function,
  // you probably want to call ResetSession() in your destructor.
  // TODO(rch): Change the connection parameter to take in a
  // std::unique_ptr<QuicConnection> instead.
  virtual std::unique_ptr<QuicSession> CreateQuicClientSession(
      QuicConnection* connection) = 0;

  // Generates the next ConnectionId for |server_id_|.  By default, if the
  // cached server config contains a server-designated ID, that ID will be
  // returned.  Otherwise, the next random ID will be returned.
  QuicConnectionId GetNextConnectionId();

  // Returns the next server-designated ConnectionId from the cached config for
  // |server_id_|, if it exists.  Otherwise, returns 0.
  QuicConnectionId GetNextServerDesignatedConnectionId();

  // Generates a new, random connection ID (as opposed to a server-designated
  // connection ID).
  virtual QuicConnectionId GenerateNewConnectionId();

  QuicAlarmFactory* alarm_factory() { return alarm_factory_.get(); }

  // Subclasses may need to explicitly clear the session on destruction
  // if they create it with objects that will be destroyed before this is.
  // You probably want to call this if you override CreateQuicSpdyClientSession.
  void ResetSession() { session_.reset(); }

 private:
  // |server_id_| is a tuple (hostname, port, is_https) of the server.
  QuicServerId server_id_;

  // Tracks if the client is initialized to connect.
  bool initialized_;

  // Address of the server.
  QuicSocketAddress server_address_;

  // If initialized, the address to bind to.
  QuicIpAddress bind_to_address_;

  // Local port to bind to. Initialize to 0.
  int local_port_;

  // config_ and crypto_config_ contain configuration and cached state about
  // servers.
  QuicConfig config_;
  QuicCryptoClientConfig crypto_config_;

  // Helper to be used by created connections. Must outlive |session_|.
  std::unique_ptr<QuicConnectionHelperInterface> helper_;

  // Alarm factory to be used by created connections. Must outlive |session_|.
  std::unique_ptr<QuicAlarmFactory> alarm_factory_;

  // Writer used to actually send packets to the wire. Must outlive |session_|.
  std::unique_ptr<QuicPacketWriter> writer_;

  // Session which manages streams.
  std::unique_ptr<QuicSession> session_;

  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary). We will always pick supported_versions_[0] as the
  // initial version to use.
  ParsedQuicVersionVector supported_versions_;

  // The initial value of maximum packet size of the connection.  If set to
  // zero, the default is used.
  QuicByteCount initial_max_packet_length_;

  // The number of stateless rejects received during the current/latest
  // connection.
  // TODO(jokulik): Consider some consistent naming scheme (or other) for member
  // variables that are kept per-request, per-connection, and over the client's
  // lifetime.
  int num_stateless_rejects_received_;

  // The number of hellos sent during the current/latest connection.
  int num_sent_client_hellos_;

  // Used to store any errors that occurred with the overall connection (as
  // opposed to that associated with the last session object).
  QuicErrorCode connection_error_;

  // True when the client is attempting to connect or re-connect the session (in
  // the case of a stateless reject).  Set to false  between a call to
  // Disconnect() and the subsequent call to StartConnect().  When
  // connected_or_attempting_connect_ is false, the session object corresponds
  // to the previous client-level connection.
  bool connected_or_attempting_connect_;

  // The network helper used to create sockets and manage the event loop.
  // Not owned by this class.
  std::unique_ptr<NetworkHelper> network_helper_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TOOLS_QUIC_CLIENT_BASE_H_
