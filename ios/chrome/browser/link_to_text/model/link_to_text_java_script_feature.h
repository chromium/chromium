// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/gtest_prod_util.h"
#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_response.h"
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
  // text fragment pointing to this selection. Will attempt on the main frame
  // first, and may subsequently attempt on certain iframes.
  virtual void GetLinkToText(
      web::WebState* state,
      base::OnceCallback<void(LinkToTextResponse*)> callback);

 protected:
  // Protected to allow faking/mocking in tests.
  LinkToTextJavaScriptFeature();
  ~LinkToTextJavaScriptFeature() override;

  // Invokes the JavaScript for link generation. This is a simple wrapper around
  // CallJavaScriptFunction with a few common params included, so it is safe to
  // override in tests where JavaScript execution should be faked without
  // affecting the rest of the logic in this class.
  virtual void RunGenerationJS(
      web::WebFrame* frame,
      base::OnceCallback<void(const base::Value*)> callback);

 private:
  friend class base::NoDestructor<LinkToTextJavaScriptFeature>;
  FRIEND_TEST_ALL_PREFIXES(LinkToTextJavaScriptFeatureTest,
                           ShouldAttemptIframeGeneration);

  void HandleResponse(base::WeakPtr<web::WebState> web_state,
                      base::ElapsedTimer link_generation_timer,
                      base::OnceCallback<void(LinkToTextResponse*)> callback,
                      const base::Value* value);

  void HandleResponseFromSubframe(
      base::OnceCallback<void(LinkToTextResponse*)> final_callback,
      std::vector<LinkToTextResponse*> responses);

  static bool ShouldAttemptIframeGeneration(
      std::optional<shared_highlighting::LinkGenerationError> error,
      const GURL& main_frame_url);

  LinkToTextJavaScriptFeature(const LinkToTextJavaScriptFeature&) = delete;
  LinkToTextJavaScriptFeature& operator=(const LinkToTextJavaScriptFeature&) =
      delete;

  base::WeakPtrFactory<LinkToTextJavaScriptFeature> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_JAVA_SCRIPT_FEATURE_H_
