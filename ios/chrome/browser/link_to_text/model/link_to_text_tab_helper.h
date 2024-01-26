// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_TAB_HELPER_H_

#include "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/timer/elapsed_timer.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_java_script_feature.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewProxy;

// A tab helper that observes WebState and triggers the link to text generation
// Javascript library on it.
class LinkToTextTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<LinkToTextTabHelper> {
 public:
  ~LinkToTextTabHelper() override;

  // Returns whether the link to text feature should be offered for the current
  // user selection.
  bool ShouldOffer();

  // Calls the JavaScript to generate a URL linking to the current
  // selected text. If successful, will invoke `callback` with the returned
  // generated payload and nil error. If unsuccessful, will invoke `callback`
  // with a nil payload and defined error.
  void GetLinkToText(base::OnceCallback<void(LinkToTextResponse*)> callback);

  // Allows replacing the JavaScriptFeature with a mocked or faked version in
  // tests.
  void SetJSFeatureForTesting(LinkToTextJavaScriptFeature* js_feature);

 private:
  friend class web::WebStateUserData<LinkToTextTabHelper>;

  explicit LinkToTextTabHelper(web::WebState* web_state);

  // Invoked with pending GetLinkToText `callback` and the `response` from
  // the JavaScript call to generate a link to selected text.
  void OnJavaScriptResponseReceived(
      base::OnceCallback<void(LinkToTextResponse*)> callback,
      const base::Value* response);

  // Identifies if a string has any characters that aren't a boundary
  // (i.e., whitespace or punctuation) character.
  bool IsOnlyBoundaryChars(NSString* str);
  FRIEND_TEST_ALL_PREFIXES(LinkToTextTabHelperTest, IsOnlyBoundaryChars);

  // Returns the object to be used for JavaScript interactions -- either the
  // real singleton for this class, or the object passed to
  // `SetJSFeatureForTesting`, if one has been provided.
  LinkToTextJavaScriptFeature* GetJSFeature();

  // Not copyable or moveable.
  LinkToTextTabHelper(const LinkToTextTabHelper&) = delete;
  LinkToTextTabHelper& operator=(const LinkToTextTabHelper&) = delete;

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Regex for `IsOnlyBoundaryChars`. Lazily-initialized to avoid recompiling
  // each time we check.
  NSRegularExpression* not_boundary_char_regex_ = nil;

  raw_ptr<LinkToTextJavaScriptFeature> js_feature_for_testing_ = nullptr;

  base::WeakPtrFactory<LinkToTextTabHelper> weak_ptr_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_TAB_HELPER_H_
