// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/web_ax_platform_tree_manager_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_private_webkit_constants_mac.h"
#include "ui/base/cocoa/remote_accessibility_api.h"

namespace {

// Use same value as in Safari's WebKit.
const int kLiveRegionChangeIntervalMS = 20;

}  // namespace

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    WebAXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerMac(initial_tree, delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    WebAXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerMac(
      BrowserAccessibilityManagerMac::GetEmptyDocument(), delegate);
}

BrowserAccessibilityManagerMac*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerMac() {
  return static_cast<BrowserAccessibilityManagerMac*>(this);
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    const ui::AXTreeUpdate& initial_tree,
    WebAXPlatformTreeManagerDelegate* delegate)
    : BrowserAccessibilityManager(delegate) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerMac::~BrowserAccessibilityManagerMac() = default;

// static
ui::AXTreeUpdate BrowserAccessibilityManagerMac::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 1;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerMac::FireFocusEvent(ui::AXNode* node) {
  ui::AXTreeManager::FireFocusEvent(node);
  FireNativeMacNotification(NSAccessibilityFocusedUIElementChangedNotification,
                            GetFromAXNode(node));
}

void BrowserAccessibilityManagerMac::FireBlinkEvent(ax::mojom::Event event_type,
                                                    BrowserAccessibility* node,
                                                    int action_request_id) {
  BrowserAccessibilityManager::FireBlinkEvent(event_type, node,
                                              action_request_id);
  NSString* mac_notification = nullptr;
  switch (event_type) {
    case ax::mojom::Event::kAutocorrectionOccured:
      mac_notification = ui::NSAccessibilityAutocorrectionOccurredNotification;
      break;
    case ax::mojom::Event::kLoadComplete:
      if (!ShouldFireLoadCompleteNotification())
        return;
      mac_notification = ui::NSAccessibilityLoadCompleteNotification;
      break;
    default:
      return;
  }

  FireNativeMacNotification(mac_notification, node);
}

void PostAnnouncementNotification(NSString* announcement,
                                  NSWindow* window,
                                  NSAccessibilityPriorityLevel priorityLevel) {
  NSDictionary* notification_info = @{
    NSAccessibilityAnnouncementKey : announcement,
    NSAccessibilityPriorityKey : @(priorityLevel)
  };
  // Trigger VoiceOver speech and show on Braille display, if available.
  // The Braille will only appear for a few seconds, and then will be replaced
  // with the previous announcement.
  NSAccessibilityPostNotificationWithUserInfo(
      window, NSAccessibilityAnnouncementRequestedNotification,
      notification_info);
}

// Check whether the current batch of events contains the event type.
bool BrowserAccessibilityManagerMac::IsInGeneratedEventBatch(
    ui::AXEventGenerator::Event event_type) const {
  for (const auto& event : event_generator()) {
    if (event.event_params.event == event_type)
      return true;  // Any side effects will have already been handled.
  }
  return false;
}

