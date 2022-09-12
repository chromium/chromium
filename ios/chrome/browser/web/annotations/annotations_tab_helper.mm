// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/annotations/annotations_tab_helper.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/annotations/annotations_text_manager.h"
#import "ios/web/annotations/annotations_utils.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/ui/crw_context_menu_item.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

static NSString* kDecorationDate = @"DATE";
static NSString* kDecorationAddress = @"ADDRESS";
static NSString* kDecorationPhoneNumber = @"PHONE_NUMBER";

namespace {

NSString* TypeForNSTextCheckingResultData(NSTextCheckingResult* match) {
  if (match.resultType == NSTextCheckingTypeDate) {
    return kDecorationDate;
  } else if (match.resultType == NSTextCheckingTypeAddress) {
    return kDecorationAddress;
  } else if (match.resultType == NSTextCheckingTypePhoneNumber) {
    return kDecorationPhoneNumber;
  }
  return nullptr;
}

}  //  namespace

AnnotationsTabHelper::AnnotationsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  web_state_->AddObserver(this);
  // In some cases, AnnotationsTextManager is created before this and in some
  // others after. Make sure it exists.
  web::AnnotationsTextManager::CreateForWebState(web_state);
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state);
  manager->AddObserver(this);
}

AnnotationsTabHelper::~AnnotationsTabHelper() {
  web_state_ = nullptr;
}

// static
void AnnotationsTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           std::make_unique<AnnotationsTabHelper>(web_state));
  }
}

void AnnotationsTabHelper::SetBaseViewController(
    UIViewController* baseViewController) {
  base_view_controller_ = baseViewController;
}

#pragma mark - WebStateObserver methods.

// TODO(crbug.com/1350974): Is this needed: when OnTextExtracted is called,
// the main frame is available, since text has already been extracted?
void AnnotationsTabHelper::WebFrameDidBecomeAvailable(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  if (web_frame->IsMainFrame() && deferred_processing_params_) {
    DecorateAnnotations();
  }
}

void AnnotationsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_->RemoveObserver(this);
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state);
  manager->RemoveObserver(this);
  web_state_ = nullptr;
  deferred_processing_params_ = {};
}

#pragma mark - AnnotationsTextObserver methods.

void AnnotationsTabHelper::OnTextExtracted(web::WebState* web_state,
                                           const std::string& text) {
  __block std::string block_text = text;
  __block base::WeakPtr<AnnotationsTabHelper> weak_this =
      weak_factory_.GetWeakPtr();
  // TODO(crbug.com/1350974): run in bg thread?
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!weak_this.get())
      return;
    weak_this->deferred_processing_params_ =
        weak_this->ApplyDataExtractor(block_text);
    if (GetMainFrame(weak_this->web_state_) &&
        weak_this->deferred_processing_params_) {
      dispatch_async(dispatch_get_main_queue(), ^{
        if (weak_this.get() && weak_this->deferred_processing_params_) {
          weak_this->DecorateAnnotations();
        }
      });
    }
  });
}

void AnnotationsTabHelper::OnDecorated(web::WebState* web_state,
                                       int successes,
                                       int annotations) {
  // TODO(crbug.com/1350974): Add metrics
}

void AnnotationsTabHelper::OnClick(web::WebState* web_state,
                                   const std::string& text,
                                   CGRect rect,
                                   const std::string& data) {
  NSTextCheckingResult* match =
      web::annotations::DecodeNSTextCheckingResultData(
          base::SysUTF8ToNSString(data));
  if (!match) {
    return;
  }

  NSMutableArray* items = [[NSMutableArray alloc] init];
  if (match.resultType == NSTextCheckingTypeDate) {
    [items addObject:[CRWContextMenuItem
                         itemWithID:@"addToGoogleCalendar"
                              title:@"Add to Google Calendar"
                             action:^{
                                 // TODO(crbug.com/1350974): execute
                             }]];
  } else if (match.resultType == NSTextCheckingTypeAddress) {
    [items addObject:[CRWContextMenuItem
                         itemWithID:@"showMiniMap"
                              title:@"Show Mini Map"
                             action:^{
                                 // TODO(crbug.com/1350974): execute
                             }]];
  } else if (match.resultType == NSTextCheckingTypePhoneNumber) {
    [items addObject:[CRWContextMenuItem
                         itemWithID:@"callPhoneNumber"
                              title:@"Call Phone Number"
                             action:^{
                                 // TODO(crbug.com/1350974): execute
                             }]];
  }

  [items
      addObject:[CRWContextMenuItem itemWithID:@"copyDate"
                                         title:@"Copy"
                                        action:^{
                                            // TODO(crbug.com/1350974): execute
                                        }]];

  [web_state_->GetWebViewProxy() showMenuWithItems:items rect:rect];
}

#pragma mark - Private Methods

absl::optional<base::Value> AnnotationsTabHelper::ApplyDataExtractor(
    const std::string& text) {
  // TODO(crbug.com/1350974): move to provider
  if (text.empty()) {
    return {};
  }

  NSString* source = base::SysUTF8ToNSString(text);
  NSError* error = nil;
  NSDataDetector* detector = [NSDataDetector
      dataDetectorWithTypes:NSTextCheckingTypeDate | NSTextCheckingTypeAddress |
                            NSTextCheckingTypePhoneNumber
                      error:&error];
  if (error) {
    return {};
  }

  __block base::Value::List parsed;
  auto matchHandler = ^(NSTextCheckingResult* match, NSMatchingFlags flags,
                        BOOL* stop) {
    NSString* type = TypeForNSTextCheckingResultData(match);
    NSString* data = web::annotations::EncodeNSTextCheckingResultData(match);
    if (data && type) {
      parsed.Append(web::annotations::ConvertMatchToAnnotation(
          source, match.range, data, type));
    }
  };

  NSRange range = NSMakeRange(0, source.length);
  [detector enumerateMatchesInString:source
                             options:NSMatchingWithTransparentBounds
                               range:range
                          usingBlock:matchHandler];

  if (parsed.empty()) {
    return {};
  }

  return absl::optional<base::Value>(
      static_cast<base::Value>(std::move(parsed)));
}

void AnnotationsTabHelper::DecorateAnnotations() {
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state_);
  DCHECK(manager);

  base::Value annotations(std::move(deferred_processing_params_.value()));
  manager->DecorateAnnotations(web_state_, annotations);
  deferred_processing_params_ = {};
}

WEB_STATE_USER_DATA_KEY_IMPL(AnnotationsTabHelper)
