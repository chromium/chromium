// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KEY_PINNING_STATE_H_
#define NET_CERT_KEY_PINNING_STATE_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/hash.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/net_buildflags.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace net {

class NET_EXPORT KeyPinningState
    : public base::RefCountedThreadSafe<KeyPinningState> {
 public:
  using HashedHost = std::array<uint8_t, crypto::hash::kSha256Size>;

  // A PKPState describes the public key pinning state for "dynamic" PKP.
  // TODO(crbug.com/497882860): PKPState is also used as a helper in the
  // "static" implementation, though it's not really necessary and somewhat
  // inefficient to create and populate just to call one method on it and then
  // destroy it. Consider refactoring to avoid that.
  class NET_EXPORT PKPState {
   public:
    PKPState();
    PKPState(const PKPState& other);
    ~PKPState();

    // The absolute time (UTC) when the |spki_hashes| expire.
    base::Time expiry;

    // Optional; hashes of pinned SubjectPublicKeyInfos.
    absl::flat_hash_set<SHA256HashValue> spki_hashes;

    // Optional; hashes of static known-bad SubjectPublicKeyInfos which MUST
    // NOT intersect with the set of SPKIs in the TLS server's certificate
    // chain.
    absl::flat_hash_set<SHA256HashValue> bad_spki_hashes;

    // Are subdomains subject to this policy state?
    bool include_subdomains = false;

    // Takes a set of SubjectPublicKeyInfo |hashes| and returns true if:
    //   1) |bad_static_spki_hashes| does not intersect |hashes|; AND
    //   2) Both |static_spki_hashes| and |dynamic_spki_hashes| are empty
    //      or at least one of them intersects |hashes|.
    //
    // |{dynamic,static}_spki_hashes| contain trustworthy public key hashes,
    // any one of which is sufficient to validate the certificate chain in
    // question. The public keys could be of a root CA, intermediate CA, or
    // leaf certificate, depending on the security vs. disaster recovery
    // tradeoff selected. (Pinning only to leaf certificates increases
    // security because you no longer trust any CAs, but it hampers disaster
    // recovery because you can't just get a new certificate signed by the
    // CA.)
    //
    // |bad_static_spki_hashes| contains public keys that we don't want to
    // trust.
    bool CheckPublicKeyPins(const std::vector<SHA256HashValue>& hashes) const;

    // Returns true if any of the HashValueVectors |static_spki_hashes|,
    // |bad_static_spki_hashes|, or |dynamic_spki_hashes| contains any
    // items.
    bool HasPublicKeyPins() const;
  };

  // PKPStatus describes the result of a pinning check.
  enum class PKPStatus {
    // Pinning was enabled and the necessary pins were not present.
    VIOLATED,

    // Pinning was not enabled, or pinning was enabled and the certificate
    // satisfied the pins.
    OK,

    // Pinning was enabled and the certificate did not satisfy the pins, but the
    // violation was ignored due to local policy, such as a local trust anchor.
    BYPASSED,
  };

  // Checks whether `host` is covered by the configured pins, and if so,
  // whether `public_key_hashes` satisfies the pinned keys.
  PKPStatus CheckPublicKeyPins(
      std::string_view host,
      bool is_issued_by_known_root,
      const std::vector<SHA256HashValue>& public_key_hashes) const;

  // Returns true if `host` is present in the configured pins (either directly
  // or as a subdomain of an `include_subdomains` rule), and has pinned SPKI
  // hashes.
  bool HasPublicKeyPins(std::string_view host) const;

  // Returns true and updates |*result| iff |host| has HPKP state. If multiple
  // entries match |host|, the most specific match determines the return value.
  virtual bool GetPKPState(std::string_view host,
                           PKPState* pkp_result) const = 0;

#if BUILDFLAG(CRONET_BUILD)
  // Enables or disables public key pinning bypass for local trust anchors.
  // Disabling the bypass for local trust anchors is highly discouraged.
  // This method is used by Cronet only and *** MUST NOT *** be used by any
  // other consumer. For more information see "How does key pinning interact
  // with local proxies and filters?" at
  // https://www.chromium.org/Home/chromium-security/security-faq
  // TODO(crbug.com/497882860): This is only ever changed on cronet, but it's
  // implemented on the base class. Consider refactoring so the base class
  // doesn't need to know about this.
  void SetEnablePublicKeyPinningBypassForLocalTrustAnchors(bool value) {
    enable_pkp_bypass_for_local_trust_anchors_ = value;
  }
#endif

 protected:
  friend class base::RefCountedThreadSafe<KeyPinningState>;

  KeyPinningState();
  virtual ~KeyPinningState();

  static std::vector<uint8_t> CanonicalizeHost(std::string_view host);

 private:
  // Helper method for actually checking pins.
  PKPStatus CheckPublicKeyPinsImpl(
      std::string_view host,
      bool is_issued_by_known_root,
      const std::vector<SHA256HashValue>& hashes) const;

  // True if public key pinning bypass is enabled for local trust anchors.
  bool enable_pkp_bypass_for_local_trust_anchors_ = true;
};

// A dummy implementation that never enforces pinning.
class NET_EXPORT KeyPinningStateDummy : public KeyPinningState {
 public:
  bool GetPKPState(std::string_view host, PKPState* pkp_result) const override;

 protected:
  ~KeyPinningStateDummy() override;
};

}  // namespace net

#endif  // NET_CERT_KEY_PINNING_STATE_H_
