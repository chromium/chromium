// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service_observer_bridge.h"

#import "base/check.h"

ExtensionServiceObserverBridge::ExtensionServiceObserverBridge(
    ExtensionService* service,
    id<ExtensionServiceObserving> observer)
    : observer_(observer) {
  DCHECK(observer_);
  if (service) {
    scoped_observation_.Observe(service);
  }
}

ExtensionServiceObserverBridge::~ExtensionServiceObserverBridge() = default;

void ExtensionServiceObserverBridge::OnExtensionLoadErrorChanged(
    ExtensionService* service,
    bool has_load_error) {
  if ([observer_ respondsToSelector:@selector(extensionService:
                                              didChangeExtensionLoadError:)]) {
    [observer_ extensionService:service
        didChangeExtensionLoadError:has_load_error];
  }
}