void BrowserAccessibilityManagerMac::FireGeneratedEvent(
    ui::AXEventGenerator::Event event_type,
    const ui::AXNode* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);
  BrowserAccessibility* wrapper = GetFromAXNode(node);
  DCHECK(wrapper);
  BrowserAccessibilityCocoa* native_node = wrapper->GetNativeViewAccessible();
  DCHECK(native_node);

  // Refer to |AXObjectCache::postPlatformNotification| in WebKit source code.
  NSString* mac_notification = nullptr;
  switch (event_type) {
    case ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      if (wrapper->GetRole() == ax::mojom::Role::kTree) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else if (wrapper->GetRole() ==
                 ax::mojom::Role::kTextFieldWithComboBox) {
        // Even though the selected item in the combo box has changed, we don't
        // want to post a focus change because this will take the focus out of
        // the combo box where the user might be typing.
        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      } else {
        // In all other cases we should post
        // |NSAccessibilityFocusedUIElementChangedNotification|, but this is
        // handled elsewhere.
        return;
      }
      break;
    case ui::AXEventGenerator::Event::ALERT:
      NSAccessibilityPostNotification(
          native_node, ui::NSAccessibilityLiveRegionCreatedNotification);
      // Voiceover requires a live region changed notification to actually
      // announce the live region.
      FireGeneratedEvent(ui::AXEventGenerator::Event::LIVE_REGION_CHANGED,
                         node);
      return;
    case ui::AXEventGenerator::Event::ARIA_CURRENT_CHANGED:
      // TODO(accessibility) Ask Apple for a notification.
      // There currently is none:
      // https://www.w3.org/TR/core-aam-1.2/#details-id-186
      return;
    case ui::AXEventGenerator::Event::BUSY_CHANGED:
      mac_notification = ui::NSAccessibilityElementBusyChangedNotification;
      break;
    case ui::AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      break;
    case ui::AXEventGenerator::Event::COLLAPSED:
      if (wrapper->GetRole() == ax::mojom::Role::kRow ||
          wrapper->GetRole() == ax::mojom::Role::kTreeItem) {
        mac_notification = NSAccessibilityRowCollapsedNotification;
      } else {
        mac_notification = ui::NSAccessibilityExpandedChanged;
      }
      break;
    case ui::AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      mac_notification = NSAccessibilitySelectedTextChangedNotification;
      // WebKit fires a notification both on the focused object and the page
      // root.
      BrowserAccessibility* focus = GetFocus();
      if (!focus)
        break;  // Just fire a notification on the root.

      NSDictionary* user_info = GetUserInfoForSelectedTextChangedNotification();

      BrowserAccessibilityManager* root_manager = GetManagerForRootFrame();
      if (!root_manager)
        return;
      BrowserAccessibility* root = root_manager->GetBrowserAccessibilityRoot();
      if (!root)
        return;

      NSAccessibilityPostNotificationWithUserInfo(
          focus->GetNativeViewAccessible(), mac_notification, user_info);
      NSAccessibilityPostNotificationWithUserInfo(
          root->GetNativeViewAccessible(), mac_notification, user_info);
      return;
    }
    case ui::AXEventGenerator::Event::EXPANDED:
      if (wrapper->GetRole() == ax::mojom::Role::kRow ||
          wrapper->GetRole() == ax::mojom::Role::kTreeItem) {
        mac_notification = NSAccessibilityRowExpandedNotification;
      } else {
        mac_notification = ui::NSAccessibilityExpandedChanged;
      }
      break;
    case ui::AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      mac_notification = ui::NSAccessibilityInvalidStatusChangedNotification;
      break;
    case ui::AXEventGenerator::Event::LIVE_REGION_CHANGED: {
      // Voiceover seems to drop live region changed notifications if they come
      // too soon after a live region created notification.
      // TODO(nektar): Limit the number of changed notifications as well.

      if (never_suppress_or_delay_events_for_testing_) {
        NSAccessibilityPostNotification(
            native_node, ui::NSAccessibilityLiveRegionChangedNotification);
        return;
      }

      BrowserAccessibilityManager* root_manager = GetManagerForRootFrame();
      if (root_manager) {
        BrowserAccessibilityManagerMac* root_manager_mac =
            root_manager->ToBrowserAccessibilityManagerMac();
        id window = root_manager_mac->GetWindow();
        if ([window isKindOfClass:[NSAccessibilityRemoteUIElement class]]) {
          // ui::NSAccessibilityLiveRegionChangedNotification seems to require
          // application be active. Use the announcement API to get around on
          // PWA. Announcement requires active window, so send the announcement
          // notification to the PWA related window. same work around like
          // https://chromium-review.googlesource.com/c/chromium/src/+/3257815
          std::string live_status =
              node->GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
          NSAccessibilityPriorityLevel priority_level =
              live_status == "assertive" ? NSAccessibilityPriorityHigh
                                         : NSAccessibilityPriorityMedium;
          PostAnnouncementNotification(
              base::SysUTF16ToNSString(wrapper->GetTextContentUTF16()),
              [root_manager_mac->GetParentView() window], priority_level);
          return;
        }
      }

      if (base::mac::IsOS10_13()) {
        // Use the announcement API to get around OS <= 10.13 VoiceOver bug
        // where it stops announcing live regions after the first time focus
        // leaves any content area.
        // Unfortunately this produces an annoying boing sound with each live
        // announcement, but the alternative is almost no live region support.
        PostAnnouncementNotification(
            base::SysUTF16ToNSString(wrapper->GetTextContentUTF16()),
            [NSApp mainWindow], NSAccessibilityPriorityLow);
        return;
      }

      // Use native VoiceOver support for live regions.
      base::scoped_nsobject<BrowserAccessibilityCocoa> retained_node(
          [native_node retain]);
      GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](base::scoped_nsobject<BrowserAccessibilityCocoa> wrapper) {
                if (wrapper && [wrapper instanceActive]) {
                  NSAccessibilityPostNotification(
                      wrapper,
                      ui::NSAccessibilityLiveRegionChangedNotification);
                }
              },
              std::move(retained_node)),
          base::Milliseconds(kLiveRegionChangeIntervalMS));
      return;
    }
    case ui::AXEventGenerator::Event::LIVE_REGION_CREATED:
      mac_notification = ui::NSAccessibilityLiveRegionCreatedNotification;
      break;
    case ui::AXEventGenerator::Event::MENU_POPUP_END:
      // Calling NSAccessibilityPostNotification on a menu which is about to be
      // closed/destroyed is possible, but the event does not appear to be
      // emitted reliably by the NSAccessibility stack. If VoiceOver is not
      // notified that a given menu has been closed, it might fail to present
      // subsequent changes to the user. WebKit seems to address this by firing
      // AXMenuClosed on the document itself when an accessible menu is being
      // detached. See WebKit's AccessibilityObject::detachRemoteParts
      if (BrowserAccessibilityManager* root_manager =
              GetManagerForRootFrame()) {
        if (BrowserAccessibility* root =
                root_manager->GetBrowserAccessibilityRoot())
          FireNativeMacNotification((NSString*)kAXMenuClosedNotification, root);
      }
      return;
    case ui::AXEventGenerator::Event::MENU_POPUP_START:
      mac_notification = (NSString*)kAXMenuOpenedNotification;
      break;
    case ui::AXEventGenerator::Event::MENU_ITEM_SELECTED:
      mac_notification = ui::NSAccessibilityMenuItemSelectedNotification;
      break;
    case ui::AXEventGenerator::Event::RANGE_VALUE_CHANGED:
      DCHECK(wrapper->GetData().IsRangeValueSupported())
          << "Range value changed but range values are not supported: "
          << wrapper;
      mac_notification = NSAccessibilityValueChangedNotification;
      break;
    case ui::AXEventGenerator::Event::ROW_COUNT_CHANGED:
      mac_notification = NSAccessibilityRowCountChangedNotification;
      break;
    case ui::AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      if (ui::IsTableLike(wrapper->GetRole())) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else {
        // VoiceOver does not read anything if selection changes on the
        // currently focused object, and the focus did not move. Fire a
        // selection change if the focus did not change.
        BrowserAccessibility* focus = GetFocus();
        BrowserAccessibility* container =
            focus ? focus->PlatformGetSelectionContainer() : nullptr;

        if (focus && wrapper == container &&
            container->HasState(ax::mojom::State::kMultiselectable) &&
            !IsInGeneratedEventBatch(
                ui::AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED) &&
            !IsInGeneratedEventBatch(
                ui::AXEventGenerator::Event::FOCUS_CHANGED)) {
          // Force announcement of current focus / activedescendant, even though
          // it's not changing. This way, the user can hear the new selection
          // state of the current object. Because VoiceOver ignores focus events
          // to an already focused object, this is done by destroying the native
          // object and creating a new one that receives focus.
          static_cast<BrowserAccessibilityMac*>(focus)->ReplaceNativeObject();
          // Don't fire selected children change, it will sometimes override
          // announcement of current focus.
          return;
        }

        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      }
      break;
    case ui::AXEventGenerator::Event::SELECTED_VALUE_CHANGED:
      DCHECK(ui::IsSelectElement(wrapper->GetRole()));
      mac_notification = NSAccessibilityValueChangedNotification;
      break;
    case ui::AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      DCHECK(wrapper->IsTextField());
      mac_notification = NSAccessibilityValueChangedNotification;
      if (!text_edits_.empty()) {
        std::u16string deleted_text;
        std::u16string inserted_text;
        int32_t node_id = wrapper->GetId();
        const auto iterator = text_edits_.find(node_id);
        id edit_text_marker = nil;
        if (iterator != text_edits_.end()) {
          AXTextEdit text_edit = iterator->second;
          deleted_text = text_edit.deleted_text;
          inserted_text = text_edit.inserted_text;
          edit_text_marker = text_edit.edit_text_marker;
        }

        NSDictionary* user_info = GetUserInfoForValueChangedNotification(
            native_node, deleted_text, inserted_text, edit_text_marker);

        BrowserAccessibility* root = GetBrowserAccessibilityRoot();
        if (!root)
          return;

        NSAccessibilityPostNotificationWithUserInfo(
            native_node, mac_notification, user_info);
        NSAccessibilityPostNotificationWithUserInfo(
            root->GetNativeViewAccessible(), mac_notification, user_info);
        return;
      }
      break;
    case ui::AXEventGenerator::Event::NAME_CHANGED:
      mac_notification = NSAccessibilityTitleChangedNotification;
      break;

    // Currently unused events on this platform.
    case ui::AXEventGenerator::Event::NONE:
    case ui::AXEventGenerator::Event::ACCESS_KEY_CHANGED:
    case ui::AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::ATOMIC_CHANGED:
    case ui::AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case ui::AXEventGenerator::Event::AUTOFILL_AVAILABILITY_CHANGED:
    case ui::AXEventGenerator::Event::CARET_BOUNDS_CHANGED:
    case ui::AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED:
    case ui::AXEventGenerator::Event::CHILDREN_CHANGED:
    case ui::AXEventGenerator::Event::CONTROLS_CHANGED:
    case ui::AXEventGenerator::Event::CLASS_NAME_CHANGED:
    case ui::AXEventGenerator::Event::DETAILS_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
    case ui::AXEventGenerator::Event::DESCRIPTION_CHANGED:
    case ui::AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
    case ui::AXEventGenerator::Event::DROPEFFECT_CHANGED:
    case ui::AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
    case ui::AXEventGenerator::Event::ENABLED_CHANGED:
    case ui::AXEventGenerator::Event::FOCUS_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_FROM_CHANGED:
    case ui::AXEventGenerator::Event::FLOW_TO_CHANGED:
    case ui::AXEventGenerator::Event::GRABBED_CHANGED:
    case ui::AXEventGenerator::Event::HASPOPUP_CHANGED:
    case ui::AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
    case ui::AXEventGenerator::Event::IGNORED_CHANGED:
    case ui::AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
    case ui::AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
    case ui::AXEventGenerator::Event::LABELED_BY_CHANGED:
    case ui::AXEventGenerator::Event::LANGUAGE_CHANGED:
    case ui::AXEventGenerator::Event::LAYOUT_INVALIDATED:
    case ui::AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
    case ui::AXEventGenerator::Event::LIVE_STATUS_CHANGED:
    case ui::AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
    case ui::AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::OTHER_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::PARENT_CHANGED:
    case ui::AXEventGenerator::Event::PLACEHOLDER_CHANGED:
    case ui::AXEventGenerator::Event::PORTAL_ACTIVATED:
    case ui::AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
    case ui::AXEventGenerator::Event::RANGE_VALUE_MAX_CHANGED:
    case ui::AXEventGenerator::Event::RANGE_VALUE_MIN_CHANGED:
    case ui::AXEventGenerator::Event::RANGE_VALUE_STEP_CHANGED:
    case ui::AXEventGenerator::Event::READONLY_CHANGED:
    case ui::AXEventGenerator::Event::RELATED_NODE_CHANGED:
    case ui::AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
    case ui::AXEventGenerator::Event::ROLE_CHANGED:
    case ui::AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
    case ui::AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
    case ui::AXEventGenerator::Event::SELECTED_CHANGED:
    case ui::AXEventGenerator::Event::SET_SIZE_CHANGED:
    case ui::AXEventGenerator::Event::SORT_CHANGED:
    case ui::AXEventGenerator::Event::STATE_CHANGED:
    case ui::AXEventGenerator::Event::SUBTREE_CREATED:
    case ui::AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
    case ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
    case ui::AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
      return;
  }

  FireNativeMacNotification(mac_notification, wrapper);
}

