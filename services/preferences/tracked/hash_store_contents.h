// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_HASH_STORE_CONTENTS_H_
#define SERVICES_PREFERENCES_TRACKED_HASH_STORE_CONTENTS_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/values.h"

// Provides access to the contents of a preference hash store. The store
// contains the following data:
// Contents: a client-defined dictionary that should map preference names to
// MACs.
// Version: a client-defined version number for the format of Contents.
// Super MAC: a MAC that authenticates the entirety of Contents.
class HashStoreContents {
 public:
  virtual ~HashStoreContents() {}

  // Returns true if this implementation of HashStoreContents can be copied via
  // MakeCopy().
  virtual bool IsCopyable() const = 0;

  // Returns a copy of this HashStoreContents. Must only be called on
  // lightweight implementations (which return true from IsCopyable()) and only
  // in scenarios where a copy cannot be avoided.
  virtual std::unique_ptr<HashStoreContents> MakeCopy() const = 0;

  // Returns the suffix to be appended to UMA histograms for this store type.
  // The returned value must either be an empty string or one of the values in
  // histograms.xml's TrackedPreferencesExternalValidators.
  virtual std::string_view GetUMASuffix() const = 0;

  // Discards all data related to this hash store.
  virtual void Reset() = 0;

  // Outputs the MAC validating the preference at path. Returns true if a MAC
  // was successfully read and false otherwise.
  virtual bool GetMac(const std::string& path, std::string* out_value) = 0;

  // Outputs the MACS validating the split preference at path. Returns true if
  // MACS were successfully read and false otherwise.
  virtual bool GetSplitMacs(const std::string& path,
                            std::map<std::string, std::string>* out_value) = 0;

  // Set the MAC validating the preference at path.
  virtual void SetMac(const std::string& path, const std::string& value) = 0;

  // Set the MAC validating the split preference at path and split_path.
  // For example, |path| is 'extension' and |split_path| is some extenson id.
  virtual void SetSplitMac(const std::string& path,
                           const std::string& split_path,
                           const std::string& value) = 0;

  // Sets the MAC for the preference at |path|.
  // If |path| is a split preference |in_value| must be a DictionaryValue whose
  // keys are keys in the split preference and whose values are MACs of the
  // corresponding values in the split preference.
  // If |path| is an atomic preference |in_value| must be a StringValue
  // containing a MAC of the preference value.
  virtual void ImportEntry(const std::string& path,
                           const base::Value* in_value) = 0;

  // Removes the MAC (for atomic preferences) or MACs (for split preferences)
  // at |path|. Returns true if there was an entry at |path| which was
  // successfully removed.
  virtual bool RemoveEntry(const std::string& path) = 0;

  // Only needed if this store supports super MACs.
  virtual const base::Value::Dict* GetContents() const = 0;

  // Retrieves the super MAC value previously stored by SetSuperMac. May be
  // empty if no super MAC has been stored or if this store does not support
  // super MACs.
  virtual std::string GetSuperMac() const = 0;

  // Stores a super MAC value for this hash store.
  virtual void SetSuperMac(const std::string& super_mac) = 0;
};

#endif  // SERVICES_PREFERENCES_TRACKED_HASH_STORE_CONTENTS_H_
