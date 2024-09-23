// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/auth_util.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/socket/ssl_socket.h"

namespace remoting::protocol {

const char kClientAuthSslExporterLabel[] =
    "EXPORTER-remoting-channel-auth-client";
const char kHostAuthSslExporterLabel[] = "EXPORTER-remoting-channel-auth-host";

const char kSslFakeHostName[] = "chromoting";

std::string GetSharedSecretHash(const std::string& tag,
                                const std::string& shared_secret) {
  crypto::HMAC response(crypto::HMAC::SHA256);
  if (!response.Init(tag)) {
    LOG(FATAL) << "HMAC::Init failed";
  }

  unsigned char out_bytes[kSharedSecretHashLength];
  if (!response.Sign(shared_secret, out_bytes, sizeof(out_bytes))) {
    LOG(FATAL) << "HMAC::Sign failed";
  }

  return std::string(out_bytes, out_bytes + sizeof(out_bytes));
}

// static
std::string GetAuthBytes(net::SSLSocket* socket,
                         const std::string_view& label,
                         const std::string_view& shared_secret) {
  // Get keying material from SSL.
  unsigned char key_material[kAuthDigestLength];
  int export_result = socket->ExportKeyingMaterial(
      label, false, "", key_material, kAuthDigestLength);
  if (export_result != net::OK) {
    LOG(ERROR) << "Error fetching keying material: " << export_result;
    return std::string();
  }

  // Generate auth digest based on the keying material and shared secret.
  crypto::HMAC response(crypto::HMAC::SHA256);
  if (!response.Init(key_material, kAuthDigestLength)) {
    NOTREACHED() << "HMAC::Init failed";
  }
  unsigned char out_bytes[kAuthDigestLength];
  if (!response.Sign(shared_secret, out_bytes, kAuthDigestLength)) {
    NOTREACHED() << "HMAC::Sign failed";
  }

  return std::string(out_bytes, out_bytes + kAuthDigestLength);
}

}  // namespace remoting::protocol
