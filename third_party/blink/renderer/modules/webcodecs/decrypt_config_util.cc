// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decrypt_config_util.h"

#include "media/base/decrypt_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_decrypt_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encryption_pattern.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_subsample_entry.h"
#include "third_party/blink/renderer/modules/webcodecs/array_buffer_util.h"

namespace blink {

std::unique_ptr<media::DecryptConfig> CreateMediaDecryptConfig(
    const DecryptConfig& js_config) {
  auto scheme = js_config.encryptionScheme();
  if (scheme != "cenc" && scheme != "cbcs") {
    return nullptr;
  }

  auto iv = AsSpan<const char>(js_config.initializationVector());
  if (iv.size() != media::DecryptConfig::kDecryptionKeySize) {
    return nullptr;
  }
  std::string iv_str(iv.data(), iv.size());

  auto key_id = AsSpan<const char>(js_config.keyId());
  std::string key_id_str(key_id.data(), key_id.size());

  std::vector<media::SubsampleEntry> subsamples;
  for (const auto& entry : js_config.subsampleLayout()) {
    subsamples.emplace_back(entry->clearBytes(), entry->cypherBytes());
  }

  if (scheme == "cenc") {
    return media::DecryptConfig::CreateCencConfig(
        std::move(key_id_str), std::move(iv_str), subsamples);
  }

  DCHECK_EQ(scheme, "cbcs");
  std::optional<media::EncryptionPattern> encryption_pattern;
  if (js_config.hasEncryptionPattern()) {
    encryption_pattern.emplace(js_config.encryptionPattern()->cryptByteBlock(),
                               js_config.encryptionPattern()->skipByteBlock());
  }
  return media::DecryptConfig::CreateCbcsConfig(
      std::move(key_id_str), std::move(iv_str), subsamples, encryption_pattern);
}

std::optional<media::EncryptionScheme> ToMediaEncryptionScheme(
    const String& scheme) {
  if (scheme == "cenc") {
    return media::EncryptionScheme::kCenc;
  } else if (scheme == "cbcs") {
    return media::EncryptionScheme::kCbcs;
  } else {
    return std::nullopt;
  }
}

}  // namespace blink
