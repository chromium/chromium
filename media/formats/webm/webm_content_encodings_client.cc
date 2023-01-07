// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_content_encodings_client.h"

#include <memory>

#include "base/logging.h"
#include "media/formats/webm/webm_constants.h"

namespace media {

WebMContentEncodingsClient::WebMContentEncodingsClient(MediaLog* media_log)
    : media_log_(media_log),
      content_encryption_encountered_(false),
      content_encodings_ready_(false) {}

WebMContentEncodingsClient::~WebMContentEncodingsClient() = default;

const ContentEncodings& WebMContentEncodingsClient::content_encodings() const {
  DCHECK(content_encodings_ready_);
  return content_encodings_;
}

WebMParserClient* WebMContentEncodingsClient::OnListStart(int id) {
  if (id == kWebMIdContentEncodings) {
    DCHECK(!cur_content_encoding_.get());
    DCHECK(!content_encryption_encountered_);
    content_encodings_.clear();
    content_encodings_ready_ = false;
    return this;
  }

  if (id == kWebMIdContentEncoding) {
    DCHECK(!cur_content_encoding_.get());
    DCHECK(!content_encryption_encountered_);
    cur_content_encoding_ = std::make_unique<ContentEncoding>();
    return this;
  }

  if (id == kWebMIdContentEncryption) {
    DCHECK(cur_content_encoding_.get());
    if (content_encryption_encountered_) {
      MEDIA_LOG(ERROR, media_log_) << "Unexpected multiple ContentEncryption.";
      return NULL;
    }
    content_encryption_encountered_ = true;
    return this;
  }

  if (id == kWebMIdContentEncAESSettings) {
    DCHECK(cur_content_encoding_.get());
    return this;
  }

  MEDIA_LOG(ERROR, media_log_) << "Unsupported element " << id;
  return NULL;
}

// Mandatory occurrence restriction is checked in this function. Multiple
// occurrence restriction is checked in OnUInt and OnBinary.
bool WebMContentEncodingsClient::OnListEnd(int id) {
  if (id == kWebMIdContentEncodings) {
    // ContentEncoding element is mandatory. Check this!
    if (content_encodings_.empty()) {
      MEDIA_LOG(ERROR, media_log_) << "Missing ContentEncoding.";
      return false;
    }
    content_encodings_ready_ = true;
    return true;
  }

  if (id == kWebMIdContentEncoding) {
    DCHECK(cur_content_encoding_.get());

    //
    // Specify default values to missing mandatory elements.
    //

    if (cur_content_encoding_->order() == ContentEncoding::kOrderInvalid) {
      // Default value of encoding order is 0, which should only be used on the
      // first ContentEncoding.
      if (!content_encodings_.empty()) {
        MEDIA_LOG(ERROR, media_log_) << "Missing ContentEncodingOrder.";
        return false;
      }
      cur_content_encoding_->set_order(0);
    }

    if (cur_content_encoding_->scope() == ContentEncoding::kScopeInvalid)
      cur_content_encoding_->set_scope(ContentEncoding::kScopeAllFrameContents);

    if (cur_content_encoding_->type() == ContentEncoding::kTypeInvalid)
      cur_content_encoding_->set_type(ContentEncoding::kTypeCompression);

    // Check for elements valid in spec but not supported for now.
    if (cur_content_encoding_->type() == ContentEncoding::kTypeCompression) {
      MEDIA_LOG(ERROR, media_log_) << "ContentCompression not supported.";
      return false;
    }

    // Enforce mandatory elements without default values.
    DCHECK(cur_content_encoding_->type() == ContentEncoding::kTypeEncryption);
    if (!content_encryption_encountered_) {
      MEDIA_LOG(ERROR, media_log_) << "ContentEncodingType is encryption but"
                                   << " ContentEncryption is missing.";
      return false;
    }

    content_encodings_.push_back(std::move(cur_content_encoding_));
    content_encryption_encountered_ = false;
    return true;
  }

  if (id == kWebMIdContentEncryption) {
    DCHECK(cur_content_encoding_.get());
    // Specify default value for elements that are not present.
    if (cur_content_encoding_->encryption_algo() ==
        ContentEncoding::kEncAlgoInvalid) {
      cur_content_encoding_->set_encryption_algo(
          ContentEncoding::kEncAlgoNotEncrypted);
    }
    return true;
  }

  if (id == kWebMIdContentEncAESSettings) {
    if (cur_content_encoding_->cipher_mode() ==
        ContentEncoding::kCipherModeInvalid)
      cur_content_encoding_->set_cipher_mode(ContentEncoding::kCipherModeCtr);
    return true;
  }

  MEDIA_LOG(ERROR, media_log_) << "Unsupported element " << id;
  return false;
}

// Multiple occurrence restriction and range are checked in this function.
// Mandatory occurrence restriction is checked in OnListEnd.
bool WebMContentEncodingsClient::OnUInt(int id, int64_t val) {
  DCHECK(cur_content_encoding_.get());

  if (id == kWebMIdContentEncodingOrder) {
    if (cur_content_encoding_->order() != ContentEncoding::kOrderInvalid) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unexpected multiple ContentEncodingOrder.";
      return false;
    }

    if (val != static_cast<int64_t>(content_encodings_.size())) {
      // According to the spec, encoding order starts with 0 and counts upwards.
      MEDIA_LOG(ERROR, media_log_) << "Unexpected ContentEncodingOrder.";
      return false;
    }

    cur_content_encoding_->set_order(val);
    return true;
  }

