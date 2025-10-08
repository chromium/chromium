// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COMPOSEBOX_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COMPOSEBOX_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "components/omnibox/composebox/ios/composebox_file_upload_observer_bridge.h"
#import "ios/chrome/browser/aim/prototype/coordinator/aim_omnibox_client_delegate.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_consumer.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_composebox_mutator.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"

@protocol AIMPrototypeURLLoader;
class ComposeboxQueryControllerIOS;
class FaviconLoader;
class GURL;
class WebStateList;

// Mediator for the AIM prototype composebox.
@interface AIMPrototypeComposeboxMediator
    : NSObject <AIMOmniboxClientDelegate,
                AIMPrototypeComposeboxMutator,
                ComposeboxFileUploadObserver,
                LoadQueryCommands>

@property(nonatomic, weak) id<AIMPrototypeComposeboxConsumer> consumer;
@property(nonatomic, weak) id<AIMPrototypeURLLoader> URLLoader;

- (instancetype)initWithComposeboxQueryController:
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

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COMPOSEBOX_MEDIATOR_H_
