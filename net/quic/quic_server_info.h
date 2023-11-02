// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SERVER_INFO_H_
#define NET_QUIC_QUIC_SERVER_INFO_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"

namespace net {

// QuicServerInfo is an interface for fetching information about a QUIC server.
// This information may be stored on disk so does not include keys or other
// sensitive information. Primarily it's intended for caching the QUIC server's
// crypto config.
class NET_EXPORT_PRIVATE QuicServerInfo {
 public:
  // Enum to track failure reasons to read/load/write of QuicServerInfo to
  // and from disk cache.
  enum FailureReason {
    WAIT_FOR_DATA_READY_INVALID_ARGUMENT_FAILURE = 0,
    GET_BACKEND_FAILURE = 1,
    OPEN_FAILURE = 2,
    CREATE_OR_OPEN_FAILURE = 3,
    PARSE_NO_DATA_FAILURE = 4,
    PARSE_FAILURE = 5,
    READ_FAILURE = 6,
    READY_TO_PERSIST_FAILURE = 7,
    PERSIST_NO_BACKEND_FAILURE = 8,
    WRITE_FAILURE = 9,
    NO_FAILURE = 10,
    PARSE_DATA_DECODE_FAILURE = 11,
    NUM_OF_FAILURES = 12,
  };

  explicit QuicServerInfo(const quic::QuicServerId& server_id);

  QuicServerInfo(const QuicServerInfo&) = delete;
  QuicServerInfo& operator=(const QuicServerInfo&) = delete;

  virtual ~QuicServerInfo();

  // Fetches the server config from the backing store, and returns true
  // if the server config was found.
  virtual bool Load() = 0;

  // Persist allows for the server information to be updated for future uses.
  virtual void Persist() = 0;

  struct State {
    State();

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    ~State();

    void Clear();

    // This class matches QuicCryptoClientConfig::CachedState.
    std::string server_config;         // A serialized handshake message.
    std::string source_address_token;  // An opaque proof of IP ownership.
    std::string cert_sct;              // Signed timestamp of the leaf cert.
    std::string chlo_hash;             // Hash of the CHLO message.
    std::vector<std::string> certs;    // A list of certificates in leaf-first
                                       // order.
    std::string server_config_sig;     // A signature of |server_config_|.
  };

  // Once the data is ready, it can be read using the following members. These
  // members can then be updated before calling |Persist|.
  const State& state() const;
  State* mutable_state();

 protected:
  // Parse parses pickled data and fills out the public member fields of this
  // object. It returns true iff the parse was successful. The public member
  // fields will be set to something sane in any case.
  bool Parse(const std::string& data);
  std::string Serialize();

  State state_;

  // This is the QUIC server (hostname, port, is_https, privacy_mode) tuple for
  // which we restore the crypto_config.
  const quic::QuicServerId server_id_;

 private:
  // ParseInner is a helper function for Parse.
  bool ParseInner(const std::string& data);

  // SerializeInner is a helper function for Serialize.
  std::string SerializeInner() const;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SERVER_INFO_H_
