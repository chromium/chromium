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
#include "mojo/public/cpp/base/big_buffer.h"

namespace content::indexed_db {

struct CONTENT_EXPORT IndexedDBValue {
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

  bool empty() const { return bits.size() == 0U; }
  void clear() {
    bits = mojo_base::BigBuffer();
    external_objects.clear();
  }

  size_t SizeEstimate() const {
    return bits.size() +
           external_objects.size() * sizeof(IndexedDBExternalObject);
  }

  // `bits` is the serialized script value, the meaning of which is opaque to
  // IndexedDB code in the browser process.
  // NB: in cases where `this` comes from the renderer process, any
  // *processing* of `bits` is subject to TOCTOU bugs as described in
  // big_buffer.h.
  mojo_base::BigBuffer bits;
  std::vector<IndexedDBExternalObject> external_objects;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_VALUE_H_
