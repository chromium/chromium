// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_android.h"

#include <algorithm>
#include <unordered_map>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/break_iterator.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/public/common/content_client.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/android/accessibility_state.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"
#include "ui/strings/grit/ax_strings.h"

namespace {

// These are enums from android.text.InputType in Java:
enum {
  ANDROID_TEXT_INPUTTYPE_TYPE_NULL = 0,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME = 0x4,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE = 0x14,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME = 0x24,
  ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER = 0x2,
  ANDROID_TEXT_INPUTTYPE_TYPE_PHONE = 0x3,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT = 0x1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI = 0x11,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EDIT_TEXT = 0xa1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL = 0xd1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD = 0xe1
};

// These are enums from android.view.View in Java:
enum {
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE = 0,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE = 1,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE = 2
};

// These are enums from
// android.view.accessibility.AccessibilityNodeInfo.RangeInfo in Java:
enum { ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT = 1 };

// These define reasons a node may be marked as clickable and provide a
// relative score to AT. Higher means more likely to be the clickable node.
enum {
  kNotClickable = 0,
  kHasClickAncestor = 100,
  kHasClickListener = 200,
  kHasClickListenerAndIsControl = 300
};

}  // namespace

namespace ui {
// static
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    AXNode* node) {
  return base::WrapUnique(
      new content::BrowserAccessibilityAndroid(manager, node));
}
}  // namespace ui

namespace content {

namespace {
// The minimum amount of characters that must be typed into a text field before
// AT will communicate invalid content to the user.
constexpr int kMinimumCharacterCountForInvalid = 7;
}  // namespace

using UniqueIdMap = std::unordered_map<int32_t, BrowserAccessibilityAndroid*>;
// Map from each AXPlatformNode's unique id to its instance.
base::LazyInstance<UniqueIdMap>::Leaky g_unique_id_map =
    LAZY_INSTANCE_INITIALIZER;

// Map from BrowserAccessibilityAndroid nodes to whether they qualify as a
// "leaf". Must be cleared on any tree mutation.
base::LazyInstance<std::map<const BrowserAccessibilityAndroid*, bool>>::Leaky
    g_leaf_map = LAZY_INSTANCE_INITIALIZER;

// static
BrowserAccessibilityAndroid* BrowserAccessibilityAndroid::GetFromUniqueId(
    int32_t unique_id) {
  UniqueIdMap* unique_ids = g_unique_id_map.Pointer();
  auto iter = unique_ids->find(unique_id);
  if (iter != unique_ids->end()) {
    return iter->second;
  }

  return nullptr;
}

// static
void BrowserAccessibilityAndroid::ResetLeafCache() {
  g_leaf_map.Get().clear();
}

BrowserAccessibilityAndroid::BrowserAccessibilityAndroid(
    ui::BrowserAccessibilityManager* manager,
    ui::AXNode* node)
    : BrowserAccessibility(manager, node) {
  g_unique_id_map.Get()[GetUniqueId()] = this;
}

BrowserAccessibilityAndroid::~BrowserAccessibilityAndroid() {
  if (auto id = GetUniqueId()) {
    g_unique_id_map.Get().erase(id);
  }
}

std::u16string BrowserAccessibilityAndroid::GetLocalizedString(
    int message_id) const {
  return CHECK_DEREF(GetContentClient()).GetLocalizedString(message_id);
}

void BrowserAccessibilityAndroid::OnLocationChanged() {
  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  manager->FireLocationChanged(this);
}

std::u16string
BrowserAccessibilityAndroid::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  // Default to standard text, except for special case of eligible.
  if (status != ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation) {
    return BrowserAccessibility::GetLocalizedStringForImageAnnotationStatus(
        status);
  }

  int message_id = 0;
  switch (static_cast<ax::mojom::WritingDirection>(
      GetIntAttribute(ax::mojom::IntAttribute::kTextDirection))) {
    case ax::mojom::WritingDirection::kRtl:
      message_id = IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION_ANDROID_RTL;
      break;
    case ax::mojom::WritingDirection::kTtb:
    case ax::mojom::WritingDirection::kBtt:
    case ax::mojom::WritingDirection::kNone:
    case ax::mojom::WritingDirection::kLtr:
      message_id = IDS_AX_IMAGE_ELIGIBLE_FOR_ANNOTATION_ANDROID_LTR;
      break;
  }

  DCHECK(message_id);

  return GetLocalizedString(message_id);
}

void BrowserAccessibilityAndroid::AppendTextToString(
    std::u16string extra_text,
    std::u16string* string) const {
  if (extra_text.empty()) {
    return;
  }

  if (string->empty()) {
    *string = extra_text;
    return;
  }

  *string += std::u16string(u", ") + extra_text;
}

bool BrowserAccessibilityAndroid::IsCheckable() const {
  return GetData().HasCheckedState();
}

bool BrowserAccessibilityAndroid::IsChecked() const {
  return GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue;
}

bool BrowserAccessibilityAndroid::IsClickable() const {
  // If it has a custom default action verb except for
  // ax::mojom::DefaultActionVerb::kClickAncestor, it's definitely clickable.
  // ax::mojom::DefaultActionVerb::kClickAncestor is used when an element with a
  // click listener is present in its ancestry chain.
  if (HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb) &&
      (GetData().GetDefaultActionVerb() !=
       ax::mojom::DefaultActionVerb::kClickAncestor)) {
    return true;
  }

  if (IsHeadingLink()) {
    return true;
  }

  // Skip web areas, PDFs and iframes, they're focusable but not clickable.
  if (ui::IsIframe(GetRole()) || ui::IsPlatformDocument(GetRole())) {
    return false;
  }

  // Otherwise it's clickable if it's a control. We include disabled nodes
  // because TalkBack won't announce a control as disabled unless it's also
  // marked as clickable. In other words, Talkback wants to know if the control
  // might be clickable, if it wasn't disabled.
  return ui::IsControlOnAndroid(GetRole(), IsFocusable());
}

bool BrowserAccessibilityAndroid::IsCollapsed() const {
  return HasState(ax::mojom::State::kCollapsed);
}

bool BrowserAccessibilityAndroid::IsCollection() const {
  switch (GetRole()) {
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kTree:
      return true;
    default:
      return ui::IsTableLike(GetRole());
  }
}

bool BrowserAccessibilityAndroid::IsCollectionItem() const {
  switch (GetRole()) {
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kTerm:
    case ax::mojom::Role::kTreeItem:
      return true;
    default:
      return ui::IsCellOrTableHeader(GetRole());
  }
}

