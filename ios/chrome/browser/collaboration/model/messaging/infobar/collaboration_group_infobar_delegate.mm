// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/infobar/collaboration_group_infobar_delegate.h"

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/data_sharing/public/group_data.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/share_kit/model/share_kit_avatar_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_avatar_primitive.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/collaboration_group_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using collaboration::messaging::CollaborationEvent;

namespace {

// the size of avatars in points.
const CGFloat kAvatarSize = 30;

// Returns the `local_group_id` attached to the given `instant_message` if any.
std::optional<tab_groups::LocalTabGroupID> GetLocalTabGroupId(
    collaboration::messaging::InstantMessage instant_message) {
  if (instant_message.attributions.empty()) {
    return std::nullopt;
  }
  // For an InstantMessage, even if it aggregates multiple underlying events,
  // they should all be related to the same tab or tab group.
  if (instant_message.attributions.front().tab_group_metadata.has_value()) {
    return instant_message.attributions.front()
        .tab_group_metadata->local_tab_group_id;
  }
  return std::nullopt;
}

// Returns wether the `local_tab_group_id` is contained in `browser`.
bool ContainsLocalTabGroup(
    std::optional<tab_groups::LocalTabGroupID> local_tab_group_id,
    Browser* browser) {
  if (!local_tab_group_id.has_value()) {
    return false;
  }

  WebStateList* web_state_list = browser->GetWebStateList();
  for (const TabGroup* group : web_state_list->GetGroups()) {
    if (group->tab_group_id() == local_tab_group_id.value()) {
      return true;
    }
  }
  return false;
}

// Returns the Browser on which to display the infobar for the given
// `instant_message`.
Browser* GetBrowserFromInstantMessage(
    collaboration::messaging::InstantMessage instant_message,
    BrowserList* browser_list) {
  std::set<Browser*> browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);
  std::optional<tab_groups::LocalTabGroupID> local_tab_group_id =
      GetLocalTabGroupId(instant_message);

  bool use_first_available_browser = false;
  switch (instant_message.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      break;
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::UNDEFINED:
    case CollaborationEvent::COLLABORATION_REMOVED:
      use_first_available_browser = true;
      break;
  }

