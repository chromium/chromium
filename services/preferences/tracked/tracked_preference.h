// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCE_H_
#define SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCE_H_

#include "base/values.h"

class PrefHashStoreTransaction;
namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

enum class TrackedPreferenceType { ATOMIC, SPLIT };

// A TrackedPreference tracks changes to an individual preference, reporting and
// reacting to them according to preference-specific and browser-wide policies.
class TrackedPreference {
 public:
  virtual ~TrackedPreference() {}

  virtual TrackedPreferenceType GetType() const = 0;

  // Gets the reporting ID for this preference.
  virtual size_t GetReportingId() const = 0;

  // Notifies the underlying TrackedPreference about its new |value| which
  // can update hashes in the corresponding hash store via |transaction|.
  virtual void OnNewValue(const base::Value* value,
                          PrefHashStoreTransaction* transaction,
                          const os_crypt_async::Encryptor* encryptor) const = 0;

  // Verifies that the value of this TrackedPreference in |pref_store_contents|
  // is valid. Responds to verification failures according to
  // preference-specific and browser-wide policy and reports results to via UMA.
  // May use |transaction| to check/modify hashes in the corresponding hash
  // store. Performs validation and reports results without enforcing for
  // |external_validation_transaction|. This call assumes exclusive access to
  // |external_validation_transaction| and its associated state and as such
  // should only be called before any other subsystem is made aware of it.
  // |encryptor| is an optional OS-level encryptor used for an additional
  // encrypted hash if the kEncryptedPrefHashing feature is enabled.
  virtual bool EnforceAndReport(
      base::Value::Dict& pref_store_contents,
      PrefHashStoreTransaction* transaction,
      PrefHashStoreTransaction* external_validation_transaction,
      const os_crypt_async::Encryptor* encryptor) const = 0;

  // Compatibility Overload
  // This calls the primary virtual method OnNewValue above, passing nullptr for
  // encryptor.
  void OnNewValue(const base::Value* value,
                  PrefHashStoreTransaction* transaction) const {
    OnNewValue(value, transaction, nullptr /*encryptor*/);
  }
  // Compatibility Overload.
  // This calls the primary virtual method EnforceAndReport above, passing
  // nullptr for encryptor.
  bool EnforceAndReport(
      base::Value::Dict& pref_store_contents,
      PrefHashStoreTransaction* transaction,
      PrefHashStoreTransaction* external_validation_transaction) const {
    return EnforceAndReport(pref_store_contents, transaction,
                            external_validation_transaction,
                            nullptr /*encryptor*/);
  }
};

#endif  // SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCE_H_
