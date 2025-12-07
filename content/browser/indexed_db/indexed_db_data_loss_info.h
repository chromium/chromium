// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATA_LOSS_INFO_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATA_LOSS_INFO_H_

#include <string>

#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content::indexed_db {

struct IndexedDBDataLossInfo {
  blink::mojom::IDBDataLoss status = blink::mojom::IDBDataLoss::None;
  std::string message;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATA_LOSS_INFO_H_