bool BrowserAccessibilityAndroid::IsContentInvalid() const {
  if (HasIntAttribute(ax::mojom::IntAttribute::kInvalidState) &&
      GetData().GetInvalidState() != ax::mojom::InvalidState::kFalse) {
    // We will not report content as invalid until a certain number of
    // characters have been typed to prevent verbose announcements to the user.
    return (GetSubstringTextContentUTF16(kMinimumCharacterCountForInvalid)
                .length() > kMinimumCharacterCountForInvalid);
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsDisabledDescendant() const {
  // Iterate over parents and see if any are disabled.
  BrowserAccessibilityAndroid* parent =
      static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());
  while (parent != nullptr) {
    if (!parent->IsEnabled()) {
      return true;
    }
    parent =
        static_cast<BrowserAccessibilityAndroid*>(parent->PlatformGetParent());
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsEnabled() const {
  switch (GetData().GetRestriction()) {
    case ax::mojom::Restriction::kNone:
      return true;
    case ax::mojom::Restriction::kReadOnly:
    case ax::mojom::Restriction::kDisabled:
      // On Android, both Disabled and ReadOnly are treated the same.
      // For both of them, we set AccessibilityNodeInfo.IsEnabled to false
      // and we don't expose certain actions like SET_VALUE and PASTE.
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return true;
}

bool BrowserAccessibilityAndroid::IsExpanded() const {
  return HasState(ax::mojom::State::kExpanded);
}

bool BrowserAccessibilityAndroid::IsFocusable() const {
  // If it's an iframe element, only mark it as focusable if the element has an
  // explicit name. Otherwise mark it as not focusable to avoid the user landing
  // on empty container elements in the tree.
  if (ui::IsIframe(GetRole()) ||
      (ui::IsPlatformDocument(GetRole()) && PlatformGetParent())) {
    return HasStringAttribute(ax::mojom::StringAttribute::kName);
  }

  return BrowserAccessibility::IsFocusable();
}

bool BrowserAccessibilityAndroid::IsFormDescendant() const {
  // Iterate over parents and see if any are a form.
  const BrowserAccessibility* parent = PlatformGetParent();
  while (parent != nullptr) {
    if (ui::IsForm(parent->GetRole())) {
      return true;
    }
    parent = parent->PlatformGetParent();
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsHeading() const {
  BrowserAccessibilityAndroid* parent =
      static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());
  if (parent && parent->IsHeading()) {
    return true;
  }

  return ui::IsHeading(GetRole());
}

bool BrowserAccessibilityAndroid::IsHierarchical() const {
  return (GetRole() == ax::mojom::Role::kTree || IsHierarchicalList());
}

bool BrowserAccessibilityAndroid::IsMultiLine() const {
  return HasState(ax::mojom::State::kMultiline);
}

bool BrowserAccessibilityAndroid::IsMultiselectable() const {
  return HasState(ax::mojom::State::kMultiselectable);
}

bool BrowserAccessibilityAndroid::IsRangeControlWithoutAriaValueText() const {
  return GetData().IsRangeValueSupported() &&
         !HasStringAttribute(ax::mojom::StringAttribute::kValue) &&
         HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
}

bool BrowserAccessibilityAndroid::IsReportingCheckable() const {
  // To communicate kMixed state Checkboxes, we will rely on state description,
  // so we will not report node as checkable to avoid duplicate utterances.
  return IsCheckable() &&
         GetData().GetCheckedState() != ax::mojom::CheckedState::kMixed;
}

bool BrowserAccessibilityAndroid::IsRequired() const {
  return HasState(ax::mojom::State::kRequired);
}

bool BrowserAccessibilityAndroid::IsScrollable() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kScrollable);
}

bool BrowserAccessibilityAndroid::IsSeekControl() const {
  // Range types should have seek control options, except progress bars.
  return GetData().IsRangeValueSupported() &&
         (GetRole() != ax::mojom::Role::kProgressIndicator);
}

bool BrowserAccessibilityAndroid::IsSelected() const {
  return GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

bool BrowserAccessibilityAndroid::IsSlider() const {
  return GetRole() == ax::mojom::Role::kSlider;
}

bool BrowserAccessibilityAndroid::IsTableHeader() const {
  return ui::IsTableHeader(GetRole());
}

bool BrowserAccessibilityAndroid::IsVisibleToUser() const {
  return !IsInvisibleOrIgnored();
}

bool BrowserAccessibilityAndroid::IsInterestingOnAndroid() const {
  // The root is not interesting if it doesn't have a title, even
  // though it's focusable.
  if (ui::IsPlatformDocument(GetRole()) &&
      GetSubstringTextContentUTF16(1).empty()) {
    return false;
  }

  // Mark as uninteresting if it's hidden, even if it is focusable.
  if (IsInvisibleOrIgnored()) {
    return false;
  }

  // Walk up the ancestry. A non-focusable child of a control is not
  // interesting. A child of an invisible iframe is also not interesting.
  // A link is never a leaf node so that its children can be navigated
  // when swiping by heading, landmark, etc. So we will also mark the
  // children of a link as not interesting to prevent double utterances.
  const BrowserAccessibility* parent = PlatformGetParent();
  while (parent) {
    if (ui::IsControl(parent->GetRole()) && !IsFocusable()) {
      return false;
    }

    if (parent->GetRole() == ax::mojom::Role::kIframe &&
        parent->IsInvisibleOrIgnored()) {
      return false;
    }

    if (parent->GetRole() == ax::mojom::Role::kLink) {
      return false;
    }

    parent = parent->PlatformGetParent();
  }

  // Otherwise, focusable nodes are always interesting. Note that IsFocusable()
  // already skips over things like iframes and child frames that are
  // technically focusable but shouldn't be exposed as focusable on Android.
  if (IsFocusable()) {
    return true;
  }

  // If it's not focusable but has a control role, then it's interesting.
  if (ui::IsControl(GetRole())) {
    return true;
  }

  // Mark progress indicators as interesting, since they are not focusable and
  // not a control, but users should be able to swipe/navigate to them.
  if (GetRole() == ax::mojom::Role::kProgressIndicator) {
    return true;
  }

  // If we are the direct descendant of a link and have no siblings/children,
  // then we are not interesting, return false
  parent = PlatformGetParent();
  if (parent != nullptr && ui::IsLink(parent->GetRole()) &&
      parent->PlatformChildCount() == 1 && PlatformChildCount() == 0) {
    return false;
  }

  // Otherwise, the interesting nodes are leaf nodes with non-whitespace text.
  return IsLeaf() && !base::ContainsOnlyChars(GetTextContentUTF16(),
                                              base::kWhitespaceUTF16);
}

bool BrowserAccessibilityAndroid::IsHeadingLink() const {
  if (!(GetRole() == ax::mojom::Role::kHeading && InternalChildCount() == 1)) {
    return false;
  }

  BrowserAccessibilityAndroid* child =
      static_cast<BrowserAccessibilityAndroid*>(InternalChildrenBegin().get());
  return ui::IsLink(child->GetRole());
}

const BrowserAccessibilityAndroid*
BrowserAccessibilityAndroid::GetSoleInterestingNodeFromSubtree() const {
  if (IsInterestingOnAndroid()) {
    return this;
  }

  const BrowserAccessibilityAndroid* sole_interesting_node = nullptr;
  for (const auto& child : PlatformChildren()) {
    const BrowserAccessibilityAndroid* interesting_node =
        static_cast<const BrowserAccessibilityAndroid&>(child)
            .GetSoleInterestingNodeFromSubtree();
    if (interesting_node && sole_interesting_node) {
      // If there are two interesting nodes, return nullptr.
      return nullptr;
    } else if (interesting_node) {
      sole_interesting_node = interesting_node;
    }
  }

  return sole_interesting_node;
}

bool BrowserAccessibilityAndroid::AreInlineTextBoxesLoaded() const {
  if (IsText()) {
    return InternalChildCount() > 0;
  }

  // Return false if any descendant needs to load inline text boxes.
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(it.get());
    if (!child->AreInlineTextBoxesLoaded()) {
      return false;
    }
  }

  // Otherwise return true - either they're all loaded, or there aren't
  // any descendants that need to load inline text boxes.
  return true;
}

int BrowserAccessibilityAndroid::ClickableScore() const {
  // For nodes that do not have the default action verb, return not clickable.
  if (!HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb)) {
    return kNotClickable;
  }

  switch (GetData().GetDefaultActionVerb()) {
    // Differentiate between nodes that are clickable because of an ancestor.
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return kHasClickAncestor;

    // For all other clickable nodes, check whether the node is also a control
    // on Android, and return score based on the result.
    case ax::mojom::DefaultActionVerb::kActivate:
    case ax::mojom::DefaultActionVerb::kCheck:
    case ax::mojom::DefaultActionVerb::kClick:
    case ax::mojom::DefaultActionVerb::kJump:
    case ax::mojom::DefaultActionVerb::kOpen:
    case ax::mojom::DefaultActionVerb::kPress:
    case ax::mojom::DefaultActionVerb::kSelect:
    case ax::mojom::DefaultActionVerb::kUncheck: {
      return ui::IsControlOnAndroid(GetRole(), IsFocusable())
                 ? kHasClickListenerAndIsControl
                 : kHasClickListener;
    }

    case ax::mojom::DefaultActionVerb::kNone:
    default:
      return kNotClickable;
  }
}

bool BrowserAccessibilityAndroid::CanOpenPopup() const {
  return HasIntAttribute(ax::mojom::IntAttribute::kHasPopup);
}

const char* BrowserAccessibilityAndroid::GetClassName() const {
  ax::mojom::Role role = GetRole();

  if (IsTextField()) {
    // On Android, contenteditable needs to be handled the same as any
    // other text field.
    role = ax::mojom::Role::kTextField;
  } else if (IsAndroidTextView()) {
    // On Android, we want to report some extra nodes as TextViews. For example,
    // a <div> that only contains text, or a <p> that only contains text.
    role = ax::mojom::Role::kStaticText;
  }

  return ui::AXRoleToAndroidClassName(role, PlatformGetParent() != nullptr);
}

bool BrowserAccessibilityAndroid::IsAndroidTextView() const {
  return ui::IsAndroidTextViewCandidate(GetRole()) && HasOnlyTextChildren();
}

bool BrowserAccessibilityAndroid::IsChildOfLeaf() const {
  BrowserAccessibility* ancestor = InternalGetParent();

  while (ancestor) {
    if (ancestor->IsLeaf()) {
      return true;
    }
    ancestor = ancestor->InternalGetParent();
  }

  return false;
}

bool BrowserAccessibilityAndroid::IsLeaf() const {
  if (base::Contains(g_leaf_map.Get(), this)) {
    return g_leaf_map.Get()[this];
  }

  if (BrowserAccessibility::IsLeaf()) {
    return true;
  }

  // Document roots (e.g. kRootWebArea and kPdfRoot), and iframes are always
  // allowed to contain children.
  if (ui::IsIframe(GetRole()) || ui::IsPlatformDocument(GetRole())) {
    return false;
  }

  // Button, date and time controls should not expose their children to Android
  // accessibility APIs.
  switch (GetRole()) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kInputTime:
      return true;
    default:
      break;
  }

  // Links are never leaves.
  if (ui::IsLink(GetRole())) {
    return false;
  }

  // For Android only, tab-panels and tab-lists are never leaves. We do this to
  // temporarily get around the gap for aria-labelledby in the Android API.
  // See b/241526393.
  if (GetRole() == ax::mojom::Role::kTabPanel ||
      GetRole() == ax::mojom::Role::kTabList) {
    return false;
  }

  // Focusable nodes with name from attribute should never drop children.
  if (HasState(ax::mojom::State::kFocusable) &&
      HasIntAttribute(ax::mojom::IntAttribute::kNameFrom) &&
      GetNameFrom() == ax::mojom::NameFrom::kAttribute) {
    // We exclude menuItems and comboBoxMenuButtons to prevent double utterance.
    if (GetRole() != ax::mojom::Role::kMenuItem &&
        GetRole() != ax::mojom::Role::kComboBoxMenuButton) {
      return false;
    }
  }

  BrowserAccessibilityManagerAndroid* manager_android =
      static_cast<BrowserAccessibilityManagerAndroid*>(manager());
  if (manager_android->prune_tree_for_screen_reader()) {
    // For some nodes, we will consider children before determining if the node
    // is a leaf. For nodes with relevant children, we will return false here
    // and allow the child nodes to be set as a leaf.

    // Headings with text can drop their children (with exceptions).
    std::u16string name = GetSubstringTextContentUTF16(1);
    if (GetRole() == ax::mojom::Role::kHeading && !name.empty()) {
      bool ret = IsLeafConsideringChildren();
      g_leaf_map.Get()[this] = ret;
      return ret;
    }

    // Focusable nodes with text can drop their children (with exceptions).
    if (HasState(ax::mojom::State::kFocusable) && !name.empty()) {
      bool ret = IsLeafConsideringChildren();
      g_leaf_map.Get()[this] = ret;
      return ret;
    }

    // Nodes with only static text can drop their children, with the exception
    // that list markers have a different role and should not be dropped.
    if (HasOnlyTextChildren() && !HasListMarkerChild()) {
      g_leaf_map.Get()[this] = true;
      return true;
    }
  }
  g_leaf_map.Get()[this] = false;
  return false;
}

bool BrowserAccessibilityAndroid::IsLeafConsideringChildren() const {
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!

  // Check for any children that should be exposed and return false if found (by
  // returning false we are saying the parent node is NOT a leaf and this child
  // node should instead be the leaf).
  //
  // If a node has a child that meets any of these criteria, it is NOT a leaf:
  //
  //   * child is focusable, and NOT a menu option
  //   * child is a table, cell, or row
  //
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();

    if (child->HasState(ax::mojom::State::kFocusable) &&
        child->GetRole() != ax::mojom::Role::kMenuListOption) {
      return false;
    }

    if (child->GetRole() == ax::mojom::Role::kTable ||
        child->GetRole() == ax::mojom::Role::kCell ||
        child->GetRole() == ax::mojom::Role::kGridCell ||
        child->GetRole() == ax::mojom::Role::kRow ||
        child->GetRole() == ax::mojom::Role::kLayoutTable ||
        child->GetRole() == ax::mojom::Role::kLayoutTableCell ||
        child->GetRole() == ax::mojom::Role::kLayoutTableRow) {
      return false;
    }

    // Check nested children and return false if any meet above criteria.
    if (!static_cast<BrowserAccessibilityAndroid*>(child)
             ->IsLeafConsideringChildren()) {
      return false;
    }
  }

  // If no such children were found, return true signaling the parent node can
  // be the leaf node.
  return true;
}

