// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/ios_send_tab_to_self_infobar_delegate.h"

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/page_context.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/window_open_disposition.h"

namespace {

// Returns the index of the WebState in `web_state_list` belonging to the most
// recently received Send Tab to Self batch, ignoring previously received tabs.
int GetMostRecentReceivedWebStateIndex(WebStateList* web_state_list) {
  if (!web_state_list) {
    return WebStateList::kInvalidIndex;
  }

  // Iterate through all tabs in the WebStateList to find the one with the
  // newest Send Tab to Self card label timestamp. Using `>=` ensures that when
  // multiple background tabs are opened simultaneously in a batch (and share
  // identical timestamps), the last inserted tab at the highest index is
  // selected.
  int received_tab_index = WebStateList::kInvalidIndex;
  base::Time max_creation_time = base::Time::Min();
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    SendTabToSelfTabCardLabelData* label_data =
        SendTabToSelfTabCardLabelData::FromWebState(web_state);
    if (!label_data) {
      continue;
    }
    if (label_data->creation_time() >= max_creation_time) {
      max_creation_time = label_data->creation_time();
      received_tab_index = i;
    }
  }
  return received_tab_index;
}

}  // namespace

namespace send_tab_to_self {

// static
std::unique_ptr<IOSSendTabToSelfInfoBarDelegate>
IOSSendTabToSelfInfoBarDelegate::Create(const SendTabToSelfEntry* entry,
                                        size_t opened_tab_count,
                                        SendTabToSelfModel* model,
                                        id<SceneCommands> scene_handler,
                                        WebStateList* web_state_list) {
  return std::make_unique<IOSSendTabToSelfInfoBarDelegate>(
      entry, opened_tab_count, model, scene_handler, web_state_list);
}

IOSSendTabToSelfInfoBarDelegate::~IOSSendTabToSelfInfoBarDelegate() = default;

const std::string& IOSSendTabToSelfInfoBarDelegate::GetGUID() const {
  return guid_;
}

IOSSendTabToSelfInfoBarDelegate::IOSSendTabToSelfInfoBarDelegate(
    const SendTabToSelfEntry* entry,
    size_t opened_tab_count,
    SendTabToSelfModel* model,
    id<SceneCommands> scene_handler,
    WebStateList* web_state_list)
    : model_(model),
      opened_tab_count_(opened_tab_count),
      scene_handler_(scene_handler),
      web_state_list_(web_state_list),
      guid_(entry->GetGUID()) {
  CHECK(entry, base::NotFatalUntil::M158);
  CHECK(model, base::NotFatalUntil::M158);
  CHECK(scene_handler, base::NotFatalUntil::M158);
  CHECK(web_state_list_, base::NotFatalUntil::M158);
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
  send_tab_to_self::RecordNotificationStatus(
      send_tab_to_self::NotificationStatus::kDismissed);
  Cancel();
}

std::u16string IOSSendTabToSelfInfoBarDelegate::GetTitleText() const {
  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_TITLE,
        static_cast<int>(opened_tab_count_));
  }
  return std::u16string();
}

std::u16string IOSSendTabToSelfInfoBarDelegate::GetMessageText() const {
  if (base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    const SendTabToSelfEntry* entry = model_->GetEntryByGUID(guid_);
    return entry ? l10n_util::GetStringFUTF16(
                       IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE,
                       base::UTF8ToUTF16(entry->GetDeviceName()))
                 : std::u16string();
  }
  return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_INFOBAR_MESSAGE);
}

bool IOSSendTabToSelfInfoBarDelegate::Accept() {
  send_tab_to_self::RecordNotificationStatus(
      send_tab_to_self::NotificationStatus::kOpened);

  const SendTabToSelfEntry* entry = model_->GetEntryByGUID(guid_);

  if (!entry) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    // Open the tab directly in foreground when auto-open feature is disabled.
    [scene_handler_
        openURLInNewTab:send_tab_to_self::CreateOpenNewTabCommand(entry)];
    model_->MarkEntryOpened(guid_);
    return true;
  }

  int received_tab_index = GetMostRecentReceivedWebStateIndex(web_state_list_);
  if (received_tab_index != WebStateList::kInvalidIndex) {
    // Directly activate the most recently received tab in the foreground.
    web_state_list_->ActivateWebStateAt(received_tab_index);
  } else {
    // Fall back to opening the Tab Grid if the received tab can no longer be
    // found (e.g., if it was closed by the time Accept() is called).
    [scene_handler_ displayTabGridInMode:TabGridOpeningMode::kRegular];
  }
  return true;
}

bool IOSSendTabToSelfInfoBarDelegate::Cancel() {
  model_->DismissEntry(guid_);
  return true;
}

}  // namespace send_tab_to_self
