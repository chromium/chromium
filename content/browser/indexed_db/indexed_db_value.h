// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT IndexedDBValue {
  // Destructively converts an IndexedDBValue to a Mojo Value.
  static blink::mojom::IDBValuePtr ConvertAndEraseValue(IndexedDBValue* value);

  IndexedDBValue();
  IndexedDBValue(const std::string& input_bits,
                 const std::vector<IndexedDBExternalObject>& external_objects);
  IndexedDBValue(const IndexedDBValue& other);
  ~IndexedDBValue();
  IndexedDBValue& operator=(const IndexedDBValue& other);

  void swap(IndexedDBValue& value) {
    bits.swap(value.bits);
    external_objects.swap(value.external_objects);
  }

  bool empty() const { return bits.empty(); }
  void clear() {
    bits.clear();
    external_objects.clear();
  }

  size_t SizeEstimate() const {
    return bits.size() +
           external_objects.size() * sizeof(IndexedDBExternalObject);
  }

  std::string bits;
  std::vector<IndexedDBExternalObject> external_objects;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_
