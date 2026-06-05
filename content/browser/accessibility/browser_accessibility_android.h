// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/browser_accessibility.h"

namespace content {

struct AXStyleData;

class CONTENT_EXPORT BrowserAccessibilityAndroid
    : public ui::BrowserAccessibility {
 public:
  static BrowserAccessibilityAndroid* GetFromUniqueId(int32_t unique_id);
  static void ResetLeafCache();

  BrowserAccessibilityAndroid(const BrowserAccessibilityAndroid&) = delete;
  BrowserAccessibilityAndroid& operator=(const BrowserAccessibilityAndroid&) =
      delete;

  ~BrowserAccessibilityAndroid() override;

  // BrowserAccessibility Overrides.
  using BrowserAccessibility::GetUniqueId;
  bool CanFireEvents() const override;
  void OnDataChanged() override;
  void OnLocationChanged() override;
  BrowserAccessibility* PlatformGetLowestPlatformAncestor() const override;
  std::u16string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;

  bool IsAndroidTextView() const;
  bool IsCheckable() const;
  bool IsChecked() const;
  bool IsClickable() const override;
  bool IsCollapsed() const;

  // Android uses the term "collection" instead of "table". These methods are
  // pass-through methods to the ax_role_properties IsTableLikeOnAndroid and
  // IsTableItem. For example, a kList will return true for IsCollection and
  // false for IsCollectionItem, whereas a kListItem will return the opposite.
  bool IsCollection() const;
  bool IsCollectionItem() const;

  bool IsContentInvalid() const;
  bool IsDisabledDescendant() const;
  bool IsEnabled() const;
  bool IsEditable() const;
  bool IsExpanded() const;
  bool IsFocusable() const override;
  bool IsFormDescendant() const;
  bool IsHeading() const;
  bool IsHierarchical() const;
  bool IsMultiLine() const;
  bool IsMultiselectable() const;
  bool IsRangeControlWithoutAriaValueText() const;
  bool IsReportingCheckable() const;
  bool IsRequired() const;
  bool IsScrollable() const;
  bool IsSeekControl() const;
  bool IsSelected() const;
  bool IsSlider() const;
  bool IsSubscript() const;
  bool IsSuperscript() const;
  bool IsTableHeader() const;
  bool IsTextSelectable() const;
  bool IsVisibleToUser() const;
  bool ShouldUsePaneTitle() const;

  // This returns true for all nodes that we should navigate to.
  // Nodes that have a generic role, no accessible name, and aren't
  // focusable or clickable aren't interesting.
  bool IsInterestingOnAndroid() const;

  // If it's a heading whose only child is a link, or a heading that is inside
  // a link, returns the link node if it exists; otherwise nullptr.
  BrowserAccessibilityAndroid* GetHeadingLinkOrLinkHeading() const;

  // If this node is interesting (IsInterestingOnAndroid() returns true),
  // returns |this|. If not, it recursively checks all of the
  // platform children of this node, and if just a single one is
  // interesting, returns that one. If no descendants are interesting, or
  // if more than one is interesting, returns nullptr.
  const BrowserAccessibilityAndroid* GetSoleInterestingNodeFromSubtree() const;

  // Returns true if the given subtree has inline text box data, or if there
  // aren't any to load.
  bool AreInlineTextBoxesLoaded() const;

  // Returns a relative score of how likely a node is to be clickable.
  int ClickableScore() const;

  int ExpandedState() const;

  bool CanOpenPopup() const;

  bool HasAriaCurrent() const;

  bool HasNonEmptyValue() const;

  bool HasCharacterLocations() const;
  bool HasImage() const;
  bool HasLayoutBasedActions() const;

  const char* GetClassName() const;
  bool IsChildOfLeaf() const override;
  bool IsLeaf() const override;

  std::u16string GetBrailleLabel() const;
  std::u16string GetBrailleRoleDescription() const;

  // Note: In the Android accessibility API, the word "text" is used where other
  // platforms would use "name". The value returned here will appear in dump
  // tree tests as "name" in the ...-android.txt files, but as "text" in the
  // ...-android-external.txt files. On other platforms this may be ::GetName().
  std::u16string GetTextContentUTF16() const override;
  std::u16string GetValueForControl() const override;
  int GetTextContentLengthUTF16() const override;

  // --- Android Property Mappings ---
  // These methods map directly to properties in the Android
  // AccessibilityNodeInfo API. They represent the different "slots" where text
  // content can be placed.

  // Returns the text content that should be placed in the Android "text"
  // property. This is often the "deep" text content from the node and its
  // subtree.
  std::u16string GetAndroidText() const;

  // Returns the text content for the Android "contentDescription" property.
  std::u16string GetAndroidContentDescription() const;

  // Returns the text content for the Android "hintText" property.
  std::u16string GetAndroidHint() const;

  // Returns the text content for the Android "stateDescription" property.
  std::u16string GetAndroidStateDescription() const;

  // Returns the text content for the Android "containerTitle" property.
  std::u16string GetAndroidContainerTitle() const;

  // Returns the text content for the Android "supplementalDescription"
  // property.
  std::u16string GetAndroidSupplementalDescription() const;

  // Returns the text content for the Android "paneTitle" property.
  std::u16string GetAndroidPaneTitle() const;

  // Returns the text content for the Android "tooltipText" property.
  std::u16string GetAndroidTooltipText() const;

  // Returns the localized role description (e.g. "heading level 1").
  std::u16string GetAndroidRoleDescription() const;

  // Returns the error message for invalid content.
  std::u16string GetAndroidContentInvalidErrorMessage() const;

  // --- End of Android Property Mappings ---

  typedef base::RepeatingCallback<bool(const std::u16string& partial)>
      EarlyExitPredicate;
  // Gets the text content of this node, up to at least `min_length` if given.
  // If `style_data` is provided, it's populated with styling information.
  std::u16string GetSubstringTextContentUTF16(
      std::optional<size_t> min_length,
      AXStyleData* style_data = nullptr) const;
  static EarlyExitPredicate NonEmptyPredicate();
  static EarlyExitPredicate LengthAtLeast(size_t length);

  std::string GetRoleString() const;

  std::u16string GetComboboxExpandedText() const;
  std::u16string GetComboboxExpandedTextFallback() const;

  std::u16string GetMultiselectableStateDescription() const;
  std::u16string GetToggleStateDescription() const;
  std::u16string GetCheckboxStateDescription() const;
  std::u16string GetAriaCurrentStateDescription() const;
  std::u16string GetRadioButtonStateDescription() const;

  std::string GetCSSDisplay() const;

  // --- Styling Methods ---
  float GetTextSize() const;
  int GetTextStyle() const;
  int GetTextPosition() const;
  int GetTextColor() const;
  int GetTextBackgroundColor() const;
  std::string GetFontFamily() const;

  std::optional<int> GetItemIndex() const;
  std::optional<int> GetItemCount() const;
  int GetSelectedItemCount() const;
  int GetSelectionMode() const;

  // --- Scrolling Methods ---
  bool CanScrollForward() const;
  bool CanScrollBackward() const;
  bool CanScrollUp() const;
  bool CanScrollDown() const;
  bool CanScrollLeft() const;
  bool CanScrollRight() const;
  int GetScrollX() const;
  int GetScrollY() const;
  int GetMinScrollX() const;
  int GetMinScrollY() const;
  int GetMaxScrollX() const;
  int GetMaxScrollY() const;
  bool Scroll(int direction, bool is_page_scroll) const;

  int GetChecked() const;

  int GetTextChangeFromIndex() const;
  int GetTextChangeAddedCount() const;
  int GetTextChangeRemovedCount() const;
  std::u16string GetTextChangeBeforeText() const;

  // Returns ui::kAXAndroidUndefinedSelectionIndex if no selection.
  int GetSelectionStart() const;
  // Returns ui::kAXAndroidUndefinedSelectionIndex if no selection.
  int GetSelectionEnd() const;
  int GetEditableTextLength() const;

  int AndroidInputType() const;
  int AndroidLiveRegionType() const;
  int AndroidRangeType() const;

  std::optional<int> RowCount() const;
  std::optional<int> ColumnCount() const;

  std::optional<int> RowIndex() const;
  std::optional<int> RowSpan() const;
  std::optional<int> ColumnIndex() const;
  std::optional<int> ColumnSpan() const;

  // These are enums from
  // android.view.accessibility.AccessibilityNodeInfo.CollectionItemInfo in
  // Java:
  enum AndroidSortDirection {
    ANDROID_SORT_DIRECTION_NONE = 0,
    ANDROID_SORT_DIRECTION_ASCENDING = 1,
    ANDROID_SORT_DIRECTION_DESCENDING = 2,
    ANDROID_SORT_DIRECTION_OTHER = 3
  };

  // This method converts from ax::mojom::IntAttribute::kSortDirection to
  // android values. If this node is not a table header, it will return
  // ANDROID_SORT_DIRECTION_NONE as Android only can set the sort direction on
  // this kind of node.
  AndroidSortDirection GetSortDirection() const;

  float RangeMin() const;
  float RangeMax() const;
  float RangeCurrentValue() const;

  // Calls GetLineBoundaries or GetWordBoundaries depending on the value
  // of |granularity|, or fails if anything else is passed in |granularity|.
  void GetGranularityBoundaries(int granularity,
                                std::vector<int32_t>* starts,
                                std::vector<int32_t>* ends,
                                int offset);

  // Enumerates all possible mappings of ax::mojom::StringAttribute::kName to
  // Android accessibility properties.
  enum class AndroidNameTo {
    kUnset = 0,
    kContainerTitle,
    kContentDescription,
    kLabeledBy,
    kSupplementalDescription,
    kText,
  };

  // Append line start and end indices for the text of this node
  // (as returned by GetTextContentUTF16()), adding |offset| to each one.
  void GetLineBoundaries(std::vector<int32_t>* line_starts,
                         std::vector<int32_t>* line_ends,
                         int offset);

  // Append word start and end indices for the text of this node
  // (as returned by GetTextContentUTF16()) to |word_starts| and |word_ends|,
  // adding |offset| to each one.
  void GetWordBoundaries(std::vector<int32_t>* word_starts,
                         std::vector<int32_t>* word_ends,
                         int offset);

  // Return the target of a link or the source of an image.
  std::u16string GetTargetUrl() const;

  // On Android, spelling errors are returned as "suggestions". Retreive
  // all of the suggestions for a given text field as vectors of start
  // and end offsets.
  void GetSuggestions(std::vector<int>* suggestion_starts,
                      std::vector<int>* suggestion_ends) const;

  // Used for tree dumps, generate a string representation of the
  // AccessibilityNodeInfo object for this node by calling through the
  // manager to the web_contents_accessibility_android JNI.
  std::u16string GenerateAccessibilityNodeInfoString() const;

  // Used to determine paint order to see in what order nodes are drawn.
  // Used by Android XR.
  int GetPaintOrder() const;

  // Returns a list of Android IDs that were set on the node using
  // aria-labelledby.
  const std::vector<int> GetLabelledByAndroidIds() const;

  void EraseLeafCacheDataForNode();

 protected:
  BrowserAccessibilityAndroid(ui::BrowserAccessibilityManager* manager,
                              ui::AXNode* node);

  std::u16string GetLocalizedString(int message_id) const override;

  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  static size_t CommonPrefixLength(const std::u16string& a,
                                   const std::u16string& b);
  static size_t CommonSuffixLength(const std::u16string& a,
                                   const std::u16string& b);
  static size_t CommonEndLengths(const std::u16string& a,
                                 const std::u16string& b);

  // Computes whether there is an Android-specific reason that the node is a
  // leaf. The computed value is cached in GetLeafMap() and is used in the
  // IsLeaf() computation.
  bool ComputeIsLeaf() const;

  bool IsLeafConsideringChildren() const;
  bool HasFocusableChild() const;

  bool HasOnlyTextChildren() const;
  bool HasOnlyTextAndImageChildren() const;
  bool HasListMarkerChild() const;

  // Central dispatcher that returns the accessible name if it is mapped to the
  // given target, otherwise returns an empty string.
  std::u16string GetAccessibleNameForTarget(AndroidNameTo target) const;

  // This method determines if a node should promote its value to the "text"
  // property. When the value is promoted, the accessible name is demoted to
  // the "hint" property instead.
  bool ShouldPromoteValueToTextProperty(const std::u16string& value) const;

  int CountChildrenWithRole(ax::mojom::Role role) const;

  void AppendTextToString(std::u16string extra_text,
                          std::u16string* string) const;

  // Returns true if the node has int attribute of kDefaultActionVerb and the
  // default action verb is kSelect.
  bool HasSelectActionVerb() const;

  // Returns tree if any child has kSelect action verb.
  bool HasSelectActionVerbChildren() const;

  // Helper function that accumulates the text content for the node by
  // recursively appending text from its subtree.
  void AppendSubtreeTextRecursive(std::u16string* accumulated_text,
                                  std::optional<size_t> min_length,
                                  AXStyleData* style_data) const;

  // This method determines if a node should expose its editable value.
  bool ShouldExposeEditableValue() const;

  // Computes the name-to-property mapping on Android.
  AndroidNameTo ComputeAndroidNameTo() const;

  // Get image description string.
  std::u16string GetImageAnnotationText() const;

  std::u16string old_value_;
  std::u16string new_value_;

  // A cached value for the result of `ComputeAndroidNameTo`.
  mutable std::optional<AndroidNameTo> name_to_cache_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
