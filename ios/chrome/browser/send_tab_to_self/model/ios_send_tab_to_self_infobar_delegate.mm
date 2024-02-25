// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/ios_send_tab_to_self_infobar_delegate.h"

#import <Foundation/Foundation.h>

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/window_open_disposition.h"

namespace {

NSString* kSendTabToSendConclusionNotification =
    @"SendTabToSendConclusionNotification";

}  // namespace

namespace send_tab_to_self {

// static
std::unique_ptr<IOSSendTabToSelfInfoBarDelegate>
IOSSendTabToSelfInfoBarDelegate::Create(const SendTabToSelfEntry* entry,
                                        SendTabToSelfModel* model) {
  return std::make_unique<IOSSendTabToSelfInfoBarDelegate>(entry, model);
}

IOSSendTabToSelfInfoBarDelegate::~IOSSendTabToSelfInfoBarDelegate() {
  [[NSNotificationCenter defaultCenter]
      removeObserver:registration_
                name:kSendTabToSendConclusionNotification
              object:nil];
}

IOSSendTabToSelfInfoBarDelegate::IOSSendTabToSelfInfoBarDelegate(
    const SendTabToSelfEntry* entry,
    SendTabToSelfModel* model)
    : entry_(entry), model_(model), weak_ptr_factory_(this) {
  DCHECK(entry);
  DCHECK(model);

  base::WeakPtr<IOSSendTabToSelfInfoBarDelegate> weakPtr =
      weak_ptr_factory_.GetWeakPtr();
  // Observe for conclusion notification from other instances.
  registration_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:kSendTabToSendConclusionNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* note) {
                if (!weakPtr) {
                  return;
                }

                // Ignore the notification if it was sent by `weakPtr` (i.e. the
                // instance that responded first to the send to self infobar)
                if (note.object == weakPtr->registration_) {
                  return;
                }

                infobars::InfoBar* infobar = weakPtr->infobar();
                infobars::InfoBarManager* owner = infobar->owner();
                if (!owner) {
                  return;
                }

                owner->RemoveInfoBar(infobar);
              }];
}

infobars::InfoBarDelegate::InfoBarIdentifier
IOSSendTabToSelfInfoBarDelegate::GetIdentifier() const {
  return SEND_TAB_TO_SELF_INFOBAR_DELEGATE;
}

int IOSSendTabToSelfInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string IOSSendTabToSelfInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE_URL);
}

int IOSSendTabToSelfInfoBarDelegate::GetIconId() const {
  return IDR_IOS_INFOBAR_SEND_TAB_TO_SELF;
}

void IOSSendTabToSelfInfoBarDelegate::InfoBarDismissed() {
  send_tab_to_self::RecordNotificationDismissed();
  Cancel();
}

std::u16string IOSSendTabToSelfInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE);
}

bool IOSSendTabToSelfInfoBarDelegate::Accept() {
  send_tab_to_self::RecordNotificationOpened();
  model_->MarkEntryOpened(entry_->GetGUID());
  infobar()->owner()->OpenURL(entry_->GetURL(),
                              WindowOpenDisposition::NEW_FOREGROUND_TAB);
  SendConclusionNotification();
  return true;
}

bool IOSSendTabToSelfInfoBarDelegate::Cancel() {
  model_->DismissEntry(entry_->GetGUID());
  SendConclusionNotification();
  return true;
}

void IOSSendTabToSelfInfoBarDelegate::SendConclusionNotification() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kSendTabToSendConclusionNotification
                    object:registration_
                  userInfo:nil];
}

}  // namespace send_tab_to_self
