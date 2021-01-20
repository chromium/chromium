// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_PUBLIC_CPP_DICTIONARY_VALUE_UPDATE_H_
#define SERVICES_PREFERENCES_PUBLIC_CPP_DICTIONARY_VALUE_UPDATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}  // namespace base

namespace prefs {

// A wrapper around base::DictionaryValue that reports changes to its contents
// via a callback.
class DictionaryValueUpdate {
 public:
  using UpdateCallback =
      base::RepeatingCallback<void(const std::vector<std::string>&)>;

  DictionaryValueUpdate(UpdateCallback report_update,
                        base::DictionaryValue* value,
                        std::vector<std::string> path);

  ~DictionaryValueUpdate();
  bool HasKey(base::StringPiece key) const;

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
  // to the path in that location. |in_value| must be non-null.
  void Set(base::StringPiece path, std::unique_ptr<base::Value> in_value);

  // This is similar to |Set|, but lets callers explicitly specify the path
  // components and thus allows nested keys with periods in them.
  void SetPath(std::initializer_list<base::StringPiece> path,
               base::Value value);

  // Convenience forms of Set().  These methods will replace any existing
  // value at that path, even if it has a different type.
  void SetBoolean(base::StringPiece path, bool in_value);
  void SetInteger(base::StringPiece path, int in_value);
  void SetDouble(base::StringPiece path, double in_value);
  void SetString(base::StringPiece path, base::StringPiece in_value);
  void SetString(base::StringPiece path, const base::string16& in_value);
  std::unique_ptr<DictionaryValueUpdate> SetDictionary(
      base::StringPiece path,
      std::unique_ptr<base::DictionaryValue> in_value);

  // Like Set(), but without special treatment of '.'.  This allows e.g. URLs to
  // be used as paths.
  void SetKey(base::StringPiece key, base::Value value);
  void SetWithoutPathExpansion(base::StringPiece key,
                               std::unique_ptr<base::Value> in_value);

  // Convenience forms of SetWithoutPathExpansion().
  std::unique_ptr<DictionaryValueUpdate> SetDictionaryWithoutPathExpansion(
      base::StringPiece path,
      std::unique_ptr<base::DictionaryValue> in_value);

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the path is valid and the value at
  // the end of the path can be returned in the form specified.
  // |out_value| is optional and will only be set if non-NULL.
  bool GetBoolean(base::StringPiece path, bool* out_value) const;
  bool GetInteger(base::StringPiece path, int* out_value) const;
  // Values of both type Type::INTEGER and Type::DOUBLE can be obtained as
  // doubles.
  bool GetDouble(base::StringPiece path, double* out_value) const;
  bool GetString(base::StringPiece path, std::string* out_value) const;
  bool GetString(base::StringPiece path, base::string16* out_value) const;
  bool GetDictionary(base::StringPiece path,
                     const base::DictionaryValue** out_value) const;
  bool GetDictionary(base::StringPiece path,
                     std::unique_ptr<DictionaryValueUpdate>* out_value);
  bool GetList(base::StringPiece path, const base::ListValue** out_value) const;
  bool GetList(base::StringPiece path, base::ListValue** out_value);

  // Like Get(), but without special treatment of '.'.  This allows e.g. URLs to
  // be used as paths.
  bool GetBooleanWithoutPathExpansion(base::StringPiece key,
                                      bool* out_value) const;
  bool GetIntegerWithoutPathExpansion(base::StringPiece key,
                                      int* out_value) const;
  bool GetDoubleWithoutPathExpansion(base::StringPiece key,
                                     double* out_value) const;
  bool GetStringWithoutPathExpansion(base::StringPiece key,
                                     std::string* out_value) const;
  bool GetStringWithoutPathExpansion(base::StringPiece key,
                                     base::string16* out_value) const;
  bool GetDictionaryWithoutPathExpansion(
      base::StringPiece key,
      const base::DictionaryValue** out_value) const;
  bool GetDictionaryWithoutPathExpansion(
      base::StringPiece key,
      std::unique_ptr<DictionaryValueUpdate>* out_value);
  bool GetListWithoutPathExpansion(base::StringPiece key,
                                   const base::ListValue** out_value) const;
  bool GetListWithoutPathExpansion(base::StringPiece key,
                                   base::ListValue** out_value);

  // Removes the Value with the specified path from this dictionary (or one
  // of its child dictionaries, if the path is more than just a local key).
  // If |out_value| is non-NULL, the removed Value will be passed out via
  // |out_value|.  If |out_value| is NULL, the removed value will be deleted.
  // This method returns true if |path| is a valid path; otherwise it will
  // return false and the DictionaryValue object will be unchanged.
  bool Remove(base::StringPiece path, std::unique_ptr<base::Value>* out_value);

  // Like Remove(), but without special treatment of '.'.  This allows e.g. URLs
  // to be used as paths.
  bool RemoveWithoutPathExpansion(base::StringPiece key,
                                  std::unique_ptr<base::Value>* out_value);

  // Removes a path, clearing out all dictionaries on |path| that remain empty
  // after removing the value at |path|.
  bool RemovePath(base::StringPiece path,
                  std::unique_ptr<base::Value>* out_value);

  base::DictionaryValue* AsDictionary();
  const base::DictionaryValue* AsConstDictionary() const;

 private:
  void RecordPath(base::StringPiece path);
  void RecordSplitPath(const std::vector<base::StringPiece>& path);
  void RecordKey(base::StringPiece key);

  std::vector<base::StringPiece> SplitPath(base::StringPiece path);
  std::vector<std::string> ConcatPath(const std::vector<std::string>& base_path,
                                      base::StringPiece path);
  std::vector<std::string> ConcatPath(
      const std::vector<std::string>& base_path,
      const std::vector<base::StringPiece>& path);

  UpdateCallback report_update_;
  base::DictionaryValue* const value_;
  const std::vector<std::string> path_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryValueUpdate);
};

}  // namespace prefs

#endif  // SERVICES_PREFERENCES_PUBLIC_CPP_DICTIONARY_VALUE_UPDATE_H_
