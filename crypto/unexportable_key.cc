// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include "base/bind.h"
#include "base/check.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#endif  // BUILDFLAG(IS_WIN)

namespace crypto {

namespace {
std::unique_ptr<UnexportableKeyProvider> (*g_mock_provider)() = nullptr;
}  // namespace

UnexportableSigningKey::~UnexportableSigningKey() = default;
UnexportableKeyProvider::~UnexportableKeyProvider() = default;

#if BUILDFLAG(IS_WIN)
std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderWin();
#endif

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProvider() {
  if (g_mock_provider) {
    return g_mock_provider();
  }

#if BUILDFLAG(IS_WIN)
  return GetUnexportableKeyProviderWin();
#else
  return nullptr;
#endif
}

#if BUILDFLAG(IS_WIN)
void MeasureTPMAvailabilityWin() {
  // Measure the fraction of Windows machines that have TPMs, and what the best
  // supported algorithm is.
  base::ThreadPool::PostTask(
      // GetUnexportableKeyProvider can call functions that take the global
      // loader lock, so although BEST_EFFORT makes it low priority to start,
      // once it starts it must run in a foreground thread to avoid priority
      // inversions.
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::ThreadPolicy::MUST_USE_FOREGROUND},
      base::BindOnce([]() {
        // Note that values here are used in a recorded histogram. Don't change
        // the values of existing members.
        enum TPMSupport {
          kNone = 0,
          kRSA = 1,
          kECDSA = 2,
          kMaxValue = 2,
        };

        TPMSupport result = TPMSupport::kNone;
        std::unique_ptr<UnexportableKeyProvider> provider =
            GetUnexportableKeyProvider();
        if (provider) {
          const SignatureVerifier::SignatureAlgorithm kAllAlgorithms[] = {
              SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
              SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
          };
          auto algo = provider->SelectAlgorithm(kAllAlgorithms);
          if (algo) {
            switch (*algo) {
              case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
                result = TPMSupport::kECDSA;
                break;
              case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
                result = TPMSupport::kRSA;
                break;
              case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
              case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
                // Not supported for this metric.
                break;
            }
          }
        }
        // This metric was previously named Crypto.TPMSupport but that expired,
        // so using a new name to avoid mixing up with old data.
        base::UmaHistogramEnumeration("Crypto.TPMSupport2", result);
      }));
}
#endif  // BUILDFLAG(IS_WIN)

namespace internal {

void SetUnexportableKeyProviderForTesting(
    std::unique_ptr<UnexportableKeyProvider> (*func)()) {
  if (g_mock_provider) {
    // Nesting ScopedMockUnexportableSigningKeyForTesting is not supported.
    CHECK(!func);
    g_mock_provider = nullptr;
  } else {
    g_mock_provider = func;
  }
}

}  // namespace internal
}  // namespace crypto
