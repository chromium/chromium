// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/crypto.h"

#include "base/numerics/safe_conversions.h"
#include "crypto/openssl_util.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

Digestor::Digestor(HashAlgorithm algorithm) {
  crypto::EnsureOpenSSLInit();
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const EVP_MD* evp_md = nullptr;
  switch (algorithm) {
    case kHashAlgorithmSha1:
      evp_md = EVP_sha1();
      break;
    case kHashAlgorithmSha256:
      evp_md = EVP_sha256();
      break;
    case kHashAlgorithmSha384:
      evp_md = EVP_sha384();
      break;
    case kHashAlgorithmSha512:
      evp_md = EVP_sha512();
      break;
  }

  has_failed_ =
      !evp_md || !EVP_DigestInit_ex(digest_context_.get(), evp_md, nullptr);
}

Digestor::~Digestor() = default;

bool Digestor::Update(base::span<const uint8_t> data) {
  if (has_failed_)
    return false;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  has_failed_ =
      !EVP_DigestUpdate(digest_context_.get(), data.data(), data.size());
  return !has_failed_;
}

bool Digestor::UpdateUtf8(const String& string, WTF::UTF8ConversionMode mode) {
  StringUTF8Adaptor utf8(string, mode);
  return Update(base::as_bytes(base::make_span(utf8)));
}

bool Digestor::Finish(DigestValue& digest_result) {
  if (has_failed_)
    return false;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  const size_t expected_size = EVP_MD_CTX_size(digest_context_.get());
  DCHECK_LE(expected_size, static_cast<size_t>(EVP_MAX_MD_SIZE));
  digest_result.resize(base::checked_cast<wtf_size_t>(expected_size));

  unsigned result_size;
  has_failed_ = !EVP_DigestFinal_ex(digest_context_.get(), digest_result.data(),
                                    &result_size) ||
                result_size != expected_size;
  return !has_failed_;
}

bool ComputeDigest(HashAlgorithm algorithm,
                   const char* digestable,
                   size_t length,
                   DigestValue& digest_result) {
  Digestor digestor(algorithm);
  digestor.Update(base::as_bytes(base::make_span(digestable, length)));
  digestor.Finish(digest_result);
  return !digestor.has_failed();
}

}  // namespace blink