void BrowserAccessibilityManagerMac::FireNativeMacNotification(
    NSString* mac_notification,
    BrowserAccessibility* node) {
  DCHECK(mac_notification);
  BrowserAccessibilityCocoa* native_node = node->GetNativeViewAccessible();
  DCHECK(native_node);
  // TODO(accessibility) We should look into why background tabs return null for
  // GetWindow. Is it safe to fire notifications when there is no window? We've
  // had trouble in the past with "Chrome is not responding" lockups in AppKit
  // with VoiceOver, when firing events in detached documents.
  // DCHECK(GetWindow());
  NSAccessibilityPostNotification(native_node, mac_notification);
}

bool BrowserAccessibilityManagerMac::OnAccessibilityEvents(
    const AXEventNotificationDetails& details) {
  text_edits_.clear();
  return BrowserAccessibilityManager::OnAccessibilityEvents(details);
}

void BrowserAccessibilityManagerMac::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  std::set<const BrowserAccessibilityCocoa*> changed_editable_roots;
  for (const auto& change : changes) {
    if (change.node->HasState(ax::mojom::State::kEditable)) {
      auto* ancestor = change.node->GetTextFieldAncestor();
      if (ancestor) {
        BrowserAccessibility* obj = GetFromAXNode(ancestor);
        const BrowserAccessibilityCocoa* editable_root =
            obj->GetNativeViewAccessible();
        if ([editable_root instanceActive])
          changed_editable_roots.insert(editable_root);
      }
    }
  }

  for (const BrowserAccessibilityCocoa* obj : changed_editable_roots) {
    DCHECK(obj);
    const AXTextEdit text_edit = [obj computeTextEdit];
    if (!text_edit.IsEmpty())
      text_edits_[[obj owner]->GetId()] = text_edit;
  }
}

