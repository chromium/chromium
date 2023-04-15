// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_BUILTIN_H_
#define NET_CERT_CERT_VERIFY_PROC_BUILTIN_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/der/parse_values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

class CertNetFetcher;
class CertVerifyProc;
class CRLSet;
class SystemTrustStore;

class NET_EXPORT CertVerifyProcBuiltinResultDebugData
    : public base::SupportsUserData::Data {
 public:
  CertVerifyProcBuiltinResultDebugData(
      base::Time verification_time,
      const der::GeneralizedTime& der_verification_time,
      absl::optional<int64_t> chrome_root_store_version);

  static const CertVerifyProcBuiltinResultDebugData* Get(
      const base::SupportsUserData* debug_data);
  static void Create(base::SupportsUserData* debug_data,
                     base::Time verification_time,
                     const der::GeneralizedTime& der_verification_time,
                     absl::optional<int64_t> chrome_root_store_version);

  // base::SupportsUserData::Data implementation:
  std::unique_ptr<Data> Clone() override;

  base::Time verification_time() const { return verification_time_; }
  const der::GeneralizedTime& der_verification_time() const {
    return der_verification_time_;
  }
  absl::optional<int64_t> chrome_root_store_version() const {
    return chrome_root_store_version_;
  }

 private:
  base::Time verification_time_;
  der::GeneralizedTime der_verification_time_;
  absl::optional<int64_t> chrome_root_store_version_;
};

// TODO(crbug.com/649017): This is not how other cert_verify_proc_*.h are
// implemented -- they expose the type in the header. Use a consistent style
// here too.
NET_EXPORT scoped_refptr<CertVerifyProc> CreateCertVerifyProcBuiltin(
    scoped_refptr<CertNetFetcher> net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    std::unique_ptr<SystemTrustStore> system_trust_store);

// Returns the time limit used by CertVerifyProcBuiltin. Intended for test use.
NET_EXPORT_PRIVATE base::TimeDelta
GetCertVerifyProcBuiltinTimeLimitForTesting();

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_BUILTIN_H_