std::u16string BrowserAccessibilityAndroid::GetBrailleLabel() const {
  if (HasStringAttribute(ax::mojom::StringAttribute::kAriaBrailleLabel)) {
    return GetString16Attribute(ax::mojom::StringAttribute::kAriaBrailleLabel);
  }
  return std::u16string();
}

std::u16string BrowserAccessibilityAndroid::GetBrailleRoleDescription() const {
  if (HasStringAttribute(
          ax::mojom::StringAttribute::kAriaBrailleRoleDescription)) {
    return GetString16Attribute(
        ax::mojom::StringAttribute::kAriaBrailleRoleDescription);
  }
  return std::u16string();
}

std::u16string BrowserAccessibilityAndroid::GetTextContentUTF16() const {
  return GetSubstringTextContentUTF16(std::nullopt);
}

int BrowserAccessibilityAndroid::GetTextContentLengthUTF16() const {
  return GetTextContentUTF16().length();
}

std::u16string BrowserAccessibilityAndroid::GetSubstringTextContentUTF16(
    std::optional<size_t> min_length) const {
  if (ui::IsIframe(GetRole())) {
    return std::u16string();
  }

  // First, always return the |value| attribute if this is an
  // input field.
  std::u16string value = GetValueForControl();
  if (ShouldExposeValueAsName()) {
    return value;
  }

  // For color wells, the color is stored in separate attributes.
  // Perhaps we could return color names in the future?
  if (GetRole() == ax::mojom::Role::kColorWell) {
    unsigned int color = static_cast<unsigned int>(
        GetIntAttribute(ax::mojom::IntAttribute::kColorValue));
    return base::UTF8ToUTF16(skia::SkColorToHexString(color));
  }

  std::u16string text = GetNameAsString16();
  if (ui::IsRangeValueSupported(GetRole())) {
    // For controls that support range values such as sliders, when a non-empty
    // name is present (e.g. a label), append this to the value so both the
    // valuetext and label are included, rather than replacing the value.
    // If the value itself is empty on a progress indicator, then this would
    // suggest it is indeterminate, so add that keyword.
    if (value.empty() && GetRole() == ax::mojom::Role::kProgressIndicator) {
      value = GetLocalizedString(IDS_AX_INDETERMINATE_VALUE);
    }

    // To prevent extra commas, only add if the text is non-empty
    if (!text.empty() && !value.empty()) {
      text = base::JoinString({std::move(value), std::move(text)}, u", ");
    } else if (!value.empty()) {
      text = std::move(value);
    }
  } else if (text.empty()) {
    // When a node does not have a name (e.g. a label), use its value instead.
    text = std::move(value);
  }

  // For almost all focusable nodes we try to get text from contents, but for
  // the root node that's redundant and often way too verbose.
  if (ui::IsPlatformDocument(GetRole())) {
    return text;
  }

  // A role="separator" is a leaf, and cannot get name from contents, even if
  // author appends text children.
  if (GetRole() == ax::mojom::Role::kSplitter) {
    return text;
  }

  // Append image description strings to the text.
  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  if (manager->ShouldAllowImageDescriptions()) {
    auto status = GetData().GetImageAnnotationStatus();
    switch (status) {
      case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
      case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
        AppendTextToString(GetLocalizedStringForImageAnnotationStatus(status),
                           &text);
        break;

      case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
        AppendTextToString(
            GetString16Attribute(ax::mojom::StringAttribute::kImageAnnotation),
            &text);
        break;

      case ax::mojom::ImageAnnotationStatus::kNone:
      case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
      case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
        break;
    }
  }

  size_t text_length = text.size();
  std::vector<std::u16string> inner_text({std::move(text)});
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!
  if (text_length == 0 && ((HasOnlyTextChildren() && !HasListMarkerChild()) ||
                           (IsFocusable() && HasOnlyTextAndImageChildren()))) {
    for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
      std::u16string child_text =
          static_cast<BrowserAccessibilityAndroid*>(it.get())
              ->GetSubstringTextContentUTF16(min_length);
      text_length += child_text.size();
      inner_text.push_back(std::move(child_text));
      if (min_length && text_length >= *min_length) {
        break;
      }
    }
  }

  text = base::JoinString(inner_text, u"");

  if (text.empty() &&
      (ui::IsLink(GetRole()) || ui::IsImageOrVideo(GetRole())) &&
      !HasExplicitlyEmptyName()) {
    std::u16string url = GetString16Attribute(ax::mojom::StringAttribute::kUrl);
    text = ui::AXUrlBaseText(url);
  }

  return text;
}

BrowserAccessibilityAndroid::EarlyExitPredicate
BrowserAccessibilityAndroid::NonEmptyPredicate() {
  return base::BindRepeating(
      [](const std::u16string& partial) { return partial.size() > 0; });
}

