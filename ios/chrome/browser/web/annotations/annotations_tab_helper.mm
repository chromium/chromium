// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/annotations/annotations_tab_helper.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/text_selection/text_classifier_model_service.h"
#import "ios/chrome/browser/text_selection/text_classifier_model_service_factory.h"
#import "ios/public/provider/chrome/browser/context_menu/context_menu_api.h"
#import "ios/web/annotations/annotations_text_manager.h"
#import "ios/web/annotations/annotations_utils.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/browser_state.h"
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

void AnnotationsTabHelper::SetBaseViewController(
    UIViewController* baseViewController) {
  base_view_controller_ = baseViewController;
}

#pragma mark - WebStateObserver methods.

void AnnotationsTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_->RemoveObserver(this);
  auto* manager = web::AnnotationsTextManager::FromWebState(web_state);
  manager->RemoveObserver(this);
  web_state_ = nullptr;
}

#pragma mark - AnnotationsTextObserver methods.

void AnnotationsTabHelper::OnTextExtracted(web::WebState* web_state,
                                           const std::string& text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(web_state_, web_state);

  TextClassifierModelService* service =
      TextClassifierModelServiceFactory::GetForBrowserState(
          ChromeBrowserState::FromBrowserState(web_state->GetBrowserState()));
  base::FilePath model_path =
      service ? service->GetModelPath() : base::FilePath();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ios::provider::ExtractDataElementsFromText, text,
                     ios::provider::GetHandledIntentTypes(web_state),
                     std::move(model_path)),
      base::BindOnce(&AnnotationsTabHelper::ApplyDeferredProcessing,
                     weak_factory_.GetWeakPtr()));
}

void AnnotationsTabHelper::OnDecorated(web::WebState* web_state,
                                       int successes,
                                       int annotations) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (annotations) {
    int percentage = (100 * successes) / annotations;
    base::UmaHistogramPercentage("IOS.Annotations.Percentage", percentage);
  }
}

void AnnotationsTabHelper::OnClick(web::WebState* web_state,
                                   const std::string& text,
                                   CGRect rect,
                                   const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSTextCheckingResult* match =
      web::annotations::DecodeNSTextCheckingResultData(
          base::SysUTF8ToNSString(data));
  if (!match) {
    return;
  }

  NSArray<CRWContextMenuItem*>* items =
      ios::provider::GetContextMenuElementsToAdd(web_state, match,
                                                 base::SysUTF8ToNSString(text),
                                                 base_view_controller_);

  if (items.count) {
    [web_state_->GetWebViewProxy() showMenuWithItems:items rect:rect];
  }
}

#pragma mark - Private Methods

void AnnotationsTabHelper::ApplyDeferredProcessing(
    absl::optional<base::Value> deferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetMainFrame(web_state_) && deferred) {
    auto* manager = web::AnnotationsTextManager::FromWebState(web_state_);
    DCHECK(manager);

    base::Value annotations(std::move(deferred.value()));
    manager->DecorateAnnotations(web_state_, annotations);
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(AnnotationsTabHelper)
