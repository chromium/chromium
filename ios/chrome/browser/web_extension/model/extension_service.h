// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "base/callback_list.h"
#import "base/functional/callback_forward.h"
#import "components/keyed_service/core/keyed_service.h"

namespace web {
class ExtensionController;
}  // namespace web

// Pure interface for the profile-keyed service managing web extensions.
class ExtensionService : public KeyedService {
 public:
  // Initializes the service, registers preference observers, and initiates
  // loading of extensions if applicable.
  virtual void Initialize() = 0;

  // Returns the `web::ExtensionController` owned by this service.
  virtual web::ExtensionController* GetExtensionController() const
      API_AVAILABLE(ios(18.4)) = 0;

  // Returns whether the initial extensions have finished loading (or if there
  // are no extensions to load) during startup. Readiness is solely for startup;
  // once the service is ready, it remains ready for the remainder of its
  // lifetime regardless of subsequent preference or extension state changes.
  virtual bool IsReady() const = 0;

  // Returns whether web extensions were loaded at startup.
  virtual bool WebExtensionsWereLoadedAtStartup() const = 0;

  // Registers `callback` to be invoked when the extension service finishes
  // startup initialization. Must only be called if `!IsReady()`.
  virtual base::CallbackListSubscription RunWhenReady(
      base::OnceClosure callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_H_
