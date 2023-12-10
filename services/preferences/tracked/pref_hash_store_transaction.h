// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_TRANSACTION_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_TRANSACTION_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

// Used to perform a series of checks/transformations on a PrefHashStore.
class PrefHashStoreTransaction {
 public:
  // Finalizes any remaining work after the transaction has been performed.
  virtual ~PrefHashStoreTransaction() {}

  // Returns the suffix to be appended to UMA histograms for the store contained
  // in this transaction.
  virtual std::string_view GetStoreUMASuffix() const = 0;

  // Checks |initial_value| against the existing stored value hash.
  virtual prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
  CheckValue(const std::string& path,
             const base::Value* initial_value) const = 0;

  // Stores a hash of the current |value| of the preference at |path|.
  virtual void StoreHash(const std::string& path, const base::Value* value) = 0;

  // Checks |initial_value| against the existing stored hashes for the split
  // preference at |path|. |initial_split_value| being an empty dictionary or
  // NULL is equivalent. |invalid_keys| must initially be empty. |invalid_keys|
  // will not be modified unless the return value is CHANGED, in which case it
  // will be filled with the keys that are considered invalid (unknown or
  // changed).
  virtual prefs::mojom::TrackedPreferenceValidationDelegate::ValueState
  CheckSplitValue(const std::string& path,
                  const base::Value::Dict* initial_split_value,
                  std::vector<std::string>* invalid_keys) const = 0;

  // Stores hashes for the |value| of the split preference at |path|.
  // |split_value| being an empty dictionary or NULL is equivalent.
  virtual void StoreSplitHash(const std::string& path,
                              const base::Value::Dict* split_value) = 0;

  // Indicates whether the store contains a hash for the preference at |path|.
  virtual bool HasHash(const std::string& path) const = 0;

  // Sets the hash for the preference at |path|.
  // If |path| is a split preference |hash| must be a DictionaryValue whose
  // keys are keys in the split preference and whose values are MACs of the
  // corresponding values in the split preference.
  // If |path| is an atomic preference |hash| must be a StringValue
  // containing a MAC of the preference value.
  // |hash| should originate from a PrefHashStore sharing the same MAC
  // parameters as this transaction's store.
  // The (in)validity of the super MAC will be maintained by this call.
  virtual void ImportHash(const std::string& path, const base::Value* hash) = 0;

  // Removes the hash stored at |path|. The (in)validity of the super MAC will
  // be maintained by this call.
  virtual void ClearHash(const std::string& path) = 0;

  // Indicates whether the super MAC was successfully verified at the beginning
  // of this transaction.
  virtual bool IsSuperMACValid() const = 0;

  // Forces a valid super MAC to be stored when this transaction terminates.
  // Returns true if this results in a change to the store contents.
  virtual bool StampSuperMac() = 0;
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_STORE_TRANSACTION_H_
