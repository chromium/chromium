// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_EXTENSION_BUILT_IN_WEB_EXTENSION_H_
#define IOS_WEB_EXTENSION_BUILT_IN_WEB_EXTENSION_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "base/functional/callback_forward.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"

@class WKWebExtension;

namespace web {

// Manages loading and caching of a built-in `WKWebExtension` instance.
class API_AVAILABLE(ios(18.4)) BuiltInWebExtension {
 public:
  // Creates a built-in web extension.
  BuiltInWebExtension();

  BuiltInWebExtension(const BuiltInWebExtension&) = delete;
  BuiltInWebExtension& operator=(const BuiltInWebExtension&) = delete;

  virtual ~BuiltInWebExtension();

  // Returns the cached `WKWebExtension` instance if ready, or nil.
  WKWebExtension* GetWKWebExtension() const;

  // Asynchronously creates and returns the `WKWebExtension` instance.
  // Multiple concurrent calls queue their completion handlers.
  void CreateWKWebExtension(
      base::OnceCallback<void(WKWebExtension*)> completion_handler);

  // Returns the URL of the extension resources directory.
  virtual NSURL* GetExtensionURL() const = 0;

 private:
  // Invoked when `extensionWithResourceBaseURL:completionHandler:` finishes.
  void OnExtensionLoaded(WKWebExtension* extension, NSError* error);

  SEQUENCE_CHECKER(sequence_checker_);

  // Cached `WKWebExtension` instance, or nil if not loaded.
  WKWebExtension* extension_ = nil;

  // Whether the extension failed to load.
  bool loading_failed_ = false;

  // Callbacks waiting for the extension to finish loading.
  std::vector<base::OnceCallback<void(WKWebExtension*)>> pending_callbacks_;

  // Weak pointer factory for asynchronous completion callbacks.
  base::WeakPtrFactory<BuiltInWebExtension> weak_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_EXTENSION_BUILT_IN_WEB_EXTENSION_H_