NSDictionary* BrowserAccessibilityManagerMac::
    GetUserInfoForSelectedTextChangedNotification() {
  NSMutableDictionary* user_info =
      [[[NSMutableDictionary alloc] init] autorelease];
  [user_info setObject:@YES forKey:ui::NSAccessibilityTextStateSyncKey];
  [user_info setObject:@(ui::AXTextSelectionDirectionUnknown)
                forKey:ui::NSAccessibilityTextSelectionDirection];
  [user_info setObject:@(ui::AXTextSelectionGranularityUnknown)
                forKey:ui::NSAccessibilityTextSelectionGranularity];
  [user_info setObject:@YES
                forKey:ui::NSAccessibilityTextSelectionChangedFocus];

  // Try to detect when the text selection changes due to a focus change.
  // This is necessary so that VoiceOver also anounces information about the
  // element that contains this selection.
  // TODO(mrobinson): Determine definitively what the type of this text
  // selection change is. This requires passing this information here from
  // blink.
  BrowserAccessibility* focus_object = GetFocus();
  DCHECK(focus_object);

  if (focus_object != GetFromAXNode(GetLastFocusedNode())) {
    [user_info setObject:@(ui::AXTextStateChangeTypeSelectionMove)
                  forKey:ui::NSAccessibilityTextStateChangeTypeKey];
  } else {
    [user_info setObject:@(ui::AXTextStateChangeTypeUnknown)
                  forKey:ui::NSAccessibilityTextStateChangeTypeKey];
  }

  focus_object = focus_object->PlatformGetLowestPlatformAncestor();
  BrowserAccessibilityCocoa* native_focus_object =
      focus_object->GetNativeViewAccessible();
  if (native_focus_object && [native_focus_object instanceActive]) {
    [user_info setObject:native_focus_object
                  forKey:ui::NSAccessibilityTextChangeElement];

    id selected_text = [native_focus_object selectedTextMarkerRange];
    if (selected_text) {
      NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
          @"AXSelectedTextMarkerRange";
      [user_info setObject:selected_text
                    forKey:NSAccessibilitySelectedTextMarkerRangeAttribute];
    }
  }

  return user_info;
}

