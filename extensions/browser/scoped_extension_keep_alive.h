// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCOPED_EXTENSION_KEEP_ALIVE_H_
#define EXTENSIONS_BROWSER_SCOPED_EXTENSION_KEEP_ALIVE_H_

namespace extensions {

// Embedders should subclass ScopedBrowserContextKeepAlive to hold any this
// class to hold any keepalive objects they need.
class ScopedBrowserContextKeepAlive {
 public:
  ScopedBrowserContextKeepAlive() = default;
  virtual ~ScopedBrowserContextKeepAlive() = 0;

  ScopedBrowserContextKeepAlive(const ScopedBrowserContextKeepAlive& other) =
      delete;
  ScopedBrowserContextKeepAlive& operator=(
      const ScopedBrowserContextKeepAlive& other) = delete;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCOPED_EXTENSION_KEEP_ALIVE_H_
