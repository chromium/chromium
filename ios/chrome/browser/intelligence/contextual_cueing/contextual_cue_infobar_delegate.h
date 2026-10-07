// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUE_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import <string>

#import "base/memory/raw_ptr.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ui/base/models/image_model.h"

@protocol GeminiCommands;

namespace web {
class WebState;
}

namespace contextual_cueing {

// Configuration data for presenting a contextual cue infobar banner.
struct ContextualCueInfobarConfig {
  // Title text displayed in the infobar banner.
  std::u16string title;
  // Label for the action button.
  std::u16string button_text;
  // Prepopulated prompt sent to Gemini on action button click.
  std::string prompt;
};

// An infobar delegate that displays a contextual cue banner recommending Gemini
// actions based on page classification and server model execution.
class ContextualCueInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates and adds a contextual cue infobar to `web_state`. Returns true if
  // the infobar was successfully added to the WebState's InfoBarManager.
  // `gemini_handler` is optional; if nil, it will be resolved from the
  // WebState's Browser when the action button is tapped.
  static bool Create(web::WebState* web_state,
                     id<GeminiCommands> gemini_handler = nil);

  // Removes any active contextual cue infobar from `web_state`.
  static void Remove(web::WebState* web_state);

  ContextualCueInfobarDelegate(web::WebState* web_state,
                               id<GeminiCommands> gemini_handler,
                               ContextualCueInfobarConfig config);
  ~ContextualCueInfobarDelegate() override;

  ContextualCueInfobarDelegate(const ContextualCueInfobarDelegate&) = delete;
  ContextualCueInfobarDelegate& operator=(const ContextualCueInfobarDelegate&) =
      delete;

  // ConfirmInfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  ui::ImageModel GetIcon() const override;
  bool Accept() override;
  void InfoBarDismissed() override;
  bool ShouldExpire(const NavigationDetails& details) const override;

 private:
  raw_ptr<web::WebState> web_state_ = nullptr;
  id<GeminiCommands> gemini_handler_ = nil;
  ContextualCueInfobarConfig config_;
  bool has_user_interacted_ = false;
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUE_INFOBAR_DELEGATE_H_
