// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_VALUE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_VALUE_H_
#include <Foundation/Foundation.h>

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "base/notreached.h"
#include "base/values.h"

namespace web {

// A ScriptMessageValue is intended to encapsulate data that is sent from the
// Renderer process via JavaScript and lazily convert the data into a
// base::Value.
class ScriptMessageValue {
 public:
  ScriptMessageValue();
  ScriptMessageValue(ScriptMessageValue&&);
  ScriptMessageValue& operator=(ScriptMessageValue&&);
  // Deleted to prevent accidental copying.
  ScriptMessageValue(const ScriptMessageValue&) = delete;
  ScriptMessageValue& operator=(const ScriptMessageValue&) = delete;
  explicit ScriptMessageValue(base::Value value);
  explicit ScriptMessageValue(std::u16string_view value);
  explicit ScriptMessageValue(double value);
  explicit ScriptMessageValue(bool value);

  ~ScriptMessageValue();
  // Type checker functions.
  base::Value::Type type() const;

  // Access the underlying data structure.
  const base::Value& GetValue();

 private:
  // The object ScriptMessageValue encapsulates.
  std::variant<std::monostate, base::Value> data_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_VALUE_H_
