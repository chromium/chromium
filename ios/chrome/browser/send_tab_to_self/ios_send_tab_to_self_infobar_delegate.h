// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_IOS_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_IOS_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
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
  ~IOSSendTabToSelfInfoBarDelegate() override;

 private:

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  int GetIconId() const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;

  // The entry that was share to this device. Must outlive this instance.
  const SendTabToSelfEntry* entry_ = nullptr;

  // The SendTabToSelfModel that holds the |entry_|. Must outlive this instance.
  SendTabToSelfModel* model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(IOSSendTabToSelfInfoBarDelegate);
};

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_IOS_SEND_TAB_TO_SELF_INFOBAR_DELEGATE_H_