BrowserAccessibilityAndroid::EarlyExitPredicate
BrowserAccessibilityAndroid::LengthAtLeast(size_t length) {
  return base::BindRepeating(
      [](size_t length, const std::u16string& partial) {
        return partial.length() > length;
      },
      length);
}

std::u16string BrowserAccessibilityAndroid::GetValueForControl() const {
  std::u16string value = BrowserAccessibility::GetValueForControl();

  // Optionally replace entered password text with bullet characters
  // based on a user preference.
  if (IsPasswordField()) {
    if (ui::AccessibilityState::ShouldRespectDisplayedPasswordText()) {
      // In the Chrome accessibility tree, the value of a password node is
      // unobscured. However, if ShouldRespectDisplayedPasswordText() returns
      // true we should try to expose whatever's actually visually displayed,
      // whether that's the actual password or dots or whatever. To do this
      // we rely on the password field's shadow dom.
      value = BrowserAccessibility::GetTextContentUTF16();
    } else if (!ui::AccessibilityState::ShouldExposePasswordText()) {
      value = std::u16string(value.size(), ui::kSecurePasswordBullet);
    }
  }

  return value;
}

std::u16string BrowserAccessibilityAndroid::GetHint() const {
  std::vector<std::u16string> strings;

  // If we're returning the value as the main text, the name needs to be
  // part of the hint.
  if (ShouldExposeValueAsName()) {
    std::u16string name = GetNameAsString16();
    if (!name.empty()) {
      strings.push_back(name);
    }
  }

  if (GetData().GetNameFrom() != ax::mojom::NameFrom::kPlaceholder) {
    std::u16string placeholder =
        GetString16Attribute(ax::mojom::StringAttribute::kPlaceholder);
    if (!placeholder.empty()) {
      strings.push_back(placeholder);
    }
  }

  std::u16string description =
      GetString16Attribute(ax::mojom::StringAttribute::kDescription);
  if (!description.empty()) {
    strings.push_back(description);
  }

  return base::JoinString(strings, u" ");
}

std::u16string BrowserAccessibilityAndroid::GetDialogModalMessageText() const {
  // For a dialog/modal, first check for a name, and then a description. If
  // both are empty, fallback to a default "dialog opened." text.
  if (HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    return GetString16Attribute(ax::mojom::StringAttribute::kName);
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    return GetString16Attribute(ax::mojom::StringAttribute::kDescription);
  }

  return GetLocalizedString(IDS_AX_DIALOG_MODAL_OPENED);
}

std::u16string BrowserAccessibilityAndroid::GetStateDescription() const {
  std::vector<std::u16string> state_descs;

  // For multiselectable state, generate a state description. We do not set a
  // state description for pop up/<select> to prevent double utterances.
  // TODO(crbug.com/40864556): Consider whether other combobox roles should be
  // accounted for.
  if (IsMultiselectable() && GetRole() != ax::mojom::Role::kPopUpButton &&
      GetRole() != ax::mojom::Role::kComboBoxSelect) {
    state_descs.push_back(GetMultiselectableStateDescription());
  }

  // For Checkboxes, if we are in a kMixed state, we will communicate
  // "partially checked" through the state description. This is mutually
  // exclusive with the on/off of toggle buttons below.
  if (IsCheckable() && !IsReportingCheckable()) {
    state_descs.push_back(GetCheckboxStateDescription());
  } else if (GetRole() == ax::mojom::Role::kToggleButton ||
             GetRole() == ax::mojom::Role::kSwitch) {
    // For Toggle buttons and switches, we will append "on"/"off" in the state
    // description.
    state_descs.push_back(GetToggleStateDescription());
  }

  // For radio buttons, we will communicate how many radio buttons are in the
  // group and which one is selected/checked (e.g. "in group, option x of y")
  if (GetRole() == ax::mojom::Role::kRadioButton) {
    state_descs.push_back(GetRadioButtonStateDescription());
  }

  // For nodes with non-trivial aria-current values, communicate state.
  if (HasAriaCurrent()) {
    state_descs.push_back(GetAriaCurrentStateDescription());
  }

  // For nodes of any type that are required, add this to the end of the state.
  if (IsRequired()) {
    state_descs.push_back(
        GetLocalizedString(IDS_AX_ARIA_REQUIRED_STATE_DESCRIPTION));
  }

  // Concatenate all state descriptions and return.
  return base::JoinString(state_descs, u" ");
}

std::u16string BrowserAccessibilityAndroid::GetMultiselectableStateDescription()
    const {
  // Count the number of children and selected children.
  int child_count = 0;
  int selected_count = 0;
  for (const auto& child : PlatformChildren()) {
    child_count++;
    const BrowserAccessibilityAndroid& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsSelected()) {
      selected_count++;
    }
  }

  // If none are selected, return special case.
  if (!selected_count) {
    return GetLocalizedString(IDS_AX_MULTISELECTABLE_STATE_DESCRIPTION_NONE);
  }

  // Generate a state description of the form: "multiselectable, x of y
  // selected.".
  std::vector<std::u16string> values;
  values.push_back(base::NumberToString16(selected_count));
  values.push_back(base::NumberToString16(child_count));
  return base::ReplaceStringPlaceholders(
      GetLocalizedString(IDS_AX_MULTISELECTABLE_STATE_DESCRIPTION), values,
      nullptr);
}

std::u16string BrowserAccessibilityAndroid::GetToggleStateDescription() const {
  // For checked Toggle buttons and switches, we will return "on", otherwise
  // "off".
  if (IsChecked()) {
    return GetLocalizedString(IDS_AX_TOGGLE_BUTTON_ON);
  }

  return GetLocalizedString(IDS_AX_TOGGLE_BUTTON_OFF);
}

std::u16string BrowserAccessibilityAndroid::GetCheckboxStateDescription()
    const {
  return GetLocalizedString(IDS_AX_CHECKBOX_PARTIALLY_CHECKED);
}

std::u16string BrowserAccessibilityAndroid::GetAriaCurrentStateDescription()
    const {
  int message_id;
  switch (static_cast<ax::mojom::AriaCurrentState>(
      GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState))) {
    case ax::mojom::AriaCurrentState::kPage:
      message_id = IDS_AX_ARIA_CURRENT_PAGE;
      break;
    case ax::mojom::AriaCurrentState::kStep:
      message_id = IDS_AX_ARIA_CURRENT_STEP;
      break;
    case ax::mojom::AriaCurrentState::kLocation:
      message_id = IDS_AX_ARIA_CURRENT_LOCATION;
      break;
    case ax::mojom::AriaCurrentState::kDate:
      message_id = IDS_AX_ARIA_CURRENT_DATE;
      break;
    case ax::mojom::AriaCurrentState::kTime:
      message_id = IDS_AX_ARIA_CURRENT_TIME;
      break;
    case ax::mojom::AriaCurrentState::kTrue:
    default:
      message_id = IDS_AX_ARIA_CURRENT_TRUE;
      break;
  }

  return GetLocalizedString(message_id);
}

std::u16string BrowserAccessibilityAndroid::GetRadioButtonStateDescription()
    const {
  // The radio button should have an IntListAttribute of kRadioGroupIds, with
  // a length of the total number of radio buttons in this group. Blink sets
  // these attributes for all nodes automatically, including for nodes of
  // <input type="radio"> which share a common name. If the list is empty,
  // escape with empty string.
  std::vector<ui::AXNodeID> group_ids =
      GetIntListAttribute(ax::mojom::IntListAttribute::kRadioGroupIds);

  if (group_ids.empty() || group_ids.size() == 1) {
    return std::u16string();
  }

  // Adding a stateDescription will override the 'checked' utterance in some
  // downstream services like TalkBack, so add it to state as well.
  int message_id = IsChecked()
                       ? IDS_AX_RADIO_BUTTON_STATE_DESCRIPTION_CHECKED
                       : IDS_AX_RADIO_BUTTON_STATE_DESCRIPTION_UNCHECKED;

  return base::ReplaceStringPlaceholders(
      GetLocalizedString(message_id),
      std::vector<std::u16string>({base::NumberToString16(GetItemIndex() + 1),
                                   base::NumberToString16(group_ids.size())}),
      /* offsets */ nullptr);
}

