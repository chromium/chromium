// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_group_infobar_delegate.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

using collaboration::messaging::CollaborationEvent;

namespace {

// Returns the `local_group_id` attached to the given `instant_message` if any.
std::optional<tab_groups::LocalTabGroupID> GetLocalTabGroupId(
    collaboration::messaging::InstantMessage instant_message) {
  if (instant_message.attributions.empty()) {
    return std::nullopt;
  }
  // For an InstantMessage, even if it aggregates multiple underlying events,
  // they should all be related to the same tab or tab group.
  if (instant_message.attributions[0].tab_group_metadata.has_value()) {
    return instant_message.attributions[0]
        .tab_group_metadata->local_tab_group_id;
  }
  return std::nullopt;
}

// Returns the local tab group for the given `local_tab_group_id` and `browser`.
const TabGroup* GetLocalTabGroup(
    std::optional<tab_groups::LocalTabGroupID> local_tab_group_id,
    Browser* browser) {
  if (!local_tab_group_id.has_value()) {
    return nullptr;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  for (const TabGroup* group : web_state_list->GetGroups()) {
    if (group->tab_group_id() == local_tab_group_id.value()) {
      return group;
    }
  }
  return nullptr;
}

}  // namespace

// static
bool CollaborationGroupInfoBarDelegate::Create(
    ProfileIOS* profile,
    collaboration::messaging::InstantMessage instant_message) {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  std::set<Browser*> browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);
  std::optional<tab_groups::LocalTabGroupID> local_tab_group_id =
      GetLocalTabGroupId(instant_message);

  // Retrieve the `source_browser`.
  Browser* source_browser;
  for (Browser* browser : browsers) {
    // TODO(crbug.com/375595834): Handle cases where the `local_tab_group_id` is
    // not set.
    if (!local_tab_group_id.has_value()) {
      // If `local_tab_group_id` is empty, use the first available browser.
      source_browser = browser;
      break;
    }
    if (GetLocalTabGroup(local_tab_group_id, browser)) {
      source_browser = browser;
    }
  }
  if (!source_browser) {
    // Early return if the group does not exist locally.
    return false;
  }

  web::WebState* active_web_state =
      source_browser->GetWebStateList()->GetActiveWebState();
  if (!active_web_state) {
    return false;
  }

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(active_web_state);

  std::unique_ptr<CollaborationGroupInfoBarDelegate> delegate(
      new CollaborationGroupInfoBarDelegate(profile, instant_message));
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeCollaborationGroup, std::move(delegate));
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

CollaborationGroupInfoBarDelegate::CollaborationGroupInfoBarDelegate(
    ProfileIOS* profile,
    collaboration::messaging::InstantMessage instant_message)
    : profile_(profile), instant_message_(instant_message) {
  CHECK(!profile->IsOffTheRecord());
}

CollaborationGroupInfoBarDelegate::~CollaborationGroupInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
CollaborationGroupInfoBarDelegate::GetIdentifier() const {
  return TAB_SHARING_INFOBAR_DELEGATE;
}

std::u16string CollaborationGroupInfoBarDelegate::GetMessageText() const {
  return u"";
}

std::u16string CollaborationGroupInfoBarDelegate::GetTitleText() const {
  return instant_message_.localized_message;
}

int CollaborationGroupInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string CollaborationGroupInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  // TODO(crbug.com/375595834): Add real strings.
  switch (instant_message_.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
      return u"test REOPEN";
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
      return u"test MANAGE";
    case CollaborationEvent::UNDEFINED:
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      return u"";
  }
}

bool CollaborationGroupInfoBarDelegate::Accept() {
  // TODO(crbug.com/375595834): Implement this.
  return true;
}

void CollaborationGroupInfoBarDelegate::InfoBarDismissed() {
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

UIImage* CollaborationGroupInfoBarDelegate::GetAvatarImage() {
  // TODO(crbug.com/375595834): Update this to show avatars when needed.
  return nil;
}
