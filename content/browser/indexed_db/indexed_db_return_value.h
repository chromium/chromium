// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_RETURN_VALUE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_RETURN_VALUE_H_

#include "content/browser/indexed_db/indexed_db_value.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

namespace content::indexed_db {

// Values returned to the IDB client may contain a primary key value generated
// by IDB. This is optional and only done when using a key generator. This key
// value cannot (at least easily) be amended to the object being written to the
// database, so they are kept separately, and sent back with the original data
// so that the render process can amend the returned object.
struct IndexedDBReturnValue : public IndexedDBValue {
  // Destructively converts an IndexedDBReturnValue to a Mojo ReturnValue.
  static blink::mojom::IDBReturnValuePtr ConvertReturnValue(
      IndexedDBReturnValue* value);

  blink::IndexedDBKey
      primary_key;  // primary key (only when using key generator)
  blink::IndexedDBKeyPath key_path;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_RETURN_VALUE_H_