std::u16string BrowserAccessibilityAndroid::GetComboboxExpandedText() const {
  // We consider comboboxes of the form:
  //
  // <div role="combobox">
  //   <input type="text" aria-controls="options">
  //   <ul role="listbox" id="options">...</ul> (Can be outside <div>)
  // </div>
  //
  // Find child input node:
  const BrowserAccessibilityAndroid* input_node = nullptr;
  for (const auto& child : PlatformChildren()) {
    const BrowserAccessibilityAndroid& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsTextField()) {
      input_node = &android_child;
      break;
    }
  }

  // If we have not found a child input element, consider aria 1.0 spec:
  //
  // <input type="text" role="combobox" aria-owns="options">
  // <ul role="listbox" id="options">...</ul>
  //
  // Check if |this| is the input, otherwise try our fallbacks.
  if (!input_node) {
    if (IsTextField()) {
      input_node = this;
    } else {
      return GetComboboxExpandedTextFallback();
    }
  }

  // Get the aria-controls nodes of |input_node|.
  std::vector<BrowserAccessibility*> controls =
      manager()->GetAriaControls(input_node);

  // |input_node| should control only one element, if it doesn't, try fallbacks.
  if (controls.size() != 1) {
    return GetComboboxExpandedTextFallback();
  }

  // |controlled_node| needs to be a combobox container, if not, try fallbacks.
  BrowserAccessibilityAndroid* controlled_node =
      static_cast<BrowserAccessibilityAndroid*>(controls[0]);
  if (!ui::IsComboBoxContainer(controlled_node->GetRole())) {
    return GetComboboxExpandedTextFallback();
  }

  // For dialogs, return special case string.
  if (controlled_node->GetRole() == ax::mojom::Role::kDialog) {
    return GetLocalizedString(IDS_AX_COMBOBOX_EXPANDED_DIALOG);
  }

  // Find |controlled_node| set size, or return default string.
  if (!controlled_node->GetSetSize()) {
    return GetLocalizedString(IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_DEFAULT);
  }

  // Replace placeholder with count and return string.
  return base::ReplaceStringPlaceholders(
      GetLocalizedString(
          IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_X_OPTIONS_AVAILABLE),
      base::NumberToString16(*controlled_node->GetSetSize()), nullptr);
}

std::u16string BrowserAccessibilityAndroid::GetComboboxExpandedTextFallback()
    const {
  // If a combobox was of an indeterminate form, attempt any special cases here,
  // or return "expanded" as a final option.

  // Check for child nodes that are collections.
  int child_collection_count = 0;
  const BrowserAccessibilityAndroid* collection_node = nullptr;
  for (const auto& child : PlatformChildren()) {
    const auto& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsCollection()) {
      child_collection_count++;
      collection_node = &android_child;
    }
  }

  // If we find none, or more than one, we will not be able to determine the
  // correct utterance, so return a default string instead.
  if (child_collection_count != 1) {
    return GetLocalizedString(IDS_AX_COMBOBOX_EXPANDED);
  }

  // Find |collection_node| set size, or return defaul string.
  if (!collection_node->GetSetSize()) {
    return GetLocalizedString(IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_DEFAULT);
  }

  // Replace placeholder with count and return string.
  return base::ReplaceStringPlaceholders(
      GetLocalizedString(
          IDS_AX_COMBOBOX_EXPANDED_AUTOCOMPLETE_X_OPTIONS_AVAILABLE),
      base::NumberToString16(*collection_node->GetSetSize()), nullptr);
}

std::string BrowserAccessibilityAndroid::GetRoleString() const {
  return ui::ToString(GetRole());
}

std::u16string BrowserAccessibilityAndroid::GetRoleDescription() const {
  // If an element has an aria-roledescription set, use that value by default.
  if (HasStringAttribute(ax::mojom::StringAttribute::kRoleDescription)) {
    return GetString16Attribute(ax::mojom::StringAttribute::kRoleDescription);
  }

  // As a special case, if we have a heading level return a string like
  // "heading level 1", etc. - and if the heading consists of a link,
  // append the word link as well.
  if (GetRole() == ax::mojom::Role::kHeading) {
    std::vector<std::u16string> role_description;
    int level = GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    if (level >= 1 && level <= 6) {
      std::vector<std::u16string> values;
      values.push_back(base::NumberToString16(level));
      role_description.push_back(base::ReplaceStringPlaceholders(
          GetLocalizedString(IDS_AX_ROLE_HEADING_WITH_LEVEL), values, nullptr));
    } else {
      role_description.push_back(GetLocalizedString(IDS_AX_ROLE_HEADING));
    }

    if (IsHeadingLink()) {
      role_description.push_back(GetLocalizedString(IDS_AX_ROLE_LINK));
    }

    // For visited links, we additionally want to append "visited" to the
    // description.
    if (HasState(ax::mojom::State::kVisited)) {
      role_description.push_back(GetLocalizedString(IDS_AX_STATE_LINK_VISITED));
    }

    return base::JoinString(role_description, u" ");
  }

  // If this node is a link and the parent is a heading, return the role
  // description of the parent (e.g. "heading 1 link").
  if (ui::IsLink(GetRole()) && PlatformGetParent()) {
    BrowserAccessibilityAndroid* parent =
        static_cast<BrowserAccessibilityAndroid*>(PlatformGetParent());
    if (parent->IsHeadingLink()) {
      return parent->GetRoleDescription();
    }
  }

  // If this node is a link and visited, append "visited" to the description.
  if (ui::IsLink(GetRole())) {
    std::vector<std::u16string> role_description = {
        GetLocalizedStringForRoleDescription()};
    if (HasState(ax::mojom::State::kVisited)) {
      role_description.push_back(GetLocalizedString(IDS_AX_STATE_LINK_VISITED));
    }
    return base::JoinString(role_description, u" ");
  }

  // If this node is an image, check status and potentially add unlabeled role.
  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  if (manager->ShouldAllowImageDescriptions()) {
    auto status = GetData().GetImageAnnotationStatus();
    switch (status) {
      case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
      case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
        return GetLocalizedRoleDescriptionForUnlabeledImage();

      case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      case ax::mojom::ImageAnnotationStatus::kNone:
      case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
      case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
      case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
        break;
    }
  }

  // For buttons with a kHasPopup attribute, return a more specific role.
  if (ui::IsButton(GetRole())) {
    switch (static_cast<ax::mojom::HasPopup>(
        GetIntAttribute(ax::mojom::IntAttribute::kHasPopup))) {
      case ax::mojom::HasPopup::kTrue:
      case ax::mojom::HasPopup::kMenu:
        return GetLocalizedString(IDS_AX_ROLE_POP_UP_BUTTON_MENU);
      case ax::mojom::HasPopup::kDialog:
        return GetLocalizedString(IDS_AX_ROLE_POP_UP_BUTTON_DIALOG);
      case ax::mojom::HasPopup::kListbox:
      case ax::mojom::HasPopup::kTree:
      case ax::mojom::HasPopup::kGrid:
        return GetLocalizedString(IDS_AX_ROLE_POP_UP_BUTTON);
      case ax::mojom::HasPopup::kFalse:
        break;
    }
  }

  switch (GetRole()) {
    case ax::mojom::Role::kAudio:
    case ax::mojom::Role::kCode:
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kDetails:
    case ax::mojom::Role::kEmphasis:
    case ax::mojom::Role::kForm:
    case ax::mojom::Role::kRowGroup:
    case ax::mojom::Role::kSectionFooter:
    case ax::mojom::Role::kSectionHeader:
    case ax::mojom::Role::kSectionWithoutName:
    case ax::mojom::Role::kStrong:
    case ax::mojom::Role::kSubscript:
    case ax::mojom::Role::kSuperscript:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTime:
      // No role description on Android.
      break;
    case ax::mojom::Role::kFigure:
      // Default is IDS_AX_ROLE_FIGURE.
      return GetLocalizedString(IDS_AX_ROLE_GRAPHIC);
    case ax::mojom::Role::kHeader:
      // Default is IDS_AX_ROLE_HEADER.
      return GetLocalizedString(IDS_AX_ROLE_BANNER);
    case ax::mojom::Role::kListGrid:
      // Default is no special role description.
      return GetLocalizedString(IDS_AX_ROLE_TABLE);
    case ax::mojom::Role::kMenuItemCheckBox:
      // Default is no special role description.
      return GetLocalizedString(IDS_AX_ROLE_CHECK_BOX);
    case ax::mojom::Role::kMenuItemRadio:
      // Default is no special role description.
      return GetLocalizedString(IDS_AX_ROLE_RADIO);
    case ax::mojom::Role::kVideo:
      // Default is no special role description.
      return GetLocalizedString(IDS_AX_MEDIA_VIDEO_ELEMENT);
    default:
      return GetLocalizedStringForRoleDescription();
  }

  return std::u16string();
}

std::string BrowserAccessibilityAndroid::GetCSSDisplay() const {
  std::string display =
      node()->GetStringAttribute(ax::mojom::StringAttribute::kDisplay);

  // Since this method is used to determine whether a text node is inline or
  // block, we can filter out other values like list-item or table-cell
  if (display == "inline" || display == "block" || display == "inline-block") {
    return display;
  }
  return std::string();
}

