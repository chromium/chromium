// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_text_fragments_handler.h"

#import "base/json/json_writer.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/text_fragments_utils.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/web_state/web_state_impl.h"

#import "ios/web/web_state/ui/crw_web_view_handler_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWTextFragmentsHandler ()

@property(nonatomic, weak) id<CRWWebViewHandlerDelegate> delegate;

// Returns the WebStateImpl from self.delegate.
@property(nonatomic, readonly, assign) web::WebStateImpl* webStateImpl;

@end

@implementation CRWTextFragmentsHandler

- (instancetype)initWithDelegate:(id<CRWWebViewHandlerDelegate>)delegate {
  if (self = [super init]) {
    _delegate = delegate;
  }

  return self;
}

- (void)processTextFragmentsWithContext:(web::NavigationContext*)context
                               referrer:(web::Referrer)referrer {
  if (!context || ![self areTextFragmentsAllowedInContext:context]) {
    return;
  }

  base::Value parsedFragments =
      web::ParseTextFragments(self.webStateImpl->GetLastCommittedURL());

  if (parsedFragments.type() == base::Value::Type::NONE)
    return;

  std::string fragmentParam;
  base::JSONWriter::Write(parsedFragments, &fragmentParam);

  std::string script = base::ReplaceStringPlaceholders(
      "__gCrWeb.textFragments.handleTextFragments($1, $2)",
      {fragmentParam, /* scroll = */ "true"},
      /* offsets= */ nil);

  self.webStateImpl->ExecuteJavaScript(base::UTF8ToUTF16(script));
}

#pragma mark - Private Methods

// Returns NO if fragments highlighting is not allowed in the current |context|.
- (BOOL)areTextFragmentsAllowedInContext:(web::NavigationContext*)context {
  if (!base::FeatureList::IsEnabled(web::features::kScrollToTextIOS))
    return NO;

  if (self.isBeingDestroyed) {
    return NO;
  }

  // If the current instance isn't being destroyed, then it must be able to get
  // a valid WebState.
  DCHECK(self.webStateImpl);

  if (self.webStateImpl->HasOpener()) {
    // TODO(crbug.com/1099268): Loosen this restriction if the opener has the
    // same domain.
    return NO;
  }

  return context->HasUserGesture() && !context->IsSameDocument();
}

- (web::WebStateImpl*)webStateImpl {
  return [self.delegate webStateImplForWebViewHandler:self];
}

@end
