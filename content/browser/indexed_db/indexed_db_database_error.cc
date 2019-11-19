// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database_error.h"

#include "base/strings/utf_string_conversions.h"

namespace content {

IndexedDBDatabaseError::IndexedDBDatabaseError() = default;

IndexedDBDatabaseError::IndexedDBDatabaseError(blink::mojom::IDBException code)
    : code_(code) {}

IndexedDBDatabaseError::IndexedDBDatabaseError(blink::mojom::IDBException code,
                                               const char* message)
    : code_(code), message_(base::ASCIIToUTF16(message)) {}

IndexedDBDatabaseError::IndexedDBDatabaseError(blink::mojom::IDBException code,
                                               const base::string16& message)
    : code_(code), message_(message) {}

IndexedDBDatabaseError::~IndexedDBDatabaseError() = default;

IndexedDBDatabaseError& IndexedDBDatabaseError::operator=(
    const IndexedDBDatabaseError& rhs) = default;

}  // namespace content