int BrowserAccessibilityAndroid::GetItemIndex() const {
  int index = 0;
  if (IsRangeControlWithoutAriaValueText()) {
    // Return a percentage here for live feedback in an AccessibilityEvent.
    // The exact value is returned in RangeCurrentValue. Exclude sliders with
    // an aria-valuetext set, as a percentage is not meaningful in those cases.
    float min = GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
    float max = GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
    float value = GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    if (max > min && value >= min && value <= max) {
      index = static_cast<int>(((value - min)) * 100 / (max - min));
    }
  } else {
    std::optional<int> pos_in_set = GetPosInSet();
    if (pos_in_set && *pos_in_set > 0) {
      index = *pos_in_set - 1;
    }
  }
  return index;
}

int BrowserAccessibilityAndroid::GetItemCount() const {
  int count = 0;
  if (IsRangeControlWithoutAriaValueText()) {
    // An AccessibilityEvent can only return integer information about a
    // seek control, so we return a percentage. The real range is returned
    // in RangeMin and RangeMax. Exclude sliders with an aria-valuetext set,
    // as a percentage is not meaningful in those cases.
    count = 100;
  } else {
    if (IsCollection() && GetSetSize()) {
      count = *GetSetSize();
    }
  }
  return count;
}

int BrowserAccessibilityAndroid::GetSelectedItemCount() const {
  // Count the number of selected children.
  int selected_count = 0;
  for (const auto& child : PlatformChildren()) {
    const BrowserAccessibilityAndroid& android_child =
        static_cast<const BrowserAccessibilityAndroid&>(child);
    if (android_child.IsSelected()) {
      selected_count++;
    }
  }

  return selected_count;
}

bool BrowserAccessibilityAndroid::CanScrollForward() const {
  if (IsSlider()) {
    float value = GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    float max = GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
    return value < max;
  } else {
    return CanScrollRight() || CanScrollDown();
  }
}

bool BrowserAccessibilityAndroid::CanScrollBackward() const {
  if (IsSlider()) {
    float value = GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    float min = GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
    return value > min;
  } else {
    return CanScrollLeft() || CanScrollUp();
  }
}

bool BrowserAccessibilityAndroid::CanScrollUp() const {
  return GetScrollY() > GetMinScrollY() && IsScrollable();
}

bool BrowserAccessibilityAndroid::CanScrollDown() const {
  return GetScrollY() < GetMaxScrollY() && IsScrollable();
}

bool BrowserAccessibilityAndroid::CanScrollLeft() const {
  return GetScrollX() > GetMinScrollX() && IsScrollable();
}

bool BrowserAccessibilityAndroid::CanScrollRight() const {
  return GetScrollX() < GetMaxScrollX() && IsScrollable();
}

int BrowserAccessibilityAndroid::GetScrollX() const {
  int value = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kScrollX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetScrollY() const {
  int value = 0;
  GetIntAttribute(ax::mojom::IntAttribute::kScrollY, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMinScrollX() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
}

int BrowserAccessibilityAndroid::GetMinScrollY() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
}

int BrowserAccessibilityAndroid::GetMaxScrollX() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
}

int BrowserAccessibilityAndroid::GetMaxScrollY() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);
}

bool BrowserAccessibilityAndroid::Scroll(int direction,
                                         bool is_page_scroll) const {
  int x_initial = GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  int x_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin);
  int x_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax);
  int y_initial = GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
  int y_min = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin);
  int y_max = GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax);

  // Figure out the bounding box of the visible portion of this scrollable
  // view so we know how much to scroll by.
  gfx::Rect bounds;
  if (ui::IsPlatformDocument(GetRole()) && !PlatformGetParent()) {
    // If this is the root node, use the bounds of the view to determine how big
    // one page is.
    if (!manager()->delegate()) {
      return false;
    }
    bounds = manager()->delegate()->AccessibilityGetViewBounds();
  } else if (ui::IsPlatformDocument(GetRole()) && PlatformGetParent()) {
    // If this is a web area inside of an iframe, try to use the bounds of
    // the containing element.
    BrowserAccessibility* parent = PlatformGetParent();
    while (parent && (parent->GetClippedRootFrameBoundsRect().width() == 0 ||
                      parent->GetClippedRootFrameBoundsRect().height() == 0)) {
      parent = parent->PlatformGetParent();
    }
    if (parent) {
      bounds = parent->GetClippedRootFrameBoundsRect();
    } else {
      bounds = GetClippedRootFrameBoundsRect();
    }
  } else {
    // Otherwise this is something like a scrollable div, just use the
    // bounds of this object itself.
    bounds = GetClippedRootFrameBoundsRect();
  }

  // Scroll by 80% of one page, or 100% for page scrolls.
  int page_x, page_y;
  if (is_page_scroll) {
    page_x = std::max(bounds.width(), 1);
    page_y = std::max(bounds.height(), 1);
  } else {
    page_x = std::max(bounds.width() * 4 / 5, 1);
    page_y = std::max(bounds.height() * 4 / 5, 1);
  }

  if (direction == FORWARD) {
    direction = y_max > y_min ? DOWN : RIGHT;
  }
  if (direction == BACKWARD) {
    direction = y_max > y_min ? UP : LEFT;
  }

  int x = x_initial;
  int y = y_initial;
  switch (direction) {
    case UP:
      if (y_initial == y_min) {
        return false;
      }
      y = std::clamp(y_initial - page_y, y_min, y_max);
      break;
    case DOWN:
      if (y_initial == y_max) {
        return false;
      }
      y = std::clamp(y_initial + page_y, y_min, y_max);
      break;
    case LEFT:
      if (x_initial == x_min) {
        return false;
      }
      x = std::clamp(x_initial - page_x, x_min, x_max);
      break;
    case RIGHT:
      if (x_initial == x_max) {
        return false;
      }
      x = std::clamp(x_initial + page_x, x_min, x_max);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  manager()->SetScrollOffset(*this, gfx::Point(x, y));
  return true;
}

// Given arbitrary old_value_ and new_value_, we must come up with reasonable
// edit metrics. Although edits like "apple" > "apples" are typical, anything
// is possible, such as "apple" > "applesauce", "apple" > "boot", or "" >
// "supercalifragilisticexpialidocious". So we consider old_value_ to be of the
// form AXB and new_value_ to be of the form AYB, where X and Y are the pieces
// that don't match. We take the X to be the "removed" characters and Y to be
// the "added" characters.

int BrowserAccessibilityAndroid::GetTextChangeFromIndex() const {
  // This is len(A)
  return CommonPrefixLength(old_value_, new_value_);
}

int BrowserAccessibilityAndroid::GetTextChangeAddedCount() const {
  // This is len(AYB) - (len(A) + len(B)), or len(Y), the added characters.
  return new_value_.length() - CommonEndLengths(old_value_, new_value_);
}

int BrowserAccessibilityAndroid::GetTextChangeRemovedCount() const {
  // This is len(AXB) - (len(A) + len(B)), or len(X), the removed characters.
  return old_value_.length() - CommonEndLengths(old_value_, new_value_);
}

// static
size_t BrowserAccessibilityAndroid::CommonPrefixLength(
    const std::u16string& a,
    const std::u16string& b) {
  size_t a_len = a.length();
  size_t b_len = b.length();
  size_t i = 0;
  while (i < a_len && i < b_len && a[i] == b[i]) {
    i++;
  }
  return i;
}

// static
size_t BrowserAccessibilityAndroid::CommonSuffixLength(
    const std::u16string& a,
    const std::u16string& b) {
  size_t a_len = a.length();
  size_t b_len = b.length();
  size_t i = 0;
  while (i < a_len && i < b_len && a[a_len - i - 1] == b[b_len - i - 1]) {
    i++;
  }
  return i;
}

// TODO(nektar): Merge this function with
// |BrowserAccessibilityCocoa::computeTextEdit|.
//
// static
size_t BrowserAccessibilityAndroid::CommonEndLengths(const std::u16string& a,
                                                     const std::u16string& b) {
  size_t prefix_len = CommonPrefixLength(a, b);
  // Remove the matching prefix before finding the suffix. Otherwise, if
  // old_value_ is "a" and new_value_ is "aa", "a" will be double-counted as
  // both a prefix and a suffix of "aa".
  std::u16string a_body = a.substr(prefix_len, std::string::npos);
  std::u16string b_body = b.substr(prefix_len, std::string::npos);
  size_t suffix_len = CommonSuffixLength(a_body, b_body);
  return prefix_len + suffix_len;
}

std::u16string BrowserAccessibilityAndroid::GetTextChangeBeforeText() const {
  return old_value_;
}

