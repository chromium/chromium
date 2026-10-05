// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"

// Objective-C protocol mirroring `ExtensionService::Observer`.
@protocol ExtensionServiceObserving <NSObject>
@optional
// Called when the extension loading error state changes.
- (void)extensionService:(ExtensionService*)service
    didChangeExtensionLoadError:(bool)hasLoadError;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class ExtensionServiceObserverBridge : public ExtensionService::Observer {
 public:
  ExtensionServiceObserverBridge(ExtensionService* service,
                                 id<ExtensionServiceObserving> observer);
  ~ExtensionServiceObserverBridge() override;

  // ExtensionService::Observer implementation.
  void OnExtensionLoadErrorChanged(ExtensionService* service,
                                   bool has_load_error) override;

 private:
  __weak id<ExtensionServiceObserving> observer_ = nil;
  base::ScopedObservation<ExtensionService, ExtensionService::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_EXTENSION_MODEL_EXTENSION_SERVICE_OBSERVER_BRIDGE_H_
