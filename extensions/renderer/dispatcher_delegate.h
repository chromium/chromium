// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
#define EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_

#include <set>
#include <string>

namespace extensions {

// Base class and default implementation for an extensions::Dispacher delegate.
// DispatcherDelegate can be used to override and extend the behavior of the
// extensions system's renderer side.
class DispatcherDelegate {
 public:
  virtual ~DispatcherDelegate() {}

  // Allows the delegate to respond to an updated set of active extensions in
  // the Dispatcher.
  virtual void OnActiveExtensionsUpdated(
      const std::set<std::string>& extension_ids) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DISPATCHER_DELEGATE_H_
