// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KEY_PINNING_STATE_CHROME_H_
#define NET_CERT_KEY_PINNING_STATE_CHROME_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/key_pinning_state.h"

namespace net {

struct TransportSecurityStatePinsSource;

// Tracks which hosts have key pinning enabled.
//
// In builds where static key pinning is enabled, the key pins are provided by
// a static list compiled in to the binary, which may be updated at runtime via
// component updater. (It's still referred to as "static" key pinning whether it
// is using the compiled-in or component updated data.)
// TODO(crbug.com/497882860): come up with better terminology for "static" key
// pinning. It's confusing that the "static" data can actually be updated data
// from component updater.
class NET_EXPORT KeyPinningStateChrome : public KeyPinningState {
 public:
  class NET_EXPORT PinSet {
   public:
    PinSet(std::string name,
           std::vector<SHA256HashValue> static_spki_hashes,
           std::vector<SHA256HashValue> bad_static_spki_hashes);
    PinSet(const PinSet& other);
    ~PinSet();

    const std::string& name() const { return name_; }
    const std::vector<SHA256HashValue>& static_spki_hashes() const {
      return static_spki_hashes_;
    }
    const std::vector<SHA256HashValue>& bad_static_spki_hashes() const {
      return bad_static_spki_hashes_;
    }

   private:
    std::string name_;
    std::vector<SHA256HashValue> static_spki_hashes_;
    std::vector<SHA256HashValue> bad_static_spki_hashes_;
  };

  struct NET_EXPORT PinSetInfo {
    std::string hostname_;
    std::string pinset_name_;
    bool include_subdomains_;

    PinSetInfo(std::string hostname,
               std::string pinset_name,
               bool include_subdomains);
  };

  // Construct a KeyPinningStateChrome that uses the compiled-in static key
  // pinning information.
  static scoped_refptr<KeyPinningStateChrome> CreateForCompiledIn();

  // Construct a KeyPinningStateChrome that uses component-updater supplied key
  // pinning information.
  // |pinsets| should include all known pinsets, |host_pins| the information
  // related to each hostname's pin, and |update_time| the time at which this
  // list was known to be up to date.
  static scoped_refptr<KeyPinningStateChrome> CreateForUpdated(
      const std::vector<PinSet>& pinsets,
      const std::vector<PinSetInfo>& host_pins,
      base::Time update_time);

  // KeyPinningState implementation:
  bool GetPKPState(std::string_view host, PKPState* pkp_result) const override;

  static base::Time GetBuiltInPinsListTimestamp();

 protected:
  ~KeyPinningStateChrome() override;

  virtual base::Time GetPinListTimestamp() const = 0;
  virtual bool GetPKPStateImpl(std::string_view normalized_host,
                               PKPState* pkp_result) const = 0;

 private:
  // Returns true if the static key pinning list has been updated in the last 10
  // weeks.
  bool IsStaticPKPListTimely() const;

  // Calculates the default states to initialize with, should only be called
  // during construction.
  static bool InitDefaultEnableState();
  static bool InitDefaultPinsListAlwaysTimelyState();

  // True if static pins should be used.
  const bool enable_static_pins_ = InitDefaultEnableState();

  // True if static pins should be considered always up to date, should be true
  // for tests only.
  const bool pins_list_always_timely_for_testing_ =
      InitDefaultPinsListAlwaysTimelyState();
};

// Do not call these methods directly, use ScopedKeyPinningStateChromeDefaults.
void NET_EXPORT_PRIVATE SetKeyPinningStateChromeOverridesForTesting(
    const TransportSecurityStatePinsSource* source,
    bool enabled,
    bool pins_list_always_timely_for_testing);
void NET_EXPORT_PRIVATE ClearKeyPinningStateChromeOverridesForTesting();

}  // namespace net

#endif  // NET_CERT_KEY_PINNING_STATE_CHROME_H_
