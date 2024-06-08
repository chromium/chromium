// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/at_exit.h"
#include "base/containers/span.h"
#include "base/i18n/icu_util.h"
#include "content/browser/web_package/signed_exchange_envelope.h"  // nogncheck
#include "content/browser/web_package/signed_exchange_prologue.h"  // nogncheck

namespace content {

namespace signed_exchange_prologue {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

std::vector<uint8_t> CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(
    base::span<const uint8_t> data) {
  return std::vector<uint8_t>(data.begin(), data.end());
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < BeforeFallbackUrl::kEncodedSizeInBytes)
    return 0;
  auto before_fallback_url_bytes =
      CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(
          base::make_span(data, BeforeFallbackUrl::kEncodedSizeInBytes));
  auto before_fallback_url = BeforeFallbackUrl::Parse(
      base::make_span(before_fallback_url_bytes), nullptr /* devtools_proxy */);

  data += BeforeFallbackUrl::kEncodedSizeInBytes;
  size -= BeforeFallbackUrl::kEncodedSizeInBytes;

  size_t fallback_url_and_after_length =
      before_fallback_url.ComputeFallbackUrlAndAfterLength();
  if (size < fallback_url_and_after_length)
    return 0;

  auto fallback_url_and_after_bytes =
      CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(
          base::make_span(data, fallback_url_and_after_length));
  auto fallback_url_and_after = FallbackUrlAndAfter::Parse(
      base::make_span(fallback_url_and_after_bytes), before_fallback_url,
      nullptr /* devtools_proxy */);
  if (!fallback_url_and_after.is_valid())
    return 0;

  data += fallback_url_and_after_length;
  size -= fallback_url_and_after_length;

  std::string signature_header_field(
      reinterpret_cast<const char*>(data),
      std::min(size, fallback_url_and_after.signature_header_field_length()));
  data += signature_header_field.size();
  size -= signature_header_field.size();

  auto cbor_header =
      CopyIntoSeparateBufferToSurfaceOutOfBoundAccess(base::make_span(
          data, std::min(size, fallback_url_and_after.cbor_header_length())));

  SignedExchangeEnvelope::Parse(
      SignedExchangeVersion::kB3, fallback_url_and_after.fallback_url(),
      signature_header_field, base::make_span(cbor_header),
      nullptr /* devtools_proxy */);
  return 0;
}

}  // namespace signed_exchange_prologue

}  // namespace content
