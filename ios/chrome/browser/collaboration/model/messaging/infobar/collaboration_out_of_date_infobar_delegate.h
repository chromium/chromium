// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INFOBAR_COLLABORATION_OUT_OF_DATE_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INFOBAR_COLLABORATION_OUT_OF_DATE_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "components/infobars/core/confirm_infobar_delegate.h"

class ProfileIOS;
@protocol ApplicationCommands;

// Shows an out-of-date message related to shared tab groups support in an
// infobar.
class CollaborationOutOfDateInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit CollaborationOutOfDateInfoBarDelegate(
      id<ApplicationCommands> application_commands_handler);

  CollaborationOutOfDateInfoBarDelegate(
      const CollaborationOutOfDateInfoBarDelegate&) = delete;
  CollaborationOutOfDateInfoBarDelegate& operator=(
      const CollaborationOutOfDateInfoBarDelegate&) = delete;

  ~CollaborationOutOfDateInfoBarDelegate() override;

  // Creates an out-of-date message infobar.
  // Returns true if the infobar creation was successful, false otherwise.
  static bool Create(ProfileIOS* profile);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  ui::ImageModel GetIcon() const override;

 private:
  id<ApplicationCommands> application_commands_handler_;
};

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INFOBAR_COLLABORATION_OUT_OF_DATE_INFOBAR_DELEGATE_H_
