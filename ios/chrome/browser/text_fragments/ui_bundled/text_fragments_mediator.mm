// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_fragments/ui_bundled/text_fragments_mediator.h"

#import <memory>

#import "base/feature_list.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/web/public/text_fragments/text_fragments_manager.h"
#import "ios/web/public/web_state.h"

@interface TextFragmentsMediator ()

@property(nonatomic, weak, readonly) id<TextFragmentsDelegate> consumer;

@end

@implementation TextFragmentsMediator

#pragma mark - TextFragmentsDelegate methods

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState {
  if (!base::FeatureList::IsEnabled(
          shared_highlighting::kIOSSharedHighlightingV2)) {
    [self removeTextFragmentsInWebState:webState];
  }
}

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState
                              withSender:(CGRect)rect
                                withText:(NSString*)text
                           withFragments:
                               (std::vector<shared_highlighting::TextFragment>)
                                   fragments {
  if (base::FeatureList::IsEnabled(
          shared_highlighting::kIOSSharedHighlightingV2)) {
    [self.consumer userTappedTextFragmentInWebState:webState
                                         withSender:rect
                                           withText:text
                                      withFragments:std::move(fragments)];
  }
}

#pragma mark - public methods

- (instancetype)initWithConsumer:(id<TextFragmentsDelegate>)consumer {
  if ((self = [super init])) {
    _consumer = consumer;
  }
  return self;
}

- (void)registerWithWebState:(web::WebState*)webState {
  DCHECK(web::TextFragmentsManager::FromWebState(webState));
  // When a new WebState is available, get the manager and attach ourselves. The
  // manager holds a weak reference and has a default behavior if no delegate is
  // available, so there's no need to explicitly detach ourselves on
  // destruction.
  web::TextFragmentsManager::FromWebState(webState)->RegisterDelegate(self);
}

- (void)removeTextFragmentsInWebState:(web::WebState*)webState {
  web::TextFragmentsManager::FromWebState(webState)->RemoveHighlights();
}

@end
