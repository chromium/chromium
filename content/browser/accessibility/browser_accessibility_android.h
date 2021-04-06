// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "ui/accessibility/platform/ax_platform_node.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityAndroid : public BrowserAccessibility {
 public:
  static BrowserAccessibilityAndroid* GetFromUniqueId(int32_t unique_id);
  int32_t unique_id() const { return GetUniqueId().Get(); }

  // BrowserAccessibility Overrides.
  void OnDataChanged() override;
  void OnLocationChanged() override;
  std::u16string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;

  bool IsCheckable() const;
  bool IsChecked() const;
  bool IsClickable() const override;
  bool IsCollapsed() const;
  bool IsCollection() const;
  bool IsCollectionItem() const;
  bool IsCombobox() const;
  bool IsComboboxControl() const;
  bool IsContentInvalid() const;
  bool IsDisabledDescendant() const;
  bool IsDismissable() const;
  bool IsEnabled() const;
  bool IsExpanded() const;
  bool IsFocusable() const;
  bool IsFormDescendant() const;
  bool IsHeading() const;
  bool IsHierarchical() const;
  bool IsLink() const;
  bool IsMultiLine() const;
  bool IsMultiselectable() const;
  bool IsReportingCheckable() const;
  bool IsScrollable() const;
  bool IsSeekControl() const;
  bool IsSelected() const;
  bool IsSlider() const;
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

  bool CanOpenPopup() const;

  bool HasAriaCurrent() const;

  bool HasNonEmptyValue() const;

  bool HasCharacterLocations() const;
  bool HasImage() const;

  const char* GetClassName() const;
  bool IsChildOfLeaf() const override;
  bool IsLeaf() const override;
  bool IsLeafConsideringChildren() const;
  std::u16string GetInnerText() const override;
  std::u16string GetValueForControl() const override;
  std::u16string GetHint() const;

  std::string GetRoleString() const;

  std::u16string GetDialogModalMessageText() const;

  std::u16string GetContentInvalidErrorMessage() const;

  std::u16string GetStateDescription() const;
  std::u16string GetMultiselectableStateDescription() const;
  std::u16string GetToggleButtonStateDescription() const;
  std::u16string GetCheckboxStateDescription() const;
  std::u16string GetListBoxStateDescription() const;
  std::u16string GetListBoxItemStateDescription() const;
  std::u16string GetAriaCurrentStateDescription() const;

  std::u16string GetComboboxExpandedText() const;
  std::u16string GetComboboxExpandedTextFallback() const;

  std::u16string GetRoleDescription() const;

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
  // (as returned by GetInnerText()), adding |offset| to each one.
  void GetLineBoundaries(std::vector<int32_t>* line_starts,
                         std::vector<int32_t>* line_ends,
                         int offset);

  // Append word start and end indices for the text of this node
  // (as returned by GetInnerText()) to |word_starts| and |word_ends|,
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

 private:
  // This gives BrowserAccessibility::Create access to the class constructor.
  friend class BrowserAccessibility;

  static size_t CommonPrefixLength(const std::u16string a,
                                   const std::u16string b);
  static size_t CommonSuffixLength(const std::u16string a,
                                   const std::u16string b);
  static size_t CommonEndLengths(const std::u16string a,
                                 const std::u16string b);

  BrowserAccessibilityAndroid();
  ~BrowserAccessibilityAndroid() override;

  // BrowserAccessibility overrides.
  BrowserAccessibility* PlatformGetLowestPlatformAncestor() const override;

  bool HasOnlyTextChildren() const;
  bool HasOnlyTextAndImageChildren() const;
  bool ShouldExposeValueAsName() const;
  int CountChildrenWithRole(ax::mojom::Role role) const;

  void AppendTextToString(std::u16string extra_text,
                          std::u16string* string) const;

  std::u16string cached_text_;
  std::u16string old_value_;
  std::u16string new_value_;
  int32_t unique_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
