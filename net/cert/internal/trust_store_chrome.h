// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_
#define NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/version.h"
#include "net/base/net_export.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace net {

// Represents a ConstraintSet for compiled-in version of the root store.
// This is a separate struct from ChromeRootCertConstraints since the in-memory
// representation parses the version constraints into a base::Version.
// (base::Version can't be used in the compiled-in version since it isn't
// constexpr.)
struct StaticChromeRootCertConstraints {
  std::optional<base::Time> sct_not_after;
  std::optional<base::Time> sct_all_after;

  std::optional<std::string_view> min_version;
  std::optional<std::string_view> max_version_exclusive;
};

struct ChromeRootCertInfo {
  base::span<const uint8_t> root_cert_der;
  base::span<const StaticChromeRootCertConstraints> constraints;
};

struct NET_EXPORT ChromeRootCertConstraints {
  ChromeRootCertConstraints(std::optional<base::Time> sct_not_after,
                            std::optional<base::Time> sct_all_after,
                            std::optional<base::Version> min_version,
                            std::optional<base::Version> max_version_exclusive);
  explicit ChromeRootCertConstraints(
      const StaticChromeRootCertConstraints& constraints);
  ~ChromeRootCertConstraints();
  ChromeRootCertConstraints(const ChromeRootCertConstraints& other);
  ChromeRootCertConstraints(ChromeRootCertConstraints&& other);
  ChromeRootCertConstraints& operator=(const ChromeRootCertConstraints& other);
  ChromeRootCertConstraints& operator=(ChromeRootCertConstraints&& other);

  std::optional<base::Time> sct_not_after;
  std::optional<base::Time> sct_all_after;

  std::optional<base::Version> min_version;
  std::optional<base::Version> max_version_exclusive;
};

// ChromeRootStoreData is a container class that stores all of the Chrome Root
// Store data in a single class.
class NET_EXPORT ChromeRootStoreData {
 public:
  struct NET_EXPORT Anchor {
    Anchor(std::shared_ptr<const bssl::ParsedCertificate> certificate,
           std::vector<ChromeRootCertConstraints> constraints);
    ~Anchor();

    Anchor(const Anchor& other);
    Anchor(Anchor&& other);
    Anchor& operator=(const Anchor& other);
    Anchor& operator=(Anchor&& other);

    std::shared_ptr<const bssl::ParsedCertificate> certificate;
    std::vector<ChromeRootCertConstraints> constraints;
  };
  // CreateChromeRootStoreData converts |proto| into a usable
  // ChromeRootStoreData object. Returns std::nullopt if the passed in
  // proto has errors in it (e.g. an unparsable DER-encoded certificate).
  static std::optional<ChromeRootStoreData> CreateChromeRootStoreData(
      const chrome_root_store::RootStore& proto);
  ~ChromeRootStoreData();

  ChromeRootStoreData(const ChromeRootStoreData& other);
  ChromeRootStoreData(ChromeRootStoreData&& other);
  ChromeRootStoreData& operator=(const ChromeRootStoreData& other);
  ChromeRootStoreData& operator=(ChromeRootStoreData&& other);

  const std::vector<Anchor>& anchors() const { return anchors_; }
  int64_t version() const { return version_; }

 private:
  ChromeRootStoreData();

  std::vector<Anchor> anchors_;
  int64_t version_;
};

// TrustStoreChrome contains the Chrome Root Store, as described at
// https://g.co/chrome/root-policy
class NET_EXPORT TrustStoreChrome : public bssl::TrustStore {
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

  // bssl::TrustStore implementation:
  void SyncGetIssuersOf(const bssl::ParsedCertificate* cert,
                        bssl::ParsedCertificateList* issuers) override;
  bssl::CertificateTrust GetTrust(const bssl::ParsedCertificate* cert) override;

  // Returns true if the trust store contains the given bssl::ParsedCertificate
  // (matches by DER).
  bool Contains(const bssl::ParsedCertificate* cert) const;

  // Returns the root store constraints for `cert`, or an empty span if the
  // certificate is not constrained.
  base::span<const ChromeRootCertConstraints> GetConstraintsForCert(
      const bssl::ParsedCertificate* cert) const;

  int64_t version() const { return version_; }

 private:
  TrustStoreChrome(base::span<const ChromeRootCertInfo> certs,
                   bool certs_are_static,
                   int64_t version);
  bssl::TrustStoreInMemory trust_store_;
  // Map from certificate DER bytes to additional constraints (if any) for that
  // certificate. The DER bytes of the key are owned by the ParsedCertificate
  // stored in `trust_store_`, so this must be below `trust_store_` in the
  // member list.
  base::flat_map<std::string_view, std::vector<ChromeRootCertConstraints>>
      constraints_;
  int64_t version_;
};

// Returns the version # of the Chrome Root Store that was compiled into the
// binary.
NET_EXPORT int64_t CompiledChromeRootStoreVersion();

// Returns the anchors of the Chrome Root Store that were compiled into the
// binary.
NET_EXPORT std::vector<ChromeRootStoreData::Anchor>
CompiledChromeRootStoreAnchors();

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_CHROME_H_
