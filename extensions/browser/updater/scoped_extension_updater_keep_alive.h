// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_SCOPED_EXTENSION_UPDATER_KEEP_ALIVE_H_
#define EXTENSIONS_BROWSER_UPDATER_SCOPED_EXTENSION_UPDATER_KEEP_ALIVE_H_

namespace extensions {

// Embedders should subclass ScopedExtensionUpdaterKeepAlive to hold any
// this class to hold any keepalive objects they need.
class ScopedExtensionUpdaterKeepAlive {
 public:
  ScopedExtensionUpdaterKeepAlive() = default;
  virtual ~ScopedExtensionUpdaterKeepAlive() = 0;

  ScopedExtensionUpdaterKeepAlive(
      const ScopedExtensionUpdaterKeepAlive& other) = delete;
  ScopedExtensionUpdaterKeepAlive& operator=(
      const ScopedExtensionUpdaterKeepAlive& other) = delete;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_SCOPED_EXTENSION_UPDATER_KEEP_ALIVE_H_
