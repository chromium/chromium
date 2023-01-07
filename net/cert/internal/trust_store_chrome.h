// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_
#define NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_

#include "base/containers/span.h"
#include "net/base/net_export.h"
#include "net/cert/pki/trust_store.h"
#include "net/cert/pki/trust_store_in_memory.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

struct ChromeRootCertInfo {
  base::span<const uint8_t> root_cert_der;
};

// ChromeRootStoreData is a container class that stores all of the Chrome Root
// Store data in a single class.
class NET_EXPORT ChromeRootStoreData {
 public:
  // CreateChromeRootStoreData converts |proto| into a usable
  // ChromeRootStoreData object. Returns absl::nullopt if the passed in
  // proto has errors in it (e.g. an unparsable DER-encoded certificate).
  static absl::optional<ChromeRootStoreData> CreateChromeRootStoreData(
      const chrome_root_store::RootStore& proto);
  ~ChromeRootStoreData();

  ChromeRootStoreData(const ChromeRootStoreData& other);
  ChromeRootStoreData(ChromeRootStoreData&& other);
  ChromeRootStoreData& operator=(const ChromeRootStoreData& other);
  ChromeRootStoreData& operator=(ChromeRootStoreData&& other);

  const ParsedCertificateList anchors() const { return anchors_; }
  int64_t version() const { return version_; }

 private:
  ChromeRootStoreData();

  ParsedCertificateList anchors_;
  int64_t version_;
};

// TrustStoreChrome contains the Chrome Root Store, as described at
// https://g.co/chrome/root-policy
class NET_EXPORT TrustStoreChrome : public TrustStore {
 public:
  // Creates a TrustStoreChrome that uses a copy of `certs`, instead of the
  // default Chrome Root Store.
  static std::unique_ptr<TrustStoreChrome> CreateTrustStoreForTesting(
      base::span<const ChromeRootCertInfo> certs,
      int64_t version);

  // Creates a TrustStoreChrome that uses the compiled in Chrome Root Store.
  TrustStoreChrome();

  // Creates a TrustStoreChrome that uses the passed in anchors as
  // the contents of the Chrome Root Store.
  TrustStoreChrome(const ChromeRootStoreData& anchors);
  ~TrustStoreChrome() override;

  TrustStoreChrome(const TrustStoreChrome& other) = delete;
  TrustStoreChrome& operator=(const TrustStoreChrome& other) = delete;

  // TrustStore implementation:
  void SyncGetIssuersOf(const ParsedCertificate* cert,
                        ParsedCertificateList* issuers) override;
  CertificateTrust GetTrust(const ParsedCertificate* cert,
                            base::SupportsUserData* debug_data) const override;

  // Returns true if the trust store contains the given ParsedCertificate
  // (matches by DER).
  bool Contains(const ParsedCertificate* cert) const;

  int64_t version() const { return version_; }

 private:
  TrustStoreChrome(base::span<const ChromeRootCertInfo> certs,
                   bool certs_are_static,
                   int64_t version);
  TrustStoreInMemory trust_store_;
  int64_t version_;
};

// Returns the version # of the Chrome Root Store that was compiled into the
// binary.
NET_EXPORT int64_t CompiledChromeRootStoreVersion();

// Returns the anchors of the Chrome Root Store that were compiled into the
// binary.
NET_EXPORT ParsedCertificateList CompiledChromeRootStoreAnchors();

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_
