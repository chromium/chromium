// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_DICT_VALUE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_DICT_VALUE_H_

#include <Foundation/Foundation.h>

#include <memory>
#include <optional>
#include <string_view>

#include "base/values.h"

namespace web {
class ScriptMessageValue;
class ScriptMessageListValue;

class ScriptMessageDictValue {
 public:
  ScriptMessageDictValue(ScriptMessageDictValue&&);
  ScriptMessageDictValue& operator=(ScriptMessageDictValue&&);

  // Deleted to prevent accidental copying.
  ScriptMessageDictValue(const ScriptMessageDictValue&) = delete;
  ScriptMessageDictValue& operator=(const ScriptMessageDictValue&) = delete;

  explicit ScriptMessageDictValue(NSDictionary* value);
  ~ScriptMessageDictValue();

  // Returns true if there are no values in this dict and false otherwise.
  bool empty() const;
  // Returns the number of values in this dict.
  size_t size() const;

  // Returns true if `key` is an entry in this dictionary.
  bool contains(std::string_view key) const;

  // Finds the entry corresponding to `key` in this dictionary. Returns
  // nullptr if there is no such entry.
  std::unique_ptr<ScriptMessageValue> Find(std::string_view key);

  // Similar to `Find()` above, but returns `std::nullopt`/`nullptr` if the type
  // of the entry does not match.
  std::optional<bool> FindBool(std::string_view key) const;
  std::optional<int> FindInt(std::string_view key) const;
  // Returns a non-null value for both `Value::Type::DOUBLE` and
  // `Value::Type::INT`, converting the latter to a double.
  std::optional<double> FindDouble(std::string_view key) const;
  std::optional<std::string> FindString(std::string_view key) const;
  std::unique_ptr<ScriptMessageDictValue> FindDict(std::string_view key) const;
  std::unique_ptr<ScriptMessageListValue> FindList(std::string_view key) const;

 private:
  NSDictionary* data_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_DICT_VALUE_H_