int BrowserAccessibilityAndroid::GetSelectionStart() const {
  int sel_start = 0;
  if (IsAtomicTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart, &sel_start)) {
    return sel_start;
  }
  ui::AXSelection unignored_selection =
      manager()->ax_tree()->GetUnignoredSelection();
  int32_t anchor_id = unignored_selection.anchor_object_id;
  BrowserAccessibility* anchor_object = manager()->GetFromID(anchor_id);
  if (!anchor_object) {
    return 0;
  }

  AXPosition position = anchor_object->CreateTextPositionAt(
      unignored_selection.anchor_offset, unignored_selection.anchor_affinity);
  while (position->GetAnchor() && position->GetAnchor() != node()) {
    position = position->CreateParentPosition();
  }

  return !position->IsNullPosition() ? position->text_offset() : 0;
}

int BrowserAccessibilityAndroid::GetSelectionEnd() const {
  int sel_end = 0;
  if (IsAtomicTextField() &&
      GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &sel_end)) {
    return sel_end;
  }

  ui::AXSelection unignored_selection =
      manager()->ax_tree()->GetUnignoredSelection();
  int32_t focus_id = unignored_selection.focus_object_id;
  BrowserAccessibility* focus_object = manager()->GetFromID(focus_id);
  if (!focus_object) {
    return 0;
  }

  AXPosition position = focus_object->CreateTextPositionAt(
      unignored_selection.focus_offset, unignored_selection.focus_affinity);
  while (position->GetAnchor() && position->GetAnchor() != node()) {
    position = position->CreateParentPosition();
  }

  return !position->IsNullPosition() ? position->text_offset() : 0;
}

int BrowserAccessibilityAndroid::GetEditableTextLength() const {
  if (IsTextField()) {
    return static_cast<int>(GetValueForControl().size());
  }
  return 0;
}

int BrowserAccessibilityAndroid::AndroidInputType() const {
  if (!HasStringAttribute(ax::mojom::StringAttribute::kInputType)) {
    return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;
  }

  if (!node()->HasStringAttribute(ax::mojom::StringAttribute::kInputType)) {
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;
  }

  const std::string& type =
      node()->GetStringAttribute(ax::mojom::StringAttribute::kInputType);
  if (type.empty() || type == "text" || type == "search") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;
  } else if (type == "date") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  } else if (type == "datetime" || type == "datetime-local") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;
  } else if (type == "email") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL;
  } else if (type == "month") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  } else if (type == "number") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER;
  } else if (type == "password") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD;
  } else if (type == "tel") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_PHONE;
  } else if (type == "time") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME;
  } else if (type == "url") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI;
  } else if (type == "week") {
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;
  }
  return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;
}

int BrowserAccessibilityAndroid::AndroidLiveRegionType() const {
  std::string live =
      GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
  if (live == "polite") {
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE;
  } else if (live == "assertive") {
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE;
  }
  return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE;
}

int BrowserAccessibilityAndroid::AndroidRangeType() const {
  return ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT;
}

int BrowserAccessibilityAndroid::RowCount() const {
  if (!IsCollection()) {
    return 0;
  }

  if (GetSetSize()) {
    return *GetSetSize();
  }

  return node()->GetTableRowCount().value_or(0);
}

int BrowserAccessibilityAndroid::ColumnCount() const {
  if (!IsCollection()) {
    return 0;
  }

  // For <ol> and <ul> elements on Android (e.g. role kList), the AX
  // code will consider these 0 columns, but on Android they are 1.
  int ax_cols = node()->GetTableColCount().value_or(0);
  if (GetRole() == ax::mojom::Role::kList ||
      GetRole() == ax::mojom::Role::kListBox) {
    DCHECK_EQ(ax_cols, 0);
    ax_cols = 1;
  }

  return ax_cols;
}

int BrowserAccessibilityAndroid::RowIndex() const {
  std::optional<int> pos_in_set = GetPosInSet();
  if (pos_in_set && pos_in_set > 0) {
    return *pos_in_set - 1;
  }
  return node()->GetTableCellRowIndex().value_or(0);
}

int BrowserAccessibilityAndroid::RowSpan() const {
  // For <ol> and <ul> elements on Android (e.g. role kListItem), the AX
  // code will consider these 0 span, but on Android they are 1.
  int ax_row_span = node()->GetTableCellRowSpan().value_or(0);
  if (GetRole() == ax::mojom::Role::kListItem ||
      GetRole() == ax::mojom::Role::kListBoxOption) {
    DCHECK_EQ(ax_row_span, 0);
    ax_row_span = 1;
  }

  return ax_row_span;
}

int BrowserAccessibilityAndroid::ColumnIndex() const {
  return node()->GetTableCellColIndex().value_or(0);
}

int BrowserAccessibilityAndroid::ColumnSpan() const {
  // For <ol> and <ul> elements on Android (e.g. role kListItem), the AX
  // code will consider these 0 span, but on Android they are 1.
  int ax_col_span = node()->GetTableCellColSpan().value_or(0);
  if (GetRole() == ax::mojom::Role::kListItem ||
      GetRole() == ax::mojom::Role::kListBoxOption) {
    DCHECK_EQ(ax_col_span, 0);
    ax_col_span = 1;
  }

  return ax_col_span;
}

float BrowserAccessibilityAndroid::RangeMin() const {
  return GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
}

float BrowserAccessibilityAndroid::RangeMax() const {
  return GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
}

float BrowserAccessibilityAndroid::RangeCurrentValue() const {
  return GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
}

void BrowserAccessibilityAndroid::GetGranularityBoundaries(
    int granularity,
    std::vector<int32_t>* starts,
    std::vector<int32_t>* ends,
    int offset) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE:
      GetLineBoundaries(starts, ends, offset);
      break;
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
      GetWordBoundaries(starts, ends, offset);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void BrowserAccessibilityAndroid::GetLineBoundaries(
    std::vector<int32_t>* line_starts,
    std::vector<int32_t>* line_ends,
    int offset) {
  // If this node has no children, treat it as all one line.
  if (GetSubstringTextContentUTF16(1).size() > 0 && !InternalChildCount()) {
    line_starts->push_back(offset);
    line_ends->push_back(offset + GetTextContentUTF16().size());
  }

  // If this is a static text node, get the line boundaries from the
  // inline text boxes if possible.
  if (GetRole() == ax::mojom::Role::kStaticText) {
    int last_y = 0;
    bool is_first = true;
    for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
      BrowserAccessibilityAndroid* child =
          static_cast<BrowserAccessibilityAndroid*>(it.get());
      CHECK_EQ(ax::mojom::Role::kInlineTextBox, child->GetRole());
      // TODO(dmazzoni): replace this with a proper API to determine
      // if two inline text boxes are on the same line. http://crbug.com/421771
      int y = child->GetClippedRootFrameBoundsRect().y();
      if (is_first) {
        is_first = false;
        line_starts->push_back(offset);
      } else if (y != last_y) {
        line_ends->push_back(offset);
        line_starts->push_back(offset);
      }
      offset += child->GetTextContentUTF16().size();
      last_y = y;
    }
    line_ends->push_back(offset);
    return;
  }

  // Otherwise, call GetLineBoundaries recursively on the children.
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(it.get());
    child->GetLineBoundaries(line_starts, line_ends, offset);
    offset += child->GetTextContentUTF16().size();
  }
}

void BrowserAccessibilityAndroid::GetWordBoundaries(
    std::vector<int32_t>* word_starts,
    std::vector<int32_t>* word_ends,
    int offset) {
  if (GetRole() == ax::mojom::Role::kInlineTextBox) {
    const std::vector<int32_t>& starts =
        GetIntListAttribute(ax::mojom::IntListAttribute::kWordStarts);
    const std::vector<int32_t>& ends =
        GetIntListAttribute(ax::mojom::IntListAttribute::kWordEnds);
    for (size_t i = 0; i < starts.size(); ++i) {
      word_starts->push_back(offset + starts[i]);
      word_ends->push_back(offset + ends[i]);
    }
    return;
  }

  std::u16string concatenated_text;
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(it.get());
    concatenated_text += child->GetTextContentUTF16();
  }

  std::u16string text = GetTextContentUTF16();
  if (text.empty() || concatenated_text == text) {
    // Great - this node is just the concatenation of its children, so
    // we can get the word boundaries recursively.
    for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
      BrowserAccessibilityAndroid* child =
          static_cast<BrowserAccessibilityAndroid*>(it.get());
      child->GetWordBoundaries(word_starts, word_ends, offset);
      offset += child->GetTextContentUTF16().size();
    }
  } else {
    // This node has its own accessible text that doesn't match its
    // visible text - like alt text for an image or something with an
    // aria-label, so split the text into words locally.
    base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
    if (!iter.Init()) {
      return;
    }
    while (iter.Advance()) {
      if (iter.IsWord()) {
        word_starts->push_back(iter.prev());
        word_ends->push_back(iter.pos());
      }
    }
  }
}

