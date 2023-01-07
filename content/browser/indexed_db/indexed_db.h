// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_H_

namespace content {

namespace indexed_db {

enum CursorType {
  CURSOR_KEY_AND_VALUE = 0,
  CURSOR_KEY_ONLY
};

}  // namespace indexed_db

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_H_
