// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_DICTIONARY_VALUE_UPDATE_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_DICTIONARY_VALUE_UPDATE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/values.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace prefs {

// A wrapper around base::Value::Dict that reports changes to its contents
// via a callback.
class DictionaryValueUpdate {
 public:
  using UpdateCallback =
      base::RepeatingCallback<void(std::vector<std::string>)>;

  DictionaryValueUpdate(UpdateCallback report_update,
                        base::Value::Dict* value,
                        std::vector<std::string> path);

  DictionaryValueUpdate(const DictionaryValueUpdate&) = delete;
  DictionaryValueUpdate& operator=(const DictionaryValueUpdate&) = delete;

  ~DictionaryValueUpdate();
  bool HasKey(std::string_view key) const;

  // Returns the number of Values in this dictionary.
  size_t size() const;

  // Returns whether the dictionary is empty.
  bool empty() const;

  // Clears any current contents of this dictionary.
  void Clear();

  // Sets the Value associated with the given path starting from this object.
  // A path has the form "<key>" or "<key>.<key>.[...]", where "." indexes
  // into the next DictionaryValue down.  Obviously, "." can't be used
  // within a key, but there are no other restrictions on keys.
  // If the key at any step of the way doesn't exist, or exists but isn't
  // a DictionaryValue, a new DictionaryValue will be created and attached
  // to the path in that location.
  void Set(std::string_view path, base::Value in_value);

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  void SetBoolean(std::string_view path, bool in_value);
  void SetInteger(std::string_view path, int in_value);
  void SetDouble(std::string_view path, double in_value);
  void SetString(std::string_view path, std::string_view in_value);
  void SetString(std::string_view path, const std::u16string& in_value);
  std::unique_ptr<DictionaryValueUpdate> SetDictionary(
      std::string_view path,
      base::Value::Dict in_value);

  // Like Set(), but without special treatment of '.'.  This allows e.g. URLs to
  // be used as paths. Returns a pointer to the set `value`.
  base::Value* SetKey(std::string_view key, base::Value value);
  void SetWithoutPathExpansion(std::string_view key, base::Value in_value);

  // Convenience forms of SetWithoutPathExpansion().
  std::unique_ptr<DictionaryValueUpdate> SetDictionaryWithoutPathExpansion(
      std::string_view path,
      base::Value::Dict in_value);

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  // |out_value| is optional and will only be set if non-NULL.
  bool GetBoolean(std::string_view path, bool* out_value) const;
  bool GetInteger(std::string_view path, int* out_value) const;
  // Values of both type Type::INTEGER and Type::DOUBLE can be obtained as
  // doubles.
  bool GetDouble(std::string_view path, double* out_value) const;
  bool GetString(std::string_view path, std::string* out_value) const;
  bool GetDictionary(std::string_view path,
                     const base::Value::Dict** out_value) const;
  bool GetDictionary(std::string_view path,
                     std::unique_ptr<DictionaryValueUpdate>* out_value);

  bool GetDictionaryWithoutPathExpansion(
      std::string_view key,
      std::unique_ptr<DictionaryValueUpdate>* out_value);
  bool GetListWithoutPathExpansion(std::string_view key,
                                   base::Value::List** out_value);

  // Removes the Value with the specified path from this dictionary (or one
  // of its child dictionaries, if the path is more than just a local key).
  // This method returns true if |path| is a valid path; otherwise it will
  // return false and the DictionaryValue object will be unchanged.
  bool Remove(std::string_view path);

  // Like Remove(), but without special treatment of '.'.  This allows e.g. URLs
  // to be used as paths.
  bool RemoveWithoutPathExpansion(std::string_view key, base::Value* out_value);

  base::Value::Dict* AsDict();
  const base::Value::Dict* AsConstDict() const;

 private:
  void RecordPath(std::string_view path);
  void RecordSplitPath(const std::vector<std::string_view>& path);
  void RecordKey(std::string_view key);

  std::vector<std::string_view> SplitPath(std::string_view path);
  std::vector<std::string> ConcatPath(const std::vector<std::string>& base_path,
                                      std::string_view path);
  std::vector<std::string> ConcatPath(
      const std::vector<std::string>& base_path,
      const std::vector<std::string_view>& path);

  UpdateCallback report_update_;
  // `value_` is not a raw_ptr<...> for performance reasons (based on analysis
  // of sampling profiler data).
  RAW_PTR_EXCLUSION base::Value::Dict* const value_;
  const std::vector<std::string> path_;
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_DICTIONARY_VALUE_UPDATE_H_
