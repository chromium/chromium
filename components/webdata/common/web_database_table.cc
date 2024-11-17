// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database_table.h"

WebDatabaseTable::WebDatabaseTable() : db_(nullptr), meta_table_(nullptr) {}
WebDatabaseTable::~WebDatabaseTable() = default;

void WebDatabaseTable::Init(sql::Database* db,
                            sql::MetaTable* meta_table,
                            const os_crypt_async::Encryptor* encryptor) {
  db_ = db;
  meta_table_ = meta_table;
  encryptor_ = encryptor;
}

void WebDatabaseTable::Shutdown() {
  db_ = nullptr;
  meta_table_ = nullptr;
  encryptor_ = nullptr;
}