NSDictionary*
BrowserAccessibilityManagerMac::GetUserInfoForValueChangedNotification(
    const BrowserAccessibilityCocoa* native_node,
    const std::u16string& deleted_text,
    const std::u16string& inserted_text,
    id edit_text_marker) const {
  DCHECK(native_node);
  if (deleted_text.empty() && inserted_text.empty())
    return nil;

  NSMutableArray* changes = [[[NSMutableArray alloc] init] autorelease];
  if (!deleted_text.empty()) {
    NSMutableDictionary* change =
        [NSMutableDictionary dictionaryWithDictionary:@{
          ui::NSAccessibilityTextEditType : @(ui::AXTextEditTypeDelete),
          ui::NSAccessibilityTextChangeValueLength : @(deleted_text.length()),
          ui::NSAccessibilityTextChangeValue :
              base::SysUTF16ToNSString(deleted_text)
        }];
    if (edit_text_marker) {
      change[ui::NSAccessibilityChangeValueStartMarker] = edit_text_marker;
    }
    [changes addObject:change];
  }
  if (!inserted_text.empty()) {
    // TODO(nektar): Figure out if this is a paste, insertion or typing.
    // Changes to Blink would be required. A heuristic is currently used.
    auto edit_type = inserted_text.length() > 1 ? @(ui::AXTextEditTypeInsert)
                                                : @(ui::AXTextEditTypeTyping);
    NSMutableDictionary* change =
        [NSMutableDictionary dictionaryWithDictionary:@{
          ui::NSAccessibilityTextEditType : edit_type,
          ui::NSAccessibilityTextChangeValueLength : @(inserted_text.length()),
          ui::NSAccessibilityTextChangeValue :
              base::SysUTF16ToNSString(inserted_text)
        }];
    if (edit_text_marker) {
      change[ui::NSAccessibilityChangeValueStartMarker] = edit_text_marker;
    }
    [changes addObject:change];
  }

  return @{
    ui::
    NSAccessibilityTextStateChangeTypeKey : @(ui::AXTextStateChangeTypeEdit),
    ui::NSAccessibilityTextChangeValues : changes,
    ui::NSAccessibilityTextChangeElement : native_node
  };
}

