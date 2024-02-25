// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_IOS_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_IOS_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_

#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace send_tab_to_self {

class SendTabToSelfEntry;
class SendTabToSelfModel;

class IOSSendTabToSelfInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  static std::unique_ptr<IOSSendTabToSelfInfoBarDelegate> Create(
      const SendTabToSelfEntry* entry,
      SendTabToSelfModel* model);

  explicit IOSSendTabToSelfInfoBarDelegate(const SendTabToSelfEntry* entry,
                                           SendTabToSelfModel* model);

  IOSSendTabToSelfInfoBarDelegate(const IOSSendTabToSelfInfoBarDelegate&) =
      delete;
  IOSSendTabToSelfInfoBarDelegate& operator=(
      const IOSSendTabToSelfInfoBarDelegate&) = delete;

  ~IOSSendTabToSelfInfoBarDelegate() override;

 private:
  // ConfirmInfoBarDelegate:
  InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  int GetIconId() const override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;

  // Send the notice of conclusion of this infobar to other windows.
  void SendConclusionNotification();

  // The entry that was share to this device. Must outlive this instance.
  raw_ptr<const SendTabToSelfEntry> entry_ = nullptr;

  // The SendTabToSelfModel that holds the `entry_`. Must outlive this instance.
  raw_ptr<SendTabToSelfModel> model_ = nullptr;

  // Registration with NSNotificationCenter for this window.
  __strong id<NSObject> registration_ = nil;

  base::WeakPtrFactory<IOSSendTabToSelfInfoBarDelegate> weak_ptr_factory_;
};

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_IOS_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_
