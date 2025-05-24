// Copyright 2014 The Chromium Authors
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

namespace content::indexed_db {

struct CONTENT_EXPORT IndexedDBValue {
  // Destructively converts an IndexedDBValue to a Mojo Value.
  static blink::mojom::IDBValuePtr ConvertAndEraseValue(IndexedDBValue* value);

  IndexedDBValue();
  ~IndexedDBValue();

  // Move is allowed.
  IndexedDBValue(IndexedDBValue&& other);
  IndexedDBValue& operator=(IndexedDBValue&& other);

  // Copy is a footgun.
  IndexedDBValue(const IndexedDBValue& other) = delete;
  IndexedDBValue& operator=(const IndexedDBValue& other) = delete;

  // In rare cases, copy is acceptable.
  IndexedDBValue Clone() const;

  // Only used for tests.
  IndexedDBValue(const std::string& input_bits,
                 const std::vector<IndexedDBExternalObject>& external_objects);

  bool empty() const { return bits.empty(); }
  void clear() {
    bits.clear();
    external_objects.clear();
  }

  size_t SizeEstimate() const {
    return bits.size() +
           external_objects.size() * sizeof(IndexedDBExternalObject);
  }

  std::vector<uint8_t> bits;
  std::vector<IndexedDBExternalObject> external_objects;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_
