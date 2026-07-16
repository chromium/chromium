// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cassert>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/libsrtp/include/srtp.h"
#include "third_party/libsrtp/include/srtp_priv.h"
#include "third_party/libsrtp/test/rtp.h"

// TODO(katrielc) Also test the authenticated path, which is what
// WebRTC uses.  This is nontrivial because you need to bypass the MAC
// check. Two options: add a UNSAFE_FUZZER_MODE flag to libsrtp (or
// the chromium fork of it), or compute the HMAC of whatever gibberish
// the fuzzer produces and write it into the packet manually.

namespace LibSrtpFuzzer {
enum CryptoPolicy {
  NONE,
  LIKE_WEBRTC,
  LIKE_WEBRTC_SHORT_AUTH,
  LIKE_WEBRTC_WITHOUT_AUTH,
  AES_128_GCM,
  AES_256_GCM,
  NUMBER_OF_POLICIES,
};
}

static size_t GetKeyLength(LibSrtpFuzzer::CryptoPolicy crypto_policy) {
  switch (crypto_policy) {
    case LibSrtpFuzzer::NUMBER_OF_POLICIES:
      return SRTP_AES_ICM_128_KEY_LEN_WSALT;
    case LibSrtpFuzzer::NONE:
      return 0;  // Null-auth.
    case LibSrtpFuzzer::LIKE_WEBRTC:
    case LibSrtpFuzzer::LIKE_WEBRTC_SHORT_AUTH:
    case LibSrtpFuzzer::LIKE_WEBRTC_WITHOUT_AUTH:
      return SRTP_AES_ICM_128_KEY_LEN_WSALT;
    case LibSrtpFuzzer::AES_128_GCM:
      return SRTP_AES_GCM_128_KEY_LEN_WSALT;
    case LibSrtpFuzzer::AES_256_GCM:
      return SRTP_AES_GCM_256_KEY_LEN_WSALT;
  }
}

struct Environment {
  srtp_policy_t GetCryptoPolicy(LibSrtpFuzzer::CryptoPolicy crypto_policy,
                                base::span<const uint8_t> replacement_key) {
    switch (crypto_policy) {
      case LibSrtpFuzzer::NUMBER_OF_POLICIES:
      case LibSrtpFuzzer::NONE:
        srtp_crypto_policy_set_null_cipher_hmac_null(&policy_.rtp);
        srtp_crypto_policy_set_null_cipher_hmac_null(&policy_.rtcp);
        break;
      case LibSrtpFuzzer::LIKE_WEBRTC:
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy_.rtp);
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy_.rtcp);
        break;
      case LibSrtpFuzzer::LIKE_WEBRTC_SHORT_AUTH:
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy_.rtp);
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy_.rtcp);
        break;
      case LibSrtpFuzzer::LIKE_WEBRTC_WITHOUT_AUTH:
        srtp_crypto_policy_set_aes_cm_128_null_auth(&policy_.rtp);
        srtp_crypto_policy_set_aes_cm_128_null_auth(&policy_.rtcp);
        break;
      case LibSrtpFuzzer::AES_128_GCM:
        // There was a security bug in the GCM mode in libsrtp 1.5.2.
        srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy_.rtp);
        srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy_.rtcp);
        break;
      case LibSrtpFuzzer::AES_256_GCM:
        // WebRTC uses AES-256-GCM by default if GCM ciphers are enabled.
        srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy_.rtp);
        srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy_.rtcp);
        break;
    }

    assert(static_cast<size_t>(policy_.rtp.cipher_key_len) ==
           replacement_key.size());
    assert(static_cast<size_t>(policy_.rtcp.cipher_key_len) ==
           replacement_key.size());
    std::ranges::fill(base::span(key_), 0);
    base::span(key_).copy_prefix_from(replacement_key);
    return policy_;
  }

  Environment() {
    srtp_init();

    policy_.allow_repeat_tx = 1;
    policy_.key = key_;
    policy_.ssrc.type = ssrc_any_inbound;
    policy_.ssrc.value = 0xdeadbeef;
    policy_.window_size = 1024;
  }

 private:
  srtp_policy_t policy_ = {};
  unsigned char key_[SRTP_MAX_KEY_LEN] = {};
};

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  static Environment env;
  base::SpanReader reader(data);

  // Read one byte and use it to choose a crypto policy.
  uint8_t policy_byte;
  if (!reader.ReadU8BigEndian(policy_byte)) {
    return 0;
  }
  LibSrtpFuzzer::CryptoPolicy policy = static_cast<LibSrtpFuzzer::CryptoPolicy>(
      policy_byte % LibSrtpFuzzer::NUMBER_OF_POLICIES);

  // Read some more bytes to use as a key.
  size_t key_length = GetKeyLength(policy);
  auto key_bytes = reader.Read<SRTP_MAX_KEY_LEN>();
  if (!key_bytes) {
    return 0;
  }
  srtp_policy_t srtp_policy =
      env.GetCryptoPolicy(policy, key_bytes->first(key_length));

  // Read one byte and use as number of encrypted header extensions.
  uint8_t num_encrypted_headers;
  if (!reader.ReadU8BigEndian(num_encrypted_headers)) {
    return 0;
  }
  std::vector<int> enc_xtn_hdrs;
  if (num_encrypted_headers > 0) {
    // Use next bytes as extension ids.
    auto headers_bytes = reader.Read(num_encrypted_headers);
    if (!headers_bytes) {
      return 0;
    }
    enc_xtn_hdrs.assign(headers_bytes->begin(), headers_bytes->end());
    srtp_policy.enc_xtn_hdr = enc_xtn_hdrs.data();
    srtp_policy.enc_xtn_hdr_count = static_cast<int>(num_encrypted_headers);
  }

  srtp_t session;
  srtp_err_status_t error = srtp_create(&session, &srtp_policy);
  if (error != srtp_err_status_ok) {
    return 0;
  }

  // Read one byte as a packet length N, then feed the next N bytes
  // into srtp_unprotect. Keep doing until we run out of data.
  while (true) {
    uint8_t packet_size_byte;
    if (!reader.ReadU8BigEndian(packet_size_byte)) {
      break;
    }
    size_t packet_size =
        std::min(static_cast<size_t>(packet_size_byte), reader.remaining());
    if (packet_size == 0) {
      break;
    }
    auto packet_span = reader.Read(packet_size);
    if (!packet_span) {
      break;
    }

    size_t header_size = std::min(sizeof(srtp_hdr_t), packet_size);

    // We deliberately do not initialise this struct. MSAN will catch
    // usage of the uninitialised memory.
    rtp_msg_t message;
    auto [header_part, body_part] = packet_span->split_at(header_size);
    base::byte_span_from_ref(message.header).copy_prefix_from(header_part);
    base::byte_span_from_ref(message.body).copy_prefix_from(body_part);

    int out_len = static_cast<int>(packet_size);
    srtp_unprotect(session, &message, &out_len);
  }

  srtp_dealloc(session);
  return 0;
}