id BrowserAccessibilityManagerMac::GetParentView() {
  return delegate()->AccessibilityGetNativeViewAccessible();
}

id BrowserAccessibilityManagerMac::GetWindow() {
  return delegate()->AccessibilityGetNativeViewAccessibleForWindow();
}

bool BrowserAccessibilityManagerMac::IsChromeNewTabPage() {
  if (!delegate() || !IsRootFrameManager())
    return false;
  content::WebContents* web_contents = WebContents::FromRenderFrameHost(
      delegate()->AccessibilityRenderFrameHost());
  if (!web_contents)
    return false;
  const GURL& url = web_contents->GetVisibleURL();
  return url == GURL("chrome://newtab/") ||
         url == GURL("chrome://new-tab-page") ||
         url == GURL("chrome-search://local-ntp/local-ntp.html");
}

bool BrowserAccessibilityManagerMac::ShouldFireLoadCompleteNotification() {
  // If it's not the top-level document, we shouldn't fire AXLoadComplete.
  if (!IsRootFrameManager())
    return false;

  // On MacOS 10.15, firing AXLoadComplete causes focus to move to the
  // webpage and read content, despite the "Automatically speak the webpage"
  // checkbox in Voiceover utility being unchecked. The checkbox is
  // unchecked by default in 10.15 so we don't fire AXLoadComplete events to
  // support the default behavior.
  if (base::mac::IsOS10_15())
    return false;

  // Voiceover moves focus to the web content when it receives an
  // AXLoadComplete event. On Chrome's new tab page, focus should stay
  // in the omnibox, so we purposefully do not fire the AXLoadComplete
  // event in this case.
  if (IsChromeNewTabPage())
    return false;

  // We also check that the window is focused because VoiceOver responds
  // to this notification by changing focus and possibly reading the entire
  // page contents, sometimes even when the window is minimized or another
  // Chrome window is active/focused.
  id window = GetWindow();
  if (!window)
    return false;

  if ([NSApp isActive])
    return window == [NSApp accessibilityFocusedWindow];

  // TODO(accessibility): We need a solution to the problem described below.
  // If the window is NSAccessibilityRemoteUIElement, there are some challenges:
  // 1. NSApp is the browser which spawned the PWA, and what it considers the
  //    accessibilityFocusedWindow is the last browser window which was focused
  //    prior to the PWA gaining focus; not the potentially-focused PWA window.
  // 2. Unlike the BrowserNativeWidgetWindow, NSAccessibilityRemoteUIElement is
  //    not an NSWindow and doesn't respond to the selector isKeyWindow. So we
  //    cannot simply verify we have the key window for the currently running
  //    application.
  // 3. NSAccessibilityRemoteUIElement does not conform to the NSAccessibility
  //    protocol, so we cannot ask it for any properties that might let us
  //    verify this window is focused.
  // 4. AppKit does not allow us to access the actual NSWindow instances of
  //    other NSRunningApplications (i.e. the shim process); just window
  //    information, which does not appear to include details regarding what
  //    is active/focused.
  // 5. Attempting to get at the accessibility tree of the shim process via
  //    AXUIElementCreateApplication is possible, but the objects retrieved
  //    in that fashion do not conform to the NSAccessibility protocol.
  // For now we'll return true to preserve current behavior. Note, however,
  // that this does not necessarily mean the event will be presented by
  // VoiceOver in the same way it would present a normal browser window.
  // This may be due to the issues described above, or the fact that one
  // cannot ascend the accessibility tree all the way to the parent window
  // from within the app shim content.
  if ([window isKindOfClass:[NSAccessibilityRemoteUIElement class]])
    return true;

  return false;
}

}  // namespace content
