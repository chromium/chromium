// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"

namespace content {

class CONTENT_EXPORT IndexedDBDatabaseError {
 public:
  IndexedDBDatabaseError();
  explicit IndexedDBDatabaseError(blink::mojom::IDBException code);
  IndexedDBDatabaseError(blink::mojom::IDBException code, const char* message);
  IndexedDBDatabaseError(blink::mojom::IDBException code,
                         const base::string16& message);
  ~IndexedDBDatabaseError();

  IndexedDBDatabaseError& operator=(const IndexedDBDatabaseError& rhs);

  blink::mojom::IDBException code() const { return code_; }
  const base::string16& message() const { return message_; }

 private:
  blink::mojom::IDBException code_ = blink::mojom::IDBException::kNoError;
  base::string16 message_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_ERROR_H_
