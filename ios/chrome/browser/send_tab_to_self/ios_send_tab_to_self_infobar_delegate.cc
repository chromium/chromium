// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/ios_send_tab_to_self_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/infobars/core/infobar.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_metrics.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

namespace send_tab_to_self {

// static
std::unique_ptr<IOSSendTabToSelfInfoBarDelegate>
IOSSendTabToSelfInfoBarDelegate::Create(const SendTabToSelfEntry* entry,
                                        SendTabToSelfModel* model) {
  return std::make_unique<IOSSendTabToSelfInfoBarDelegate>(entry, model);
}

IOSSendTabToSelfInfoBarDelegate::~IOSSendTabToSelfInfoBarDelegate() {}

IOSSendTabToSelfInfoBarDelegate::IOSSendTabToSelfInfoBarDelegate(
    const SendTabToSelfEntry* entry,
    SendTabToSelfModel* model)
    : entry_(entry), model_(model) {
  DCHECK(entry);
  DCHECK(model);
  RecordNotificationHistogram(SendTabToSelfNotification::kShown);
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSSendTabToSelfInfoBarDelegate::GetIdentifier() const {
  return SEND_TAB_TO_SELF_INFOBAR_DELEGATE;
}

int IOSSendTabToSelfInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 IOSSendTabToSelfInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE_URL);
}

int IOSSendTabToSelfInfoBarDelegate::GetIconId() const {
  return IDR_IOS_INFOBAR_SEND_TAB_TO_SELF;
}

void IOSSendTabToSelfInfoBarDelegate::InfoBarDismissed() {
  Cancel();
}

base::string16 IOSSendTabToSelfInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE);
}

bool IOSSendTabToSelfInfoBarDelegate::Accept() {
  model_->MarkEntryOpened(entry_->GetGUID());
  RecordNotificationHistogram(SendTabToSelfNotification::kOpened);
  infobar()->owner()->OpenURL(entry_->GetURL(),
                              WindowOpenDisposition::NEW_FOREGROUND_TAB);
  return true;
}

bool IOSSendTabToSelfInfoBarDelegate::Cancel() {
  model_->DismissEntry(entry_->GetGUID());
  RecordNotificationHistogram(SendTabToSelfNotification::kDismissed);
  return true;
}

}  // namespace send_tab_to_self
