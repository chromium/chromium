// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GRANTED_FILE_ENTRY_H_
#define EXTENSIONS_BROWSER_GRANTED_FILE_ENTRY_H_

#include <string>

namespace extensions {

// Refers to a file entry that a renderer has been given access to.
struct GrantedFileEntry {
  GrantedFileEntry();
  ~GrantedFileEntry();

  std::string id;
  std::string filesystem_id;
  std::string registered_name;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GRANTED_FILE_ENTRY_H_
