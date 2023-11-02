// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_CONTROLLER_INTERNAL_H_

#import "ios/web_view/public/cwv_translation_controller.h"

#include <memory>
#include <string>

#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"

namespace ios_web_view {
class WebViewTranslateClient;
}  // namespace is_web_view

namespace web {
class WebState;
}  // namespace web

NS_ASSUME_NONNULL_BEGIN

// Some internal methods needed to hide any C++ details.
@interface CWVTranslationController ()

- (instancetype)initWithWebState:(web::WebState*)webState
                 translateClient:
                     (std::unique_ptr<ios_web_view::WebViewTranslateClient>)
                         translateClient NS_DESIGNATED_INITIALIZER;

// Called to keep this class informed of the current translate progress.
// |step| the state of current translation.
// |sourceLanguage| the source language associated with the current |step|.
// |targetLanguage| the target language associated with the current |step|.
// |errorType| the error, if any for the current |step|.
// |triggeredFromMenu| should be true if this was a result from user action.
- (void)updateTranslateStep:(translate::TranslateStep)step
             sourceLanguage:(const std::string&)sourceLanguage
             targetLanguage:(const std::string&)targetLanguage
                  errorType:(translate::TranslateErrors)errorType
          triggeredFromMenu:(bool)triggeredFromMenu;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_CWV_TRANSLATION_CONTROLLER_INTERNAL_H_
