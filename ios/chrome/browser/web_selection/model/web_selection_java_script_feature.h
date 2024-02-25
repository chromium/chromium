// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#import "base/no_destructor.h"
#import "base/observer_list.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebState;
}  // namespace web

class WebSelectionJavaScriptFeatureObserver;
@class WebSelectionResponse;

/**
 * Handles JS communication to retrieve the page selection.
 */
class WebSelectionJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static WebSelectionJavaScriptFeature* GetInstance();

  WebSelectionJavaScriptFeature(const WebSelectionJavaScriptFeature&) = delete;
  WebSelectionJavaScriptFeature& operator=(
      const WebSelectionJavaScriptFeature&) = delete;

  void AddObserver(WebSelectionJavaScriptFeatureObserver* observer);
  void RemoveObserver(WebSelectionJavaScriptFeatureObserver* observer);

  // Invokes JS-side handlers to grab the current selected text.
  // The selection (if any) will be sent to the `observers_`.
  // If returning `false`, the selection could not be retrieved at
  // all and no observer will be notified.
  virtual bool GetSelectedText(web::WebState* web_state);

  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& script_message) override;
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void Timeout();

 private:
  WebSelectionJavaScriptFeature();
  ~WebSelectionJavaScriptFeature() override;

  friend class base::NoDestructor<WebSelectionJavaScriptFeature>;

  // A list of observers. Weak references.
  base::ObserverList<WebSelectionJavaScriptFeatureObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_JAVA_SCRIPT_FEATURE_H_
