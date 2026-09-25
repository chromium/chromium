// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_map.h"  // Used by signer-set-inc.cc.
#include "net/cert/internal/chrome_root_store_data.h"

namespace net {

namespace {

#include "net/data/ssl/chrome_root_store/chrome-root-store-inc.cc"
#include "net/data/ssl/chrome_root_store/signer-set-inc.cc"

}  // namespace

base::span<const ChromeRootCertInfo> GetCompiledChromeRootCertList() {
  return kChromeRootCertList;
}

base::span<const base::span<const uint8_t>> GetCompiledEutlRootCertList() {
  return kEutlRootCertList;
}

int64_t GetCompiledChromeRootStoreVersion() {
  return kRootStoreVersion;
}

int64_t GetCompiledSignerSetTimestampSeconds() {
  return kSignerSetCompiledTimestampSeconds;
}

base::span<const uint8_t> GetCompiledSignerSetProtoBytes() {
  return kSignerSetProto;
}

std::optional<base::span<const uint8_t>> FindCompiledSignerKey(
    base::span<const uint8_t> sha256_hash) {
  auto it = kSignerKeys.find(sha256_hash);
  if (it == kSignerKeys.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace net
