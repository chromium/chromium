// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_server_info.h"

#include <limits>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/stl_util.h"

using std::string;

namespace {

const int kQuicCryptoConfigVersion = 2;

}  // namespace

namespace net {

QuicServerInfo::State::State() = default;

QuicServerInfo::State::~State() = default;

void QuicServerInfo::State::Clear() {
  base::STLClearObject(&server_config);
  base::STLClearObject(&source_address_token);
  base::STLClearObject(&cert_sct);
  base::STLClearObject(&chlo_hash);
  base::STLClearObject(&server_config_sig);
  base::STLClearObject(&certs);
}

QuicServerInfo::QuicServerInfo(const quic::QuicServerId& server_id)
    : server_id_(server_id) {}

QuicServerInfo::~QuicServerInfo() = default;

const QuicServerInfo::State& QuicServerInfo::state() const {
  return state_;
}

QuicServerInfo::State* QuicServerInfo::mutable_state() {
  return &state_;
}

bool QuicServerInfo::Parse(const string& data) {
  State* state = mutable_state();

  state->Clear();

  bool r = ParseInner(data);
  if (!r)
    state->Clear();
  return r;
}

bool QuicServerInfo::ParseInner(const string& data) {
  State* state = mutable_state();

  // No data was read from the disk cache.
  if (data.empty()) {
    return false;
  }

  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(data));
  base::PickleIterator iter(pickle);

  int version = -1;
  if (!iter.ReadInt(&version)) {
    DVLOG(1) << "Missing version";
    return false;
  }

  if (version != kQuicCryptoConfigVersion) {
    DVLOG(1) << "Unsupported version";
    return false;
  }

  if (!iter.ReadString(&state->server_config)) {
    DVLOG(1) << "Malformed server_config";
    return false;
  }
  if (!iter.ReadString(&state->source_address_token)) {
    DVLOG(1) << "Malformed source_address_token";
    return false;
  }
  if (!iter.ReadString(&state->cert_sct)) {
    DVLOG(1) << "Malformed cert_sct";
    return false;
  }
  if (!iter.ReadString(&state->chlo_hash)) {
    DVLOG(1) << "Malformed chlo_hash";
    return false;
  }
  if (!iter.ReadString(&state->server_config_sig)) {
    DVLOG(1) << "Malformed server_config_sig";
    return false;
  }

  // Read certs.
  uint32_t num_certs;
  if (!iter.ReadUInt32(&num_certs)) {
    DVLOG(1) << "Malformed num_certs";
    return false;
  }

  for (uint32_t i = 0; i < num_certs; i++) {
    string cert;
    if (!iter.ReadString(&cert)) {
      DVLOG(1) << "Malformed cert";
      return false;
    }
    state->certs.push_back(cert);
  }

  return true;
}

string QuicServerInfo::Serialize() {
  string pickled_data = SerializeInner();
  state_.Clear();
  return pickled_data;
}

string QuicServerInfo::SerializeInner() const {
  if (state_.certs.size() > std::numeric_limits<uint32_t>::max())
    return std::string();

  base::Pickle p;
  p.WriteInt(kQuicCryptoConfigVersion);
  p.WriteString(state_.server_config);
  p.WriteString(state_.source_address_token);
  p.WriteString(state_.cert_sct);
  p.WriteString(state_.chlo_hash);
  p.WriteString(state_.server_config_sig);
  p.WriteUInt32(state_.certs.size());

  for (const auto& cert : state_.certs)
    p.WriteString(cert);

  return string(reinterpret_cast<const char*>(p.data()), p.size());
}

}  // namespace net
