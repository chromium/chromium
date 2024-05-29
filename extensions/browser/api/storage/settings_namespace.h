// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_NAMESPACE_H_
#define EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_NAMESPACE_H_

#include <string>

namespace extensions {

namespace settings_namespace {

// The namespaces of the storage areas that have ValueStore.
enum Namespace {
  LOCAL,    // "local"    i.e. chrome.storage.local
  SYNC,     // "sync"     i.e. chrome.storage.sync
  MANAGED,  // "managed"  i.e. chrome.storage.managed
  INVALID
};

// Converts a namespace to its string representation.
// Namespace must not be INVALID.
std::string ToString(Namespace settings_namespace);

}  // namespace settings_namespace

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_STORAGE_SETTINGS_NAMESPACE_H_
