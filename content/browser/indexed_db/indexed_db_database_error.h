// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_

#include <stdint.h>

#include <string>

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-shared.h"

namespace content {

class CONTENT_EXPORT IndexedDBDatabaseError {
 public:
  IndexedDBDatabaseError();
  explicit IndexedDBDatabaseError(blink::mojom::IDBException code);
  IndexedDBDatabaseError(blink::mojom::IDBException code,
                         const std::string& message);
  IndexedDBDatabaseError(blink::mojom::IDBException code,
                         const std::u16string& message);
  ~IndexedDBDatabaseError();

  IndexedDBDatabaseError& operator=(const IndexedDBDatabaseError& rhs);

  blink::mojom::IDBException code() const { return code_; }
  const std::u16string& message() const { return message_; }

 private:
  blink::mojom::IDBException code_ = blink::mojom::IDBException::kNoError;
  std::u16string message_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_
