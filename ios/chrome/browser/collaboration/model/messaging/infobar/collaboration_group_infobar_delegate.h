// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INFOBAR_COLLABORATION_GROUP_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INFOBAR_COLLABORATION_GROUP_INFOBAR_DELEGATE_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "components/collaboration/public/messaging/message.h"
#import "components/infobars/core/confirm_infobar_delegate.h"

class ProfileIOS;
@protocol ShareKitAvatarPrimitive;

// Shows a collaboration group update in an infobar.
class CollaborationGroupInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  CollaborationGroupInfoBarDelegate(
      ProfileIOS* profile,
      collaboration::messaging::InstantMessage instant_message);

  CollaborationGroupInfoBarDelegate(const CollaborationGroupInfoBarDelegate&) =
      delete;
  CollaborationGroupInfoBarDelegate& operator=(
      const CollaborationGroupInfoBarDelegate&) = delete;

  ~CollaborationGroupInfoBarDelegate() override;

  // Creates a collaboration group infobar for the given `instant_message`.
  // Returns true if the infobar creation was successful, false otherwise.
  static bool Create(ProfileIOS* profile,
                     collaboration::messaging::InstantMessage instant_message);

  // InfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  std::u16string GetTitleText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;

  // Returns an avatar primitive if there is only one affected user in
  // `instant_message_`, otherwise return nil.
  id<ShareKitAvatarPrimitive> GetAvatarPrimitive();

  // Returns a symbol image based on the current `instant_message_`.
  UIImage* GetSymbolImage();

 private:
  // Reopens the previous tab or the latest closed tab contained in
  // `instant_message_`.
  void ReopenTab();

  // Starts a manage share kit flow for the collaboration group contained in
  // `instant_message_`.
  void ManageGroup();

  raw_ptr<ProfileIOS> profile_;
  collaboration::messaging::InstantMessage instant_message_;
};

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_MESSAGING_INFOBAR_COLLABORATION_GROUP_INFOBAR_DELEGATE_H_
