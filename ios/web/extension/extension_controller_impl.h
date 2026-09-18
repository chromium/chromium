// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_EXTENSION_EXTENSION_CONTROLLER_IMPL_H_
#define IOS_WEB_EXTENSION_EXTENSION_CONTROLLER_IMPL_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "ios/web/public/extension/extension_controller.h"

@class WKWebExtension;
@class WKWebExtensionController;

namespace web {

// Implementation of ExtensionController that manages a WKWebExtensionController
// (available on iOS 18.4+).
class API_AVAILABLE(ios(18.4)) ExtensionControllerImpl
    : public ExtensionController {
 public:
  ExtensionControllerImpl();
  ~ExtensionControllerImpl() override;

  ExtensionControllerImpl(const ExtensionControllerImpl&) = delete;
  ExtensionControllerImpl& operator=(const ExtensionControllerImpl&) = delete;

  // ExtensionController:
  void LoadBuiltInExtension(BuiltInExtension extension,
                            base::OnceCallback<void(bool)> callback) override;
  void UnloadBuiltInExtension(BuiltInExtension extension,
                              base::OnceCallback<void(bool)> callback) override;
  bool IsBuiltInExtensionLoaded(BuiltInExtension extension) const override;

  // Returns the underlying WKWebExtensionController if available.
  WKWebExtensionController* GetWKWebExtensionController() const;

 private:
  // Called when the WKWebExtension has been created.
  void OnBuiltInWebExtensionCreated(BuiltInExtension extension,
                                    base::OnceCallback<void(bool)> callback,
                                    WKWebExtension* wk_extension);

  SEQUENCE_CHECKER(sequence_checker_);

  WKWebExtensionController* extension_controller_ = nil;

  base::WeakPtrFactory<ExtensionControllerImpl> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_EXTENSION_EXTENSION_CONTROLLER_IMPL_H_
