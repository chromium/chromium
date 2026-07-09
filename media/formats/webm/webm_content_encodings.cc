// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_content_encodings.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"

namespace media {

ContentEncoding::ContentEncoding()
    : order_(kOrderInvalid),
      scope_(kScopeInvalid),
      type_(kTypeInvalid),
      encryption_algo_(kEncAlgoInvalid),
      cipher_mode_(kCipherModeInvalid) {
}

ContentEncoding::~ContentEncoding() = default;

void ContentEncoding::SetEncryptionKeyId(
    base::span<const uint8_t> encryption_key_id) {
  DCHECK(!encryption_key_id.empty());
  encryption_key_id_ = std::string(base::as_string_view(encryption_key_id));
}

}  // namespace media
