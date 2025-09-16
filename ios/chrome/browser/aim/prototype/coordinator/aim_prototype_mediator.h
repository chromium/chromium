// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_omnibox_client_delegate.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_consumer.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_mutator.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"

class ComposeboxQueryControllerIOS;
@class AIMPrototypeMediator;
class UrlLoadingBrowserAgent;
class FaviconLoader;
class GURL;
class WebStateList;

// Delegate for the AIM prototype mediator.
@protocol AIMPrototypeMediatorDelegate
- (void)dismissAimPrototype;
@end

// Mediator for the AIM prototype.
@interface AIMPrototypeMediator : NSObject <AIMOmniboxClientDelegate,
                                            AIMPrototypeMutator,
                                            ComposeboxFileUploadObserver,
                                            LoadQueryCommands>

@property(nonatomic, weak) id<AIMPrototypeConsumer> consumer;
@property(nonatomic, weak) id<AIMPrototypeMediatorDelegate> delegate;

- (instancetype)initWithUrlLoadingBrowserAgent:
                    (UrlLoadingBrowserAgent*)urlLoadingBrowserAgent
                     composeboxQueryController:
                         (std::unique_ptr<ComposeboxQueryControllerIOS>)
                             composeboxQueryController
                                  webStateList:(WebStateList*)webStateList
                                 faviconLoader:(FaviconLoader*)faviconLoader;

- (void)disconnect;

// Processes the given `itemProvider` for an image.
- (void)processImageItemProvider:(NSItemProvider*)itemProvider;

// Processes the given `PDFFileURL` for a file.
- (void)processPDFFileURL:(GURL)PDFFileURL;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_MEDIATOR_H_
