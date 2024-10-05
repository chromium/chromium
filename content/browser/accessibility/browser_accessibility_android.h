// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/browser_accessibility.h"

namespace content {

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
  bool IsTableHeader() const;
  bool IsVisibleToUser() const;

  // This returns true for all nodes that we should navigate to.
  // Nodes that have a generic role, no accessible name, and aren't
  // focusable or clickable aren't interesting.
  bool IsInterestingOnAndroid() const;

  // Is a heading whose only child is a link.
  bool IsHeadingLink() const;

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

  bool CanOpenPopup() const;

  bool HasAriaCurrent() const;

  bool HasNonEmptyValue() const;

  bool HasCharacterLocations() const;
  bool HasImage() const;

  const char* GetClassName() const;
  bool IsChildOfLeaf() const override;
  bool IsLeaf() const override;
  bool IsLeafConsideringChildren() const;

  std::u16string GetBrailleLabel() const;
  std::u16string GetBrailleRoleDescription() const;

  // Note: In the Android accessibility API, the word "text" is used where other
  // platforms would use "name". The value returned here will appear in dump
  // tree tests as "name" in the ...-android.txt files, but as "text" in the
  // ...-android-external.txt files. On other platforms this may be ::GetName().
  std::u16string GetTextContentUTF16() const override;
  std::u16string GetValueForControl() const override;
  int GetTextContentLengthUTF16() const override;

  typedef base::RepeatingCallback<bool(const std::u16string& partial)>
      EarlyExitPredicate;
  std::u16string GetSubstringTextContentUTF16(
      std::optional<size_t> min_length) const;
  static EarlyExitPredicate NonEmptyPredicate();
  static EarlyExitPredicate LengthAtLeast(size_t length);

  // This method maps to the Android API's "hint" attribute. For nodes that have
  // chosen to expose their value in the name ("text") attribute, the hint must
  // contain the text that would otherwise have been present. The hint includes
  // the placeholder and describedby values for all nodes regardless of where
  // the value is placed. These pieces of content are concatenated for Android.
  std::u16string GetHint() const;

  std::string GetRoleString() const;

  std::u16string GetDialogModalMessageText() const;

  std::u16string GetContentInvalidErrorMessage() const;

  std::u16string GetStateDescription() const;
  std::u16string GetMultiselectableStateDescription() const;
  std::u16string GetToggleStateDescription() const;
  std::u16string GetCheckboxStateDescription() const;
  std::u16string GetAriaCurrentStateDescription() const;
  std::u16string GetRadioButtonStateDescription() const;

  std::u16string GetComboboxExpandedText() const;
  std::u16string GetComboboxExpandedTextFallback() const;

  std::u16string GetRoleDescription() const;

  std::string GetCSSDisplay() const;

  int GetItemIndex() const;
  int GetItemCount() const;
  int GetSelectedItemCount() const;

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

  int GetTextChangeFromIndex() const;
  int GetTextChangeAddedCount() const;
  int GetTextChangeRemovedCount() const;
  std::u16string GetTextChangeBeforeText() const;

  int GetSelectionStart() const;
  int GetSelectionEnd() const;
  int GetEditableTextLength() const;

  int AndroidInputType() const;
  int AndroidLiveRegionType() const;
  int AndroidRangeType() const;

  int RowCount() const;
  int ColumnCount() const;

  int RowIndex() const;
  int RowSpan() const;
  int ColumnIndex() const;
  int ColumnSpan() const;

  float RangeMin() const;
  float RangeMax() const;
  float RangeCurrentValue() const;

  // Calls GetLineBoundaries or GetWordBoundaries depending on the value
  // of |granularity|, or fails if anything else is passed in |granularity|.
  void GetGranularityBoundaries(int granularity,
                                std::vector<int32_t>* starts,
                                std::vector<int32_t>* ends,
                                int offset);

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

  // BrowserAccessibility overrides.
  BrowserAccessibility* PlatformGetLowestPlatformAncestor() const override;

  bool HasOnlyTextChildren() const;
  bool HasOnlyTextAndImageChildren() const;
  bool HasListMarkerChild() const;

  // This method determines if a node should expose its value as a name, which
  // is placed in the Android API's "text" attribute. For controls that can take
  // on a value (e.g. a date time, or combobox), we wish to expose the value
  // that the user has chosen. When the value is exposed as the name, then the
  // accessible name is added to the Android API's "hint" attribute instead.
  bool ShouldExposeValueAsName() const;

  int CountChildrenWithRole(ax::mojom::Role role) const;

  void AppendTextToString(std::u16string extra_text,
                          std::u16string* string) const;

  std::u16string cached_text_;
  std::u16string old_value_;
  std::u16string new_value_;
  int32_t unique_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
