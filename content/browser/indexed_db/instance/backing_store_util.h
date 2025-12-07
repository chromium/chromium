// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_UTIL_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_UTIL_H_

#include "base/values.h"
#include "content/browser/indexed_db/instance/backing_store.h"
#include "content/browser/indexed_db/status.h"
#include "content/common/content_export.h"

namespace content::indexed_db {

// Translates a database into a base::Value. It's intended that this fully
// captures the observable state/contents of the database. Some internal state
// might not be observable via this interface, including
//
// * records that exist in the database, but aren't associated with object
//   stores described in the metadata, e.g. if a deletion left behind records
// * internal bookkeeping, e.g. tombstones for LevelDB or blob references for
//   SQLite
//
// Currently, blob contents are ignored and only blob metadata is factored into
// the output.
//
// NB: the entire DB is loaded into a DictValue which can consume a lot of
// memory! To cut down on total memory requirements, hashing is applied to
// larger keys, values, and object stores. Care should still be taken to limit
// its usage and impact on real users.
CONTENT_EXPORT StatusOr<base::DictValue> SnapshotDatabase(
    BackingStore::Database& db);

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BACKING_STORE_UTIL_H_
