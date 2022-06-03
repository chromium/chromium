// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/cookie_blocking_error_logger.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kCommandPrefix[] = "cookie";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ContentBlockType {
  kCookie = 0,
  kLocalStorage = 1,
  kSessionStorage = 2,
  kMaxValue = kSessionStorage,
};

void LogContentBlockFailure(ContentBlockType failure_type) {
  base::UmaHistogramEnumeration("IOS.JavascriptContentBlockFailure",
                                failure_type);
}

}

namespace web {

CookieBlockingErrorLogger::CookieBlockingErrorLogger(WebState* web_state)
    : web_state_impl_(web_state) {
  subscription_ = web_state_impl_->AddScriptCommandCallback(
      base::BindRepeating(
          &CookieBlockingErrorLogger::OnJavascriptMessageReceived,
          base::Unretained(this)),
      kCommandPrefix);
}

CookieBlockingErrorLogger::~CookieBlockingErrorLogger() {}

void CookieBlockingErrorLogger::OnJavascriptMessageReceived(
    const base::Value& message,
    const GURL& page_url,
    bool user_is_interacting,
    WebFrame* sender_frame) {
  const base::Value* broken_overrides = message.FindListKey("brokenOverrides");
  if (!broken_overrides) {
    DLOG(WARNING) << "Broken overrides parameter not found: brokenOverrides";
    return;
  }

  for (const base::Value& broken_override : broken_overrides->GetList()) {
    std::string broken_string = broken_override.GetString();
    if (broken_string == "cookie") {
      LogContentBlockFailure(ContentBlockType::kCookie);
    } else if (broken_string == "localStorage") {
      LogContentBlockFailure(ContentBlockType::kLocalStorage);
    } else if (broken_string == "sessionStorage") {
      LogContentBlockFailure(ContentBlockType::kSessionStorage);
    } else {
      DLOG(ERROR) << "Cookie override error, Unmatched type present in alert: "
                  << broken_string << " URL:" << page_url.spec();
    }
  }

  DCHECK(broken_overrides->GetList().size() > 0)
      << "Javacript overriding has failed. This is likely an iOS/WebKit change";
}

}  // namespace web