  if (id == kWebMIdContentEncodingScope) {
    if (cur_content_encoding_->scope() != ContentEncoding::kScopeInvalid) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unexpected multiple ContentEncodingScope.";
      return false;
    }

    if (val == ContentEncoding::kScopeInvalid ||
        val > ContentEncoding::kScopeMax) {
      MEDIA_LOG(ERROR, media_log_) << "Unexpected ContentEncodingScope.";
      return false;
    }

    if (val & ContentEncoding::kScopeNextContentEncodingData) {
      MEDIA_LOG(ERROR, media_log_) << "Encoded next ContentEncoding is not "
                                      "supported.";
      return false;
    }

    cur_content_encoding_->set_scope(static_cast<ContentEncoding::Scope>(val));
    return true;
  }

  if (id == kWebMIdContentEncodingType) {
    if (cur_content_encoding_->type() != ContentEncoding::kTypeInvalid) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unexpected multiple ContentEncodingType.";
      return false;
    }

    if (val == ContentEncoding::kTypeCompression) {
      MEDIA_LOG(ERROR, media_log_) << "ContentCompression not supported.";
      return false;
    }

    if (val != ContentEncoding::kTypeEncryption) {
      MEDIA_LOG(ERROR, media_log_) << "Unexpected ContentEncodingType " << val
                                   << ".";
      return false;
    }

    cur_content_encoding_->set_type(static_cast<ContentEncoding::Type>(val));
    return true;
  }

  if (id == kWebMIdContentEncAlgo) {
    if (cur_content_encoding_->encryption_algo() !=
        ContentEncoding::kEncAlgoInvalid) {
      MEDIA_LOG(ERROR, media_log_) << "Unexpected multiple ContentEncAlgo.";
      return false;
    }

    if (val < ContentEncoding::kEncAlgoNotEncrypted ||
        val > ContentEncoding::kEncAlgoAes) {
      MEDIA_LOG(ERROR, media_log_) << "Unexpected ContentEncAlgo " << val
                                   << ".";
      return false;
    }

    cur_content_encoding_->set_encryption_algo(
        static_cast<ContentEncoding::EncryptionAlgo>(val));
    return true;
  }

  if (id == kWebMIdAESSettingsCipherMode) {
    if (cur_content_encoding_->cipher_mode() !=
        ContentEncoding::kCipherModeInvalid) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unexpected multiple AESSettingsCipherMode.";
      return false;
    }

    if (val != ContentEncoding::kCipherModeCtr) {
      MEDIA_LOG(ERROR, media_log_) << "Unexpected AESSettingsCipherMode " << val
                                   << ".";
      return false;
    }

    cur_content_encoding_->set_cipher_mode(
        static_cast<ContentEncoding::CipherMode>(val));
    return true;
  }

  MEDIA_LOG(ERROR, media_log_) << "Unsupported element " << id;
  return false;
}

// Multiple occurrence restriction is checked in this function.  Mandatory
// restriction is checked in OnListEnd.
bool WebMContentEncodingsClient::OnBinary(int id,
                                          const uint8_t* data,
                                          int size) {
  DCHECK(cur_content_encoding_.get());
  DCHECK(data);

  if (id != kWebMIdContentEncKeyID) {
    MEDIA_LOG(ERROR, media_log_) << "Unsupported element " << id;
    return false;
  }

  if (!cur_content_encoding_->encryption_key_id().empty()) {
    MEDIA_LOG(ERROR, media_log_) << "Unexpected multiple ContentEncKeyID";
    return false;
  }

  if (size <= 0) {
    MEDIA_LOG(ERROR, media_log_) << "Invalid ContentEncKeyID size: " << size;
    return false;
  }

  cur_content_encoding_->SetEncryptionKeyId(data, size);
  return true;
}

}  // namespace media
