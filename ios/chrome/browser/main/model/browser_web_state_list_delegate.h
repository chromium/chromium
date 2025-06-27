// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_WEB_STATE_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_WEB_STATE_LIST_DELEGATE_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_delegate.h"
#import "ios/web/public/web_state_observer.h"

class ProfileIOS;

// WebStateList delegate used by Browser implementation.
class BrowserWebStateListDelegate : public WebStateListDelegate,
                                    public web::WebStateObserver {
 public:
  // Policy controlling what to do when a WebState is inserted in the
  // WebStateList.
  enum class InsertionPolicy {
    kDoNothing,
    kAttachTabHelpers,
  };

  // Policy Controlling what to do when a WebState is activated.
  enum class ActivationPolicy {
    kDoNothing,
    kForceRealization,
  };

  // Creates a BrowserWebStateListDelegate with specific policies.
  explicit BrowserWebStateListDelegate(
      ProfileIOS* profile,
      InsertionPolicy insertion_policy = InsertionPolicy::kAttachTabHelpers,
      ActivationPolicy activation_policy = ActivationPolicy::kForceRealization);

  ~BrowserWebStateListDelegate() override;

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override;
  void WillActivateWebState(web::WebState* web_state) override;
  void WillRemoveWebState(web::WebState* web_state) override;

  // web::WebStateObserver implementation.
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Returns the profile used for this instance.
  ProfileIOS* profile() { return profile_.get(); }

 private:
  // Profile that should be used for all WebState in that WebStateList.
  raw_ptr<ProfileIOS> const profile_;

  // Controls what to do when a WebState is inserted.
  const InsertionPolicy insertion_policy_;

  // Controls what to do when a WebState is marked as active.
  const ActivationPolicy activation_policy_;

  // Scoped observation of unrealized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_MAIN_MODEL_BROWSER_WEB_STATE_LIST_DELEGATE_H_
