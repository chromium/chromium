// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "components/value_store/value_store.h"

namespace extensions {

// A ValueStore decorator which makes calls through |Set| ignore quota.
// "Weak" because ownership of the delegate isn't taken; this is designed to be
// temporarily attached to storage areas.
class WeakUnlimitedSettingsStorage : public value_store::ValueStore {
 public:
  // Ownership of |delegate| NOT taken.
  explicit WeakUnlimitedSettingsStorage(value_store::ValueStore* delegate);

  WeakUnlimitedSettingsStorage(const WeakUnlimitedSettingsStorage&) = delete;
  WeakUnlimitedSettingsStorage& operator=(const WeakUnlimitedSettingsStorage&) =
      delete;

  ~WeakUnlimitedSettingsStorage() override;

  // ValueStore implementation.
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;
  ReadResult Get(const std::string& key) override;
  ReadResult Get(const std::vector<std::string>& keys) override;
  ReadResult Get() override;
  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override;
  WriteResult Set(WriteOptions options,
                  const base::Value::Dict& values) override;
  WriteResult Remove(const std::string& key) override;
  WriteResult Remove(const std::vector<std::string>& keys) override;
  WriteResult Clear() override;

 private:
  // The delegate storage area, NOT OWNED.
  const raw_ptr<value_store::ValueStore> delegate_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_WEAK_UNLIMITED_SETTINGS_STORAGE_H_