std::u16string BrowserAccessibilityAndroid::GetTargetUrl() const {
  if (ui::IsImageOrVideo(GetRole()) || ui::IsLink(GetRole())) {
    return GetString16Attribute(ax::mojom::StringAttribute::kUrl);
  }

  return {};
}

void BrowserAccessibilityAndroid::GetSuggestions(
    std::vector<int>* suggestion_starts,
    std::vector<int>* suggestion_ends) const {
  DCHECK(suggestion_starts);
  DCHECK(suggestion_ends);

  if (!IsTextField()) {
    return;
  }

  // TODO(accessibility): using FindTextOnlyObjectsInRange or NextInTreeOrder
  // doesn't work because Android's IsLeaf
  // implementation deliberately excludes a lot of nodes. We need a version of
  // FindTextOnlyObjectsInRange and/or NextInTreeOrder that only walk
  // the internal tree.
  BrowserAccessibility* node = InternalGetFirstChild();
  int start_offset = 0;
  while (node && node != this) {
    if (node->IsText()) {
      const std::vector<int32_t>& marker_types =
          node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
      if (!marker_types.empty()) {
        const std::vector<int>& marker_starts = node->GetIntListAttribute(
            ax::mojom::IntListAttribute::kMarkerStarts);
        const std::vector<int>& marker_ends =
            node->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);

        for (size_t i = 0; i < marker_types.size(); ++i) {
          // On Android, both spelling errors and alternatives from dictation
          // are both encoded as suggestions.
          if (!(marker_types[i] &
                static_cast<int32_t>(ax::mojom::MarkerType::kSuggestion))) {
            continue;
          }

          int suggestion_start = start_offset + marker_starts[i];
          int suggestion_end = start_offset + marker_ends[i];
          suggestion_starts->push_back(suggestion_start);
          suggestion_ends->push_back(suggestion_end);
        }
      }
      start_offset += node->GetTextContentUTF16().length();
    }

    // Implementation of NextInTreeOrder, but walking the internal tree.
    if (node->InternalChildCount()) {
      node = node->InternalGetFirstChild();
    } else {
      while (node && node != this) {
        BrowserAccessibility* sibling = node->InternalGetNextSibling();
        if (sibling) {
          node = sibling;
          break;
        }

        node = node->InternalGetParent();
      }
    }
  }
}

bool BrowserAccessibilityAndroid::HasAriaCurrent() const {
  if (!HasIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState)) {
    return false;
  }

  auto current = static_cast<ax::mojom::AriaCurrentState>(
      GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState));

  return current != ax::mojom::AriaCurrentState::kNone &&
         current != ax::mojom::AriaCurrentState::kFalse;
}

bool BrowserAccessibilityAndroid::HasNonEmptyValue() const {
  return IsTextField() && !GetValueForControl().empty();
}

bool BrowserAccessibilityAndroid::HasCharacterLocations() const {
  if (GetRole() == ax::mojom::Role::kStaticText) {
    return true;
  }

  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    if (static_cast<BrowserAccessibilityAndroid*>(it.get())
            ->HasCharacterLocations()) {
      return true;
    }
  }
  return false;
}

bool BrowserAccessibilityAndroid::HasImage() const {
  if (ui::IsImageOrVideo(GetRole())) {
    return true;
  }

  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    if (static_cast<BrowserAccessibilityAndroid*>(it.get())->HasImage()) {
      return true;
    }
  }
  return false;
}

ui::BrowserAccessibility*
BrowserAccessibilityAndroid::PlatformGetLowestPlatformAncestor() const {
  ui::BrowserAccessibility* current_object =
      const_cast<BrowserAccessibilityAndroid*>(this);
  ui::BrowserAccessibility* lowest_unignored_node = current_object;
  if (lowest_unignored_node->IsIgnored()) {
    lowest_unignored_node = lowest_unignored_node->PlatformGetParent();
  }
  DCHECK(!lowest_unignored_node || !lowest_unignored_node->IsIgnored())
      << "`BrowserAccessibility::PlatformGetParent()` should return either an "
         "unignored object or nullptr.";

  // `highest_leaf_node` could be nullptr.
  ui::BrowserAccessibility* highest_leaf_node = lowest_unignored_node;
  // For the purposes of this method, a leaf node does not include leaves in the
  // internal accessibility tree, only in the platform exposed tree.
  for (ui::BrowserAccessibility* ancestor_node = lowest_unignored_node;
       ancestor_node; ancestor_node = ancestor_node->PlatformGetParent()) {
    if (ancestor_node->IsLeaf()) {
      highest_leaf_node = ancestor_node;
    }
  }
  if (highest_leaf_node) {
    return highest_leaf_node;
  }

  if (lowest_unignored_node) {
    return lowest_unignored_node;
  }
  return current_object;
}

bool BrowserAccessibilityAndroid::HasOnlyTextChildren() const {
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    if (!it->IsText()) {
      return false;
    }
  }
  return true;
}

bool BrowserAccessibilityAndroid::HasOnlyTextAndImageChildren() const {
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();
    if (!child->IsText() && !ui::IsImageOrVideo(child->GetRole())) {
      return false;
    }
  }
  return true;
}

bool BrowserAccessibilityAndroid::HasListMarkerChild() const {
  // This is called from IsLeaf, so don't call PlatformChildCount
  // from within this!
  for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd(); ++it) {
    if (it->GetRole() == ax::mojom::Role::kListMarker) {
      return true;
    }
  }
  return false;
}

bool BrowserAccessibilityAndroid::ShouldExposeValueAsName() const {
  switch (GetRole()) {
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kInputTime:
      return true;
    case ax::mojom::Role::kColorWell:
      return false;
    default:
      break;
  }

  if (GetData().IsRangeValueSupported()) {
    return false;
  }

  if (IsTextField()) {
    return true;
  }

  if (ui::IsComboBox(GetRole())) {
    return true;
  }

  if (GetRole() == ax::mojom::Role::kPopUpButton &&
      !GetValueForControl().empty()) {
    return true;
  }

  return false;
}

bool BrowserAccessibilityAndroid::CanFireEvents() const {
  return !IsChildOfLeaf();
}

void BrowserAccessibilityAndroid::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();

  if (IsTextField()) {
    std::u16string value = GetValueForControl();
    if (value != new_value_) {
      old_value_ = new_value_;
      new_value_ = value;
    }
  }

  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  manager->ClearNodeInfoCacheForGivenId(GetUniqueId());
}

int BrowserAccessibilityAndroid::CountChildrenWithRole(
    ax::mojom::Role role) const {
  int count = 0;
  for (const auto& child : PlatformChildren()) {
    if (child.GetRole() == role) {
      count++;
    }
  }
  return count;
}

std::u16string BrowserAccessibilityAndroid::GetContentInvalidErrorMessage()
    const {
  int message_id = -1;

  if (!IsContentInvalid()) {
    return std::u16string();
  }

  switch (GetData().GetInvalidState()) {
    case ax::mojom::InvalidState::kNone:
    case ax::mojom::InvalidState::kFalse:
      // No error message necessary
      break;

    case ax::mojom::InvalidState::kTrue:
      message_id = CONTENT_INVALID_TRUE;
      // Handle Grammar or Spelling errors.
      // TODO(accessibility): using FindTextOnlyObjectsInRange or
      // NextInTreeOrder doesn't work because Android's IsLeaf implementation
      // deliberately excludes a lot of nodes.
      for (auto it = InternalChildrenBegin(); it != InternalChildrenEnd();
           ++it) {
        BrowserAccessibilityAndroid* child =
            static_cast<BrowserAccessibilityAndroid*>(it.get());
        if (child->IsText()) {
          const std::vector<int32_t>& marker_types = child->GetIntListAttribute(
              ax::mojom::IntListAttribute::kMarkerTypes);

          for (int marker_type : marker_types) {
            if (marker_type &
                static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)) {
              message_id = CONTENT_INVALID_SPELLING;
              break;
            } else if (marker_type &
                       static_cast<int32_t>(ax::mojom::MarkerType::kGrammar)) {
              message_id = CONTENT_INVALID_GRAMMAR;
              break;
            }
          }
        }
      }
      break;
  }

  if (message_id != -1) {
    return GetLocalizedString(message_id);
  }

  return std::u16string();
}

std::u16string
BrowserAccessibilityAndroid::GenerateAccessibilityNodeInfoString() const {
  auto* manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(this->manager());
  return manager->GenerateAccessibilityNodeInfoString(GetUniqueId());
}

}  // namespace content
