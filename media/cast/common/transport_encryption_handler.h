// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_TRANSPORT_ENCRYPTION_HANDLER_H_
#define MEDIA_CAST_COMMON_TRANSPORT_ENCRYPTION_HANDLER_H_

// Helper class to handle encryption for the Cast Transport library.

#include <stdint.h>

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "media/cast/common/frame_id.h"

namespace crypto {
class Encryptor;
class SymmetricKey;
}

namespace media {
namespace cast {

class TransportEncryptionHandler {
 public:
  TransportEncryptionHandler();

  TransportEncryptionHandler(const TransportEncryptionHandler&) = delete;
  TransportEncryptionHandler& operator=(const TransportEncryptionHandler&) =
      delete;

  ~TransportEncryptionHandler();

  bool Initialize(const std::string& aes_key, const std::string& aes_iv_mask);

  bool Encrypt(FrameId frame_id,
               const base::StringPiece& data,
               std::string* encrypted_data);

  bool Decrypt(FrameId frame_id,
               const base::StringPiece& ciphertext,
               std::string* plaintext);

  bool is_activated() const { return is_activated_; }

 private:
  std::unique_ptr<crypto::SymmetricKey> key_;
  std::unique_ptr<crypto::Encryptor> encryptor_;
  std::string iv_mask_;
  bool is_activated_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_COMMON_TRANSPORT_ENCRYPTION_HANDLER_H_
