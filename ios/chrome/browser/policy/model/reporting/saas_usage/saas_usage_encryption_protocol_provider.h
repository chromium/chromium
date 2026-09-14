// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_ENCRYPTION_PROTOCOL_PROVIDER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_ENCRYPTION_PROTOCOL_PROVIDER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "url/gurl.h"

namespace enterprise_reporting {

// Singleton that determines TLS encryption protocol versions on iOS by sending
// and inspecting HEAD requests. Caches resolved domains in memory.
class SaasUsageEncryptionProtocolProvider {
 public:
  using EncryptionProtocolCallback = base::OnceCallback<void(std::string_view)>;

  // Delegate class for the HEAD requests.
  class Prober {
   public:
    virtual ~Prober() = default;
    virtual void ProbeUrl(const GURL& url,
                          EncryptionProtocolCallback callback) = 0;
  };

  static SaasUsageEncryptionProtocolProvider& GetInstance();

  SaasUsageEncryptionProtocolProvider(
      const SaasUsageEncryptionProtocolProvider&) = delete;
  SaasUsageEncryptionProtocolProvider& operator=(
      const SaasUsageEncryptionProtocolProvider&) = delete;

  // Resolves the encryption protocol version for `url`.
  void GetEncryptionProtocol(const GURL& url,
                             EncryptionProtocolCallback callback);

  // Sets the prober used to send probe requests. If `prober` is null, resets
  // to the default prober implementation.
  void SetProberForTesting(std::unique_ptr<Prober> prober = nullptr);

 private:
  friend class base::NoDestructor<SaasUsageEncryptionProtocolProvider>;

  SaasUsageEncryptionProtocolProvider();
  ~SaasUsageEncryptionProtocolProvider();

  void OnProbeComplete(std::string domain, std::string_view protocol);

  std::unique_ptr<Prober> prober_;

  // Domain -> protocol mapping to avoid sending multiple HEAD requests per
  // domain.
  absl::flat_hash_map<std::string, std::string> cache_;
  absl::flat_hash_map<std::string, std::vector<EncryptionProtocolCallback>>
      pending_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SaasUsageEncryptionProtocolProvider> weak_ptr_factory_{
      this};
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_ENCRYPTION_PROTOCOL_PROVIDER_H_