  for (Browser* browser : browsers) {
    if (!local_tab_group_id.has_value() || use_first_available_browser) {
      // If `local_tab_group_id` is empty, use the first available browser.
      return browser;
    }
    if (ContainsLocalTabGroup(local_tab_group_id, browser)) {
      return browser;
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
  Browser* source_browser =
      GetBrowserFromInstantMessage(instant_message, browser_list);
  if (!source_browser) {
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

void CollaborationGroupInfoBarDelegate::ClearCollaborationGroupInfobars(
    ProfileIOS* profile,
    const std::set<base::Uuid>& message_ids) {
  // Only check the first available regular browser.
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  Browser* browser = GetBrowserFromInstantMessage(
      collaboration::messaging::InstantMessage(), browser_list);
  if (!browser) {
    return;
  }

  web::WebState* active_web_state =
      browser->GetWebStateList()->GetActiveWebState();
  if (!active_web_state) {
    return;
  }

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(active_web_state);

  // Iterate in reverse. This prevents issues when removing infobars.
  const auto& infobars = infobar_manager->infobars();
  for (int i = static_cast<int>(infobars.size()) - 1; i >= 0; --i) {
    infobars::InfoBar* infobar = infobars[i];

    InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
    if (infobar_ios->infobar_type() !=
        InfobarType::kInfobarTypeCollaborationGroup) {
      continue;
    }

    CollaborationGroupInfoBarDelegate* delegate =
        static_cast<CollaborationGroupInfoBarDelegate*>(
            infobar_ios->delegate());
    if (!delegate) {
      continue;
    }

    // Retrieve the instant message identifier for the infobar.
    std::optional<base::Uuid> opt_message_id =
        delegate->GetInstantMessageIdentifier();
    if (!opt_message_id) {
      continue;
    }

    // Remove the infobar if its message ID is in the set of IDs to clear.
    base::Uuid message_id = opt_message_id.value();
    if (message_ids.count(message_id)) {
      infobar_manager->RemoveInfoBar(infobar);
    }
  }
}

CollaborationGroupInfoBarDelegate::CollaborationGroupInfoBarDelegate(
    ProfileIOS* profile,
    collaboration::messaging::InstantMessage instant_message)
    : profile_(profile), instant_message_(instant_message) {
  CHECK(!profile->IsOffTheRecord());
}

CollaborationGroupInfoBarDelegate::~CollaborationGroupInfoBarDelegate() {}

std::optional<base::Uuid>
CollaborationGroupInfoBarDelegate::GetInstantMessageIdentifier() const {
  const auto& attributions = instant_message_.attributions;
  if (attributions.empty()) {
    return std::nullopt;
  }

  const auto& message_id = attributions.at(0).id;
  if (!message_id.has_value() || !message_id.value().is_valid()) {
    return std::nullopt;
  }

  return message_id;
}

infobars::InfoBarDelegate::InfoBarIdentifier
CollaborationGroupInfoBarDelegate::GetIdentifier() const {
  return COLLABORATION_GROUP_UPDATE_INFOBAR_DELEGATE;
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
  switch (instant_message_.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
      return l10n_util::GetStringUTF16(
          IDS_IOS_COLLABORATION_GROUP_TAB_REOPEN_PRIMARY_TOOLBAR_BUTTON);
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
      return l10n_util::GetStringUTF16(
          IDS_IOS_COLLABORATION_GROUP_MEMBER_ADDED_PRIMARY_TOOLBAR_BUTTON);
    case CollaborationEvent::UNDEFINED:
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      return l10n_util::GetStringUTF16(
          IDS_IOS_COLLABORATION_GROUP_DEFAULT_PRIMARY_TOOLBAR_BUTTON);
  }
}

bool CollaborationGroupInfoBarDelegate::Accept() {
  switch (instant_message_.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
      ReopenTab();
      break;
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
      ManageGroup();
      break;
    case CollaborationEvent::UNDEFINED:
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      break;
  }

  return true;
}

void CollaborationGroupInfoBarDelegate::InfoBarDismissed() {
  ConfirmInfoBarDelegate::InfoBarDismissed();
}

bool CollaborationGroupInfoBarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  if (delegate->GetIdentifier() != GetIdentifier()) {
    return false;
  }
  CollaborationGroupInfoBarDelegate* collaboration_delegate =
      static_cast<CollaborationGroupInfoBarDelegate*>(delegate);
  return collaboration_delegate->GetInstantMessageIdentifier() ==
         GetInstantMessageIdentifier();
}

id<ShareKitAvatarPrimitive>
CollaborationGroupInfoBarDelegate::GetAvatarPrimitive() {
  const auto& attributions = instant_message_.attributions;
  if (attributions.size() != 1) {
    // No avatar primitive if not exactly one affected user.
    return nil;
  }

  const auto& attribution = attributions.front();
  const auto& opt_user = instant_message_.collaboration_event ==
                                 CollaborationEvent::COLLABORATION_MEMBER_ADDED
                             ? attribution.affected_user
                             : attribution.triggering_user;

  if (!opt_user) {
    // No avatar primitive if no user.
    return nil;
  }

  data_sharing::GroupMember user = opt_user.value();
  ShareKitService* share_kit_service =
      ShareKitServiceFactory::GetForProfile(profile_);

  ShareKitAvatarConfiguration* config =
      [[ShareKitAvatarConfiguration alloc] init];
  config.avatarUrl =
      [NSURL URLWithString:base::SysUTF8ToNSString(user.avatar_url.spec())];
  // Use email instead when the display name is empty.
  config.displayName = user.display_name.empty()
                           ? base::SysUTF8ToNSString(user.email)
                           : base::SysUTF8ToNSString(user.display_name);
  config.avatarSize = CGSizeMake(kAvatarSize, kAvatarSize);

  return share_kit_service->AvatarImage(config);
}

UIImage* CollaborationGroupInfoBarDelegate::GetSymbolImage() {
  NSString* symbolName;
  switch (instant_message_.collaboration_event) {
    case CollaborationEvent::TAB_UPDATED:
    case CollaborationEvent::TAB_REMOVED:
    case CollaborationEvent::COLLABORATION_MEMBER_ADDED:
    case CollaborationEvent::UNDEFINED:
    case CollaborationEvent::TAB_ADDED:
    case CollaborationEvent::TAB_GROUP_ADDED:
    case CollaborationEvent::TAB_GROUP_NAME_UPDATED:
    case CollaborationEvent::TAB_GROUP_COLOR_UPDATED:
    case CollaborationEvent::COLLABORATION_ADDED:
    case CollaborationEvent::COLLABORATION_MEMBER_REMOVED:
      symbolName = kMultiIdentitySymbol;
      break;
    case CollaborationEvent::TAB_GROUP_REMOVED:
    case CollaborationEvent::COLLABORATION_REMOVED:
      symbolName = kTabGroupsSymbol;
      break;
  }
  return DefaultSymbolTemplateWithPointSize(symbolName,
                                            kInfobarSymbolPointSize);
}

void CollaborationGroupInfoBarDelegate::ReopenTab() {
  std::optional<tab_groups::LocalTabGroupID> local_tab_group_id =
      GetLocalTabGroupId(instant_message_);
  if (!local_tab_group_id.has_value()) {
    return;
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  tab_groups::utils::LocalTabGroupInfo group_info =
      tab_groups::utils::GetLocalTabGroupInfo(browser_list,
                                              local_tab_group_id.value());
  if (!group_info.tab_group) {
    return;
  }

  // Configure `params` for the reopen updated tab flow.
  UrlLoadParams params;
  if (instant_message_.collaboration_event == CollaborationEvent::TAB_UPDATED &&
      instant_message_.attributions.front().tab_metadata.has_value() &&
      instant_message_.attributions.front()
          .tab_metadata->previous_url.has_value()) {
    // Extract the previous URL.
    GURL previous_url = GURL(instant_message_.attributions.front()
                                 .tab_metadata->previous_url.value());
    params = UrlLoadParams::InCurrentTab(previous_url);

    // Configure `params` for the reopen closed tab flow.
  } else if (instant_message_.collaboration_event ==
                 CollaborationEvent::TAB_REMOVED &&
             instant_message_.attributions.front().tab_metadata.has_value() &&
             instant_message_.attributions.front()
                 .tab_metadata->last_known_url.has_value()) {
    GURL last_known_url = GURL(instant_message_.attributions.front()
                                   .tab_metadata->last_known_url.value());
    params = UrlLoadParams::InNewTab(last_known_url);
    params.load_in_group = true;
    params.tab_group = group_info.tab_group->GetWeakPtr();
  } else {
    NOTREACHED();
  }

  UrlLoadingBrowserAgent::FromBrowser(group_info.browser)->Load(params);
}

void CollaborationGroupInfoBarDelegate::ManageGroup() {
  std::optional<tab_groups::LocalTabGroupID> local_tab_group_id =
      GetLocalTabGroupId(instant_message_);
  if (!local_tab_group_id.has_value()) {
    return;
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  tab_groups::utils::LocalTabGroupInfo group_info =
      tab_groups::utils::GetLocalTabGroupInfo(browser_list,
                                              local_tab_group_id.value());
  if (!group_info.tab_group) {
    return;
  }

  id<CollaborationGroupCommands> collaborationGroupHandler = HandlerForProtocol(
      group_info.browser->GetCommandDispatcher(), CollaborationGroupCommands);
  [collaborationGroupHandler
      shareOrManageTabGroup:group_info.tab_group
                 entryPoint:collaboration::
                                CollaborationServiceShareOrManageEntryPoint::
                                    kiOSMessage];
}
