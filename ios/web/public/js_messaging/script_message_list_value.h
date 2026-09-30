// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_LIST_VALUE_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_LIST_VALUE_H_

#include <Foundation/Foundation.h>

#include <optional>

#include "base/types/pass_key.h"

namespace web {
class ScriptMessageValue;

// An iterator that traverses a ScriptMessageListValue.
class ScriptMessageListIterator {
 public:
  using PassKey = base::PassKey<ScriptMessageListValue>;

  using iterator_category = std::forward_iterator_tag;
  using value_type = ScriptMessageValue;
  using difference_type = std::ptrdiff_t;

  ScriptMessageListIterator(PassKey, NSArray* data, size_t index);

  // Overload dereference operator
  ScriptMessageValue operator*() const;

  // Overload increment operators (Prefix and Postfix)
  ScriptMessageListIterator& operator++();
  ScriptMessageListIterator operator++(int);

  friend bool operator==(const ScriptMessageListIterator& a,
                         const ScriptMessageListIterator& b);
  friend bool operator!=(const ScriptMessageListIterator& a,
                         const ScriptMessageListIterator& b);

 private:
  // The array over which the iterator traverses.
  NSArray* data_;
  // The current index the iterator with respect to the NSArray.
  size_t index_;
};

class ScriptMessageListValue {
 public:
  using iterator = ScriptMessageListIterator;

  ScriptMessageListValue(ScriptMessageListValue&&);
  ScriptMessageListValue& operator=(ScriptMessageListValue&&);

  // Deleted to prevent accidental copying.
  ScriptMessageListValue(const ScriptMessageListValue&) = delete;
  ScriptMessageListValue& operator=(const ScriptMessageListValue&) = delete;

  explicit ScriptMessageListValue(NSArray* value);
  ~ScriptMessageListValue();

  // Returns true if there are no elements in the list.
  bool empty() const;

  // Returns the number of elements in the list.
  size_t size() const;

  // Returns the value stored at the beginning of the list or `std::nullopt
  // if the list is empty.
  std::optional<ScriptMessageValue> front() const;

  // Returns the value stored at the end of the list or `std::nullopt
  // if the list is empty.
  std::optional<ScriptMessageValue> back() const;

  // Returns an iterator used to traverse the list.
  iterator begin();

  // Returns an iterator used to traverse the list in reverse starting at the
  // end.
  iterator end();

  // Return the underlying `data_` property.
  // TODO(crbug.com/514993435): Remove once all JavaScriptFeature callsites
  // migrate to using JavaScriptMessage.
  const NSArray* data() const { return data_; }

 private:
  NSArray* data_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_SCRIPT_MESSAGE_LIST_VALUE_H_
