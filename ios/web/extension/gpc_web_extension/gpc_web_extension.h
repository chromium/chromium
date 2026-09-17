// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_EXTENSION_GPC_WEB_EXTENSION_GPC_WEB_EXTENSION_H_
#define IOS_WEB_EXTENSION_GPC_WEB_EXTENSION_GPC_WEB_EXTENSION_H_

#import <Foundation/Foundation.h>

#import "base/no_destructor.h"
#import "ios/web/extension/built_in_web_extension.h"

namespace web {

// Singleton object managing the bundled Global Privacy Control (GPC)
// `WKWebExtension` instance.
class API_AVAILABLE(ios(18.4)) GPCWebExtension : public BuiltInWebExtension {
 public:
  // Returns the singleton instance.
  static GPCWebExtension* GetInstance();

  GPCWebExtension(const GPCWebExtension&) = delete;
  GPCWebExtension& operator=(const GPCWebExtension&) = delete;

  // BuiltInWebExtension:
  NSURL* GetExtensionURL() const override;

 private:
  friend class base::NoDestructor<GPCWebExtension>;

  // Constructs the GPC built-in web extension.
  GPCWebExtension();
  ~GPCWebExtension() override;
};

}  // namespace web

#endif  // IOS_WEB_EXTENSION_GPC_WEB_EXTENSION_GPC_WEB_EXTENSION_H_
