// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database_error.h"

#include "base/strings/utf_string_conversions.h"

namespace content::indexed_db {

DatabaseError::DatabaseError() = default;

DatabaseError::DatabaseError(blink::mojom::IDBException code) : code_(code) {}

DatabaseError::DatabaseError(blink::mojom::IDBException code,
                             const std::string& message)
    : code_(code), message_(base::UTF8ToUTF16(message)) {}

DatabaseError::DatabaseError(blink::mojom::IDBException code,
                             const std::u16string& message)
    : code_(code), message_(message) {}

DatabaseError::~DatabaseError() = default;

DatabaseError& DatabaseError::operator=(const DatabaseError& rhs) = default;

}  // namespace content::indexed_db
