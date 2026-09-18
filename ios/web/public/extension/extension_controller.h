// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_EXTENSION_EXTENSION_CONTROLLER_H_
#define IOS_WEB_PUBLIC_EXTENSION_EXTENSION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/functional/callback_forward.h"

namespace web {

// Built-in extensions supported by `ExtensionController`.
enum class BuiltInExtension {
  kGPC,
};

// Interface for managing web extensions.
class API_AVAILABLE(ios(18.4)) ExtensionController {
 public:
  virtual ~ExtensionController() = default;

  // Creates an instance of ExtensionController. Returns nullptr if the platform
  // does not support web extensions.
  static std::unique_ptr<ExtensionController> Create();

  // Loads the specified built-in extension into this controller.
  // Invokes `callback` with true if loading succeeded, false otherwise.
  virtual void LoadBuiltInExtension(
      BuiltInExtension extension,
      base::OnceCallback<void(bool)> callback) = 0;

  // Unloads the specified built-in extension from this controller.
  // Invokes `callback` with true if unloading succeeded, or false if the
  // extension was not loaded or unloading failed.
  virtual void UnloadBuiltInExtension(
      BuiltInExtension extension,
      base::OnceCallback<void(bool)> callback) = 0;

  // Returns whether the specified built-in extension is loaded in this
  // controller.
  virtual bool IsBuiltInExtensionLoaded(BuiltInExtension extension) const = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_EXTENSION_EXTENSION_CONTROLLER_H_
