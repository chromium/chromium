// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decrypt_config.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/media_buildflags.h"

namespace media {

// static
std::unique_ptr<DecryptConfig> DecryptConfig::CreateCencConfig(
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples) {
  return std::make_unique<DecryptConfig>(EncryptionScheme::kCenc, key_id, iv,
                                         subsamples, base::nullopt);
}

// static
std::unique_ptr<DecryptConfig> DecryptConfig::CreateCbcsConfig(
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples,
    base::Optional<EncryptionPattern> encryption_pattern) {
  return std::make_unique<DecryptConfig>(EncryptionScheme::kCbcs, key_id, iv,
                                         subsamples,
                                         std::move(encryption_pattern));
}

DecryptConfig::DecryptConfig(
    EncryptionScheme encryption_scheme,
    const std::string& key_id,
    const std::string& iv,
    const std::vector<SubsampleEntry>& subsamples,
    base::Optional<EncryptionPattern> encryption_pattern)
    : encryption_scheme_(encryption_scheme),
      key_id_(key_id),
      iv_(iv),
      subsamples_(subsamples),
      encryption_pattern_(std::move(encryption_pattern)) {
  // Unencrypted blocks should not have a DecryptConfig.
  DCHECK_NE(encryption_scheme_, EncryptionScheme::kUnencrypted);
  CHECK_GT(key_id_.size(), 0u);
  CHECK_EQ(iv_.size(), static_cast<size_t>(DecryptConfig::kDecryptionKeySize));

  // Pattern not allowed for non-'cbcs' schemes.
  DCHECK(encryption_scheme_ == EncryptionScheme::kCbcs || !encryption_pattern_);
}

DecryptConfig::~DecryptConfig() = default;

std::unique_ptr<DecryptConfig> DecryptConfig::Clone() const {
  return base::WrapUnique(new DecryptConfig(*this));
}

std::unique_ptr<DecryptConfig> DecryptConfig::CopyNewSubsamplesIV(
    const std::vector<SubsampleEntry>& subsamples,
    const std::string& iv) {
  return std::make_unique<DecryptConfig>(encryption_scheme_, key_id_, iv,
                                         subsamples, encryption_pattern_);
}

bool DecryptConfig::HasPattern() const {
  return encryption_pattern_.has_value();
}

bool DecryptConfig::Matches(const DecryptConfig& config) const {
  if (key_id() != config.key_id() || iv() != config.iv() ||
      subsamples().size() != config.subsamples().size() ||
      encryption_scheme_ != config.encryption_scheme_ ||
      encryption_pattern_ != config.encryption_pattern_) {
    return false;
  }

  for (size_t i = 0; i < subsamples().size(); ++i) {
    if ((subsamples()[i].clear_bytes != config.subsamples()[i].clear_bytes) ||
        (subsamples()[i].cypher_bytes != config.subsamples()[i].cypher_bytes)) {
      return false;
    }
  }

  return true;
}

std::ostream& DecryptConfig::Print(std::ostream& os) const {
  os << "key_id:'" << base::HexEncode(key_id_.data(), key_id_.size()) << "'"
     << " iv:'" << base::HexEncode(iv_.data(), iv_.size()) << "'"
     << " scheme:" << encryption_scheme_;

  if (encryption_pattern_) {
    os << " pattern:" << encryption_pattern_->crypt_byte_block() << ":"
       << encryption_pattern_->skip_byte_block();
  }

  os << " subsamples:[";
  for (const SubsampleEntry& entry : subsamples_) {
    os << "(clear:" << entry.clear_bytes << ", cypher:" << entry.cypher_bytes
       << ")";
  }
  os << "]";
  return os;
}

DecryptConfig::DecryptConfig(const DecryptConfig& other) = default;

}  // namespace media
