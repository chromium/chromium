// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_H_
#define MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT ContentEncoding {
 public:
  // The following enum definitions are based on the ContentEncoding element
  // specified in the Matroska spec.

  static const int kOrderInvalid = -1;

  enum Scope {
    kScopeInvalid = 0,
    kScopeAllFrameContents = 1,
    kScopeTrackPrivateData = 2,
    kScopeNextContentEncodingData = 4,
    kScopeMax = 7,
  };

  enum Type {
    kTypeInvalid = -1,
    kTypeCompression = 0,
    kTypeEncryption = 1,
  };

  enum EncryptionAlgo {
    kEncAlgoInvalid = -1,
    kEncAlgoNotEncrypted = 0,
    kEncAlgoDes = 1,
    kEncAlgo3des = 2,
    kEncAlgoTwofish = 3,
    kEncAlgoBlowfish = 4,
    kEncAlgoAes = 5,
  };

  enum CipherMode {
    kCipherModeInvalid = 0,
    kCipherModeCtr = 1,
  };

  ContentEncoding();

  ContentEncoding(const ContentEncoding&) = delete;
  ContentEncoding& operator=(const ContentEncoding&) = delete;

  ~ContentEncoding();

  int64_t order() const { return order_; }
  void set_order(int64_t order) { order_ = order; }

  Scope scope() const { return scope_; }
  void set_scope(Scope scope) { scope_ = scope; }

  Type type() const { return type_; }
  void set_type(Type type) { type_ = type; }

  EncryptionAlgo encryption_algo() const { return encryption_algo_; }
  void set_encryption_algo(EncryptionAlgo encryption_algo) {
    encryption_algo_ = encryption_algo;
  }

  const std::string& encryption_key_id() const { return encryption_key_id_; }
  void SetEncryptionKeyId(const uint8_t* encryption_key_id, int size);

  CipherMode cipher_mode() const { return cipher_mode_; }
  void set_cipher_mode(CipherMode mode) { cipher_mode_ = mode; }

 private:
  int64_t order_;
  Scope scope_;
  Type type_;
  EncryptionAlgo encryption_algo_;
  std::string encryption_key_id_;
  CipherMode cipher_mode_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_H_
