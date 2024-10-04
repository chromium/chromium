// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_view.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_suggestions_delegate.h"

class GURL;
class OmniboxClient;
struct AutocompleteMatch;
@class OmniboxTextFieldIOS;
@protocol OmniboxCommands;
@protocol ToolbarCommands;
@protocol OmniboxFocusDelegate;
@protocol OmniboxViewConsumer;

// iOS implementation of OmniBoxView.  Wraps a UITextField and
// interfaces with the rest of the autocomplete system.
class OmniboxViewIOS : public OmniboxView,
                       public OmniboxPopupViewSuggestionsDelegate,
                       public OmniboxTextChangeDelegate,
                       public OmniboxTextAcceptDelegate {
 public:
  // Retains `field`.
  OmniboxViewIOS(OmniboxTextFieldIOS* field,
                 std::unique_ptr<OmniboxClient> client,
                 ProfileIOS* profile,
                 id<OmniboxCommands> omnibox_focuser,
                 id<OmniboxFocusDelegate> focus_delegate,
                 id<ToolbarCommands> toolbar_commands_handler,
                 id<OmniboxViewConsumer> consumer,
                 bool is_lens_overlay);

  ~OmniboxViewIOS() override;

  void SetPopupProvider(OmniboxPopupProvider* provider) {
    popup_provider_ = provider;
  }

  void OnReceiveClipboardURLForOpenMatch(
      const AutocompleteMatch& match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      std::optional<GURL> optional_gurl);

  void OnReceiveClipboardTextForOpenMatch(
      const AutocompleteMatch& match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      std::optional<std::u16string> optional_text);

  void OnReceiveClipboardImageForOpenMatch(
      const AutocompleteMatch& match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      std::optional<gfx::Image> optional_image);

  void OnReceiveImageMatchForOpenMatch(
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t selected_line,
      base::TimeTicks match_selection_timestamp,
      std::optional<AutocompleteMatch> optional_match);

  /// Sets the image used in image search.
  void SetThumbnailImage(UIImage* image);

  // OmniboxView implementation.
  std::u16string GetText() const override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  void RevertAll() override;
  void UpdatePopup() override;
  void OnTemporaryTextMaybeChanged(const std::u16string& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& display_text,
      std::vector<gfx::Range> selections,
      const std::u16string& prefix_autocompletion,
      const std::u16string& inline_autocompletion) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  bool IsImeComposing() const override;
  bool IsIndicatingQueryRefinement() const override;
  void SetAdditionalText(const std::u16string& text) override;

  // OmniboxView stubs.
  void Update() override {}
  void EnterKeywordModeForDefaultSearchProvider() override {}
  bool IsSelectAll() const override;
  void GetSelectionBounds(std::u16string::size_type* start,
                          std::u16string::size_type* end) const override;
  size_t GetAllSelectionsLength() const override;
  void SelectAll(bool reversed) override {}
  void SetFocus(bool is_user_initiated) override {}
  void ApplyCaretVisibility() override {}
  void OnInlineAutocompleteTextCleared() override {}
  void OnRevertTemporaryText(const std::u16string& display_text,
                             const AutocompleteMatch& match) override {}
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;

  // OmniboxTextChangeDelegate methods

  void OnDidBeginEditing() override;
  bool OnWillChange(NSRange range, NSString* new_text) override;
  void OnDidChange(bool processing_user_input) override;
  void EndEditing() override;
  void OnCopy() override;
  void ClearText() override;
  void WillPaste() override;
  void OnDeleteBackward() override;
  void OnAcceptAutocomplete() override;
  void OnRemoveAdditionalText() override;
  void RemoveThumbnail() override;

  // OmniboxTextAcceptDelegate methods
  void OnAccept() override;

  // OmniboxPopupViewSuggestionsDelegate methods
  void OnPopupDidScroll() override;
  void OnSelectedMatchForAppending(const std::u16string& str) override;
  void OnSelectedMatchForOpening(AutocompleteMatch match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 const std::u16string& pasted_text,
                                 size_t index) override;
  void OnCallActionTap() override;

  // Updates this edit view to show the proper text, highlight and images.
  void UpdateAppearance();

  // Updates the appearance of popup to have proper text alignment.
  void UpdatePopupAppearance();

  void OnClear();

  // Hide keyboard only.  Used when omnibox popups grab focus but editing isn't
  // complete.
  void HideKeyboard();

  // Focus the omnibox field.  This is used when the omnibox popup copies a
  // search query to the omnibox so the user can modify it further.
  // This does not affect the popup state and is a NOOP if the omnibox is
  // already focused.
  void FocusOmnibox();

  // Returns `true` if AutocompletePopupView is currently open.
  BOOL IsPopupOpen();

 protected:
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override {}

 private:
  void SetEmphasis(bool emphasize, const gfx::Range& range) override {}
  void UpdateSchemeStyle(const gfx::Range& scheme_range) override {}

  /// Accepts thumbnail edits and update the client.
  void AcceptThumbnailEdits();
  /// Discards edits and restore the thumbnail.
  void RevertThumbnailEdits();

  OmniboxTextFieldIOS* field_;

  // Focuser, used to transition the location bar to focused/defocused state as
  // necessary.
  __weak id<OmniboxCommands> omnibox_focuser_;

  // Delegate that manages the browser UI changes in response to omnibox being
  // focused and defocused.
  __weak id<OmniboxFocusDelegate> focus_delegate_;

  // Handler for ToolbarCommands.
  __weak id<ToolbarCommands> toolbar_commands_handler_;

  // Consumer for this class.
  __weak id<OmniboxViewConsumer> consumer_;

  State state_before_change_;
  NSString* marked_text_before_change_;
  NSRange current_selection_;
  NSRange old_selection_;

  // Thumbnail image before any edit from the omnibox.
  UIImage* thumbnail_image_before_edit_;
  // Whether the thumbnail image was removed during omnibox edit.
  BOOL thumbnail_deleted_;

  // TODO(rohitrao): This is a monster hack, needed because closing the popup
  // ends up inadvertently triggering a new round of autocomplete.  Fix the
  // underlying problem, which is that textDidChange: is called when closing the
  // popup, and then remove this hack.  b/5877366.
  BOOL ignore_popup_updates_;

  // Whether the popup was scrolled during this omnibox interaction.
  bool suggestions_list_scrolled_ = false;
  // Whether it's the lens overlay omnibox.
  bool is_lens_overlay_;

  raw_ptr<OmniboxPopupProvider> popup_provider_;  // weak

  // Used to cancel clipboard callbacks if this is deallocated;
  base::WeakPtrFactory<OmniboxViewIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
