// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_JAVA_SCRIPT_FEATURE_H_

#import "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/values.h"
#import "ios/chrome/browser/link_to_text/link_to_text_response.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace web {
class WebState;
}  // namespace web

/**
 * Handles JS communication for the Link-to-Text feature.
 */
class LinkToTextJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static LinkToTextJavaScriptFeature* GetInstance();

  // Invokes JS-side handlers to grab the current selected text, and generate a
  // text fragment pointing to this selection.
  virtual void GetLinkToText(
      web::WebState* state,
      web::WebFrame* frame,
      base::OnceCallback<void(LinkToTextResponse*)> callback);

 protected:
  // Protected to allow faking/mocking in tests.
  LinkToTextJavaScriptFeature();
  ~LinkToTextJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<LinkToTextJavaScriptFeature>;

  void HandleResponse(web::WebState* state,
                      base::ElapsedTimer link_generation_timer,
                      base::OnceCallback<void(LinkToTextResponse*)> callback,
                      const base::Value* value);

  LinkToTextJavaScriptFeature(const LinkToTextJavaScriptFeature&) = delete;
  LinkToTextJavaScriptFeature& operator=(const LinkToTextJavaScriptFeature&) =
      delete;

  base::WeakPtrFactory<LinkToTextJavaScriptFeature> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_JAVA_SCRIPT_FEATURE_H_
