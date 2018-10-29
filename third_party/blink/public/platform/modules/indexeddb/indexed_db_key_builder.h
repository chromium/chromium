// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_INDEXED_DB_KEY_BUILDER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_INDEXED_DB_KEY_BUILDER_H_

#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_key.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class IndexedDBKeyRange;
class WebIDBKeyPath;
class WebIDBKeyRange;

class BLINK_EXPORT IndexedDBKeyBuilder {
 public:
  static IndexedDBKey Build(WebIDBKeyView key);

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBKeyBuilder);
};

class BLINK_EXPORT WebIDBKeyBuilder {
 public:
  static WebIDBKey Build(const IndexedDBKey& key);
  static WebIDBKey Build(const WebIDBKeyView& key);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBKeyBuilder);
};

class BLINK_EXPORT IndexedDBKeyRangeBuilder {
 public:
  static IndexedDBKeyRange Build(const WebIDBKeyRange& key_range);

  // Builds a point range (containing a single key).
  static IndexedDBKeyRange Build(WebIDBKeyView key);

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBKeyRangeBuilder);
};

class BLINK_EXPORT WebIDBKeyRangeBuilder {
 public:
  static WebIDBKeyRange Build(const IndexedDBKeyRange& key);

  // Builds a point range (containing a single key).
  static WebIDBKeyRange Build(WebIDBKeyView key);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBKeyRangeBuilder);
};

class BLINK_EXPORT IndexedDBKeyPathBuilder {
 public:
  static IndexedDBKeyPath Build(const WebIDBKeyPath& key_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBKeyPathBuilder);
};

class BLINK_EXPORT WebIDBKeyPathBuilder {
 public:
  static WebIDBKeyPath Build(const IndexedDBKeyPath& key_path);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBKeyPathBuilder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_INDEXEDDB_INDEXED_DB_KEY_BUILDER_H_
