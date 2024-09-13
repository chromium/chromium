// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/indexed_db/indexed_db_return_value.h"

#include <stdint.h>
#include <vector>

#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

// static
blink::mojom::IDBReturnValuePtr IndexedDBReturnValue::ConvertReturnValue(
    IndexedDBReturnValue* value) {
  auto mojo_value = blink::mojom::IDBReturnValue::New();
  mojo_value->value = blink::mojom::IDBValue::New();
  if (value->primary_key.IsValid()) {
    mojo_value->primary_key = std::move(value->primary_key);
    mojo_value->key_path = std::move(value->key_path);
  }
  if (!value->empty()) {
    mojo_value->value->bits = std::move(value->bits);
  }
  IndexedDBExternalObject::ConvertToMojo(value->external_objects,
                                         &mojo_value->value->external_objects);
  return mojo_value;
}

}  // namespace content::indexed_db
