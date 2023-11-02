// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_H_
#define IOS_WEB_PUBLIC_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_H_

#import <UIKit/UIKit.h>

#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/web/public/web_state_user_data.h"

// Protocol for clients which handle text fragments-related events.
@protocol TextFragmentsDelegate <NSObject>

// Invoked on user tap anywhere in the page. Default behavior is to remove
// highlights on tap.
- (void)userTappedTextFragmentInWebState:(web::WebState*)webState;

// Invoked on user tap in a particular text fragment. Default behavior is no-op.
- (void)userTappedTextFragmentInWebState:(web::WebState*)webState
                              withSender:(CGRect)rect
                                withText:(NSString*)text
                           withFragments:
                               (std::vector<shared_highlighting::TextFragment>)
                                   fragments;

@end

namespace web {

// Handles highlighting of text fragments on the page and user interactions
// with these highlights.
class TextFragmentsManager : public WebStateUserData<TextFragmentsManager> {
 public:
  ~TextFragmentsManager() override {}
  TextFragmentsManager(const TextFragmentsManager&) = delete;
  TextFragmentsManager& operator=(const TextFragmentsManager&) = delete;

  // Removes any highlights which are currently being displayed on the page.
  virtual void RemoveHighlights() = 0;

  // Allows delegate to register themselves as the handler for certain events.
  // If no delegate is registered, a default behavior will occur for these
  // events, as described in the protocol documentation above.
  virtual void RegisterDelegate(id<TextFragmentsDelegate> delegate) = 0;

  WEB_STATE_USER_DATA_KEY_DECL();

 protected:
  TextFragmentsManager() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEXT_FRAGMENTS_TEXT_FRAGMENTS_MANAGER_H_
