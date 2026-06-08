// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_LIST_VALUE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_LIST_VALUE_H_

#include <Foundation/Foundation.h>

#include <memory>

namespace web {
class ScriptMessageValue;

class ScriptMessageListValue {
 public:
  ScriptMessageListValue(ScriptMessageListValue&&);
  ScriptMessageListValue& operator=(ScriptMessageListValue&&);

  // Deleted to prevent accidental copying.
  ScriptMessageListValue(const ScriptMessageListValue&) = delete;
  ScriptMessageListValue& operator=(const ScriptMessageListValue&) = delete;

  explicit ScriptMessageListValue(NSArray* value);
  ~ScriptMessageListValue();

  // Returns true if there are no elements in the list.
  bool Empty() const;

  // Returns the number of elements in the list.
  size_t Size() const;

  // Returns the value stored at the beginning of the list.
  std::unique_ptr<ScriptMessageValue> Front() const;

  // Returns the value stored at the end of the list.
  std::unique_ptr<ScriptMessageValue> Back() const;

  // TODO(crbug.com/509501985): Add support for iteration.
 private:
  NSArray* data_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_LIST_VALUE_H_
