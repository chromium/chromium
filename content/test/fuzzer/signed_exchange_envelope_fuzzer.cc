// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_envelope.h"  // nogncheck

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/containers/span.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "content/browser/web_package/signed_exchange_prologue.h"  // nogncheck
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace content {

namespace signed_exchange_prologue {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

std::vector<uint8_t> CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(
    base::span<const uint8_t> data) {
  return std::vector<uint8_t>(std::from_range, data);
}

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  static const base::NoDestructor<IcuEnvironment> env;
  if (data.size() < BeforeFallbackUrl::kEncodedSizeInBytes) {
    return 0;
  }
  const auto before_fallback_url_bytes =
      CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(
          data.take_first(BeforeFallbackUrl::kEncodedSizeInBytes));
  const auto before_fallback_url = BeforeFallbackUrl::Parse(
      before_fallback_url_bytes, /*devtools_proxy=*/nullptr);

  size_t fallback_url_and_after_length =
      before_fallback_url.ComputeFallbackUrlAndAfterLength();
  if (data.size() < fallback_url_and_after_length) {
    return 0;
  }

  const auto fallback_url_and_after_bytes =
      CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(
          data.take_first(fallback_url_and_after_length));
  const auto fallback_url_and_after = FallbackUrlAndAfter::Parse(
      fallback_url_and_after_bytes, before_fallback_url,
      /*devtools_proxy=*/nullptr);
  if (!fallback_url_and_after.is_valid()) {
    return 0;
  }

  const std::string signature_header_field(
      std::from_range,
      data.take_first(
          std::min(data.size(),
                   fallback_url_and_after.signature_header_field_length())));

  const auto cbor_header =
      CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(data.take_first(
          std::min(data.size(), fallback_url_and_after.cbor_header_length())));

  SignedExchangeEnvelope::Parse(SignedExchangeVersion::kB3,
                                fallback_url_and_after.fallback_url(),
                                signature_header_field, cbor_header,
                                /*devtools_proxy=*/nullptr);
  return 0;
}

}  // namespace signed_exchange_prologue

}  // namespace content
