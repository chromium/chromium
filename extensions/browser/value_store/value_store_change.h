// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_CHANGE_H_
#define EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_CHANGE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/values.h"

class ValueStoreChange;
typedef std::vector<ValueStoreChange> ValueStoreChangeList;

// A change to a setting.  Safe/efficient to copy.
class ValueStoreChange {
 public:
  // Converts an ValueStoreChangeList into JSON of the form:
  // { "foo": { "key": "foo", "oldValue": "bar", "newValue": "baz" } }
  static std::string ToJson(const ValueStoreChangeList& changes);

  ValueStoreChange(const std::string& key,
                   base::Optional<base::Value> old_value,
                   base::Optional<base::Value> new_value);

  ValueStoreChange(const ValueStoreChange& other);

  ~ValueStoreChange();

  // Gets the key of the setting which changed.
  const std::string& key() const;

  // Gets the value of the setting before the change, or NULL if there was no
  // old value.
  const base::Value* old_value() const;

  // Gets the value of the setting after the change, or NULL if there is no new
  // value.
  const base::Value* new_value() const;

 private:
  class Inner : public base::RefCountedThreadSafe<Inner> {
   public:
    Inner(const std::string& key,
          base::Optional<base::Value> old_value,
          base::Optional<base::Value> new_value);

    const std::string key_;
    const base::Optional<base::Value> old_value_;
    const base::Optional<base::Value> new_value_;

   private:
    friend class base::RefCountedThreadSafe<Inner>;
    virtual ~Inner();
  };

  scoped_refptr<Inner> inner_;
};

#endif  // EXTENSIONS_BROWSER_VALUE_STORE_VALUE_STORE_CHANGE_H_
