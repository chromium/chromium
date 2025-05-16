// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_EDIT_MODEL_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_EDIT_MODEL_IOS_H_

#import <stddef.h>

#import <map>
#import <memory>
#import <string>
#import <string_view>

#import "base/compiler_specific.h"
#import "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/time/time.h"
#import "build/build_config.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox.mojom-shared.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "ios/chrome/browser/omnibox/model/omnibox_view_base.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/base/window_open_disposition.h"
#import "ui/gfx/native_widget_types.h"
#import "url/gurl.h"

class OmniboxControllerIOS;
class OmniboxPopupViewBase;
namespace gfx {
class Image;
}

class OmniboxEditModelIOS {
 public:
  OmniboxEditModelIOS(OmniboxControllerIOS* controller, OmniboxViewBase* view);
  virtual ~OmniboxEditModelIOS();
  OmniboxEditModelIOS(const OmniboxEditModelIOS&) = delete;
  OmniboxEditModelIOS& operator=(const OmniboxEditModelIOS&) = delete;

  void set_popup_view(OmniboxPopupViewBase* popup_view);
  OmniboxPopupViewBase* get_popup_view() { return popup_view_; }
  const OmniboxPopupViewBase* get_popup_view() const { return popup_view_; }

  metrics::OmniboxEventProto::PageClassification GetPageClassification() const;

  // Returns the match for the current text. If the user has not edited the text
  // this is the match corresponding to the permanent text. Returns the
  // alternate nav URL, if `alternate_nav_url` is non-NULL and there is such a
  // URL. Virtual for testing.
  virtual AutocompleteMatch CurrentMatch(GURL* alternate_nav_url) const;

  // Called when the user wants to export the entire current text as a URL.
  // Sets the url, and if known, the title and favicon.
  void GetDataForURLExport(GURL* url,
                           std::u16string* title,
                           gfx::Image* favicon);

  // Returns true if the current edit contents will be treated as a
  // URL/navigation, as opposed to a search.
  bool CurrentTextIsURL() const;

  // Adjusts the copied text before writing it to the clipboard. If the copied
  // text is a URL with the scheme elided, this method reattaches the scheme.
  // Copied text that looks like a search query will not be modified.
  //
  // `sel_min` gives the minimum of the selection, e.g. min(sel_start, sel_end).
  // `text` is the currently selected text, and may be modified by this method.
  // `url_from_text` is the GURL interpretation of the selected text, and may
  // be used for drag-and-drop models or writing hyperlink data types to
  // system clipboards.
  //
  // If the copied text is interpreted as a URL:
  //  - `write_url` is set to true.
  //  - `url_from_text` is set to the URL.
  //  - `text` is set to the URL's spec. The output will be pure ASCII and
  //    %-escaped, since canonical URLs are always encoded to ASCII.
  //
  // If the copied text is *NOT* interpreted as a URL:
  //  - `write_url` is set to false.
  //  - `url_from_text` may be modified, but might not contain a valid GURL.
  //  - `text` is full UTF-16 and not %-escaped. This is because we are not
  //    interpreting `text` as a URL, so we leave the Unicode characters as-is.
  void AdjustTextForCopy(int sel_min,
                         std::u16string* text,
                         GURL* url_from_text,
                         bool* write_url);

  bool user_input_in_progress() const { return user_input_in_progress_; }

  // Sets the state of user_input_in_progress_, and notifies the observer if
  // that state has changed.
  void SetInputInProgress(bool in_progress);

  // Calls SetInputInProgress, via SetInputInProgressNoNotify and
  // NotifyObserversInputInProgress, calling the latter after
  // StartAutocomplete, so that the result is only updated once.
  void UpdateInput(bool has_selected_text, bool prevent_inline_autocomplete);

  // Resets the permanent display texts `url_for_editing_` to those provided by
  // the controller. Returns true if the display text shave changed and the
  // change should be immediately user-visible, because either the user is not
  // editing or the edit does not have focus.
  bool ResetDisplayTexts();

  // Returns the permanent display text for the current page and Omnibox state.
  std::u16string GetPermanentDisplayText() const;

  // Sets the user_text_ to `text`. Also enters user-input-in-progress mode.
  // Virtual for testing.
  virtual void SetUserText(const std::u16string& text);

  // Invoked any time the text may have changed in the edit. Notifies the
  // controller.
  void OnChanged();

  // Reverts the edit model back to its unedited state (permanent text showing,
  // no user input in progress).
  void Revert();

  // Directs the popup to start autocomplete.  Makes use of the `view_` text and
  // selection, so make sure to set those before calling StartAutocomplete().
  void StartAutocomplete(bool has_selected_text,
                         bool prevent_inline_autocomplete);

  // Determines whether the user can "paste and go", given the specified text.
  bool CanPasteAndGo(const std::u16string& text) const;

  // Navigates to the destination last supplied to CanPasteAndGo.
  void PasteAndGo(
      const std::u16string& text,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Sets `match` and `alternate_nav_url` based on classifying `text`.
  // `alternate_nav_url` may be nullptr.
  void ClassifyString(const std::u16string& text,
                      AutocompleteMatch* match,
                      GURL* alternate_nav_url) const;

  // Opens given selection. Most kinds of selection invoke an action or
  // otherwise call `OpenMatch`, but some may `AcceptInput` which is not
  // guaranteed to open a match or commit the omnibox.
  virtual void OpenSelection(
      OmniboxPopupSelection selection,
      base::TimeTicks timestamp = base::TimeTicks(),
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

  // A simplified version of OpenSelection that opens the model's current
  // selection.
  virtual void OpenSelection(
      base::TimeTicks timestamp = base::TimeTicks(),
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

  OmniboxFocusState focus_state() const { return focus_state_; }
  bool has_focus() const { return focus_state_ != OMNIBOX_FOCUS_NONE; }

  base::TimeTicks last_omnibox_focus() const { return last_omnibox_focus_; }

  // This is the same as when the Omnibox is visibly focused.
  bool is_caret_visible() const {
    return focus_state_ == OMNIBOX_FOCUS_VISIBLE;
  }

  // Clears additional text.
  void ClearAdditionalText();

  // Called when the view is gaining focus.  `control_down` is whether the
  // control key is down (at the time we're gaining focus).
  void OnSetFocus(bool control_down);

  // Starts a request for zero-prefix suggestions if no query is currently
  // running and the popup is closed. This can be called multiple times without
  // harm, since it will early-exit if an earlier request is in progress or
  // done.
  void StartZeroSuggestRequest(bool user_clobbered_permanent_text = false);

  // Sets the visibility of the caret in the omnibox, if it has focus. The
  // visibility of the caret is reset to visible if either
  //   - The user starts typing, or
  //   - We explicitly focus the omnibox again.
  // The latter case must be handled in three separate places--OnSetFocus(),
  // OmniboxViewBase::SetFocus(), and the mouse handlers in OmniboxViewBase. See
  // accompanying comments for why each of these is necessary.
  //
  // Caret visibility is tracked per-tab and updates automatically upon
  // switching tabs.
  void SetCaretVisibility(bool visible);

  // If the ctrl key is down, marks it as consumed to prevent it from triggering
  // ctrl-enter behavior unless it is released and re-pressed.
  void ConsumeCtrlKey();

  // Sent before `OnKillFocus` and before the popup is closed.
  void OnWillKillFocus();

  // Called when the view is losing focus.  Resets some state.
  void OnKillFocus();

  // Called when the user presses the escape key.  Decides what, if anything, to
  // revert about any current edits.  Returns whether the key was handled.
  bool OnEscapeKeyPressed();

  // Called when the user presses or releases the control key.  Changes state as
  // necessary.
  void OnControlKeyChanged(bool pressed);

  // Called when the user pastes in text.
  void OnPaste();

  // Returns true if pasting is in progress.
  bool is_pasting() const { return paste_state_ == PASTING; }

  // Called when any relevant data changes.  This rolls together several
  // separate pieces of data into one call so we can update all the UI
  // efficiently. Specifically, it's invoked for autocompletion.
  //   `inline_autocompletion` is the autocompletion.
  //   `additional_text` is additional omnibox text to be displayed adjacent to
  //     the omnibox view.
  //   `new_match` is the selected match when the user is changing selection,
  //     the default match if the user is typing, or an empty match when
  //     selecting a header.
  // Virtual to allow testing.
  virtual void OnPopupDataChanged(const std::u16string& inline_autocompletion,
                                  const std::u16string& additional_text,
                                  const AutocompleteMatch& new_match);

  // Called by the OmniboxViewBase after something changes, with details about
  // what state changes occurred.  Updates internal state, updates the popup if
  // necessary, and returns true if any significant changes occurred.  Note that
  // `text_change.text_differs` may be set even if `text_change.old_text` ==
  // `text_change.new_text`, e.g. if we've just committed an IME composition.
  bool OnAfterPossibleChange(
      const OmniboxViewBase::StateChanges& state_changes);

  // Called when the current match has changed in the OmniboxControllerIOS.
  void OnCurrentMatchChanged();

  std::u16string GetUserTextForTesting() const { return user_text_; }

  AutocompleteInput GetInputForTesting() const { return input_; }

  // Name of the histogram tracking cut or copy omnibox commands.
  static const char kCutOrCopyAllTextHistogram[];

  // Just forwards the call to the OmniboxViewBase referred within.
  void SetAccessibilityLabel(const AutocompleteMatch& match);

  // Returns true if the destination URL of the match is bookmarked.
  bool IsStarredMatch(const AutocompleteMatch& match) const;

  // Gets the suggestion group header text associated with the given suggestion
  // group ID.
  // In addition to calling `AutocompleteResult::GetHeaderForSuggestionGroup()`,
  // this function takes into account certain header visibility criteria (e.g.
  // experiment flags) to determine the proper header text, which will then be
  // used by the relevant code to conditionally show suggestion group headers
  // in the Omnibox/Realbox popup.
  std::u16string GetSuggestionGroupHeaderText(
      const std::optional<omnibox::GroupId>& suggestion_group_id) const;

  // Returns true if the popup exists and is open. Virtual for testing.
  virtual bool PopupIsOpen() const;

  // Gets popup's current selection.
  OmniboxPopupSelection GetPopupSelection() const;

  // Invoked any time the result set of the controller changes.
  // This method seems like a good candidate for removal; it is
  // preserved here only to prevent possible behavior change while refactoring.
  void OnPopupResultChanged();

  void SetAutocompleteInput(AutocompleteInput input);

  // This calls `OpenMatch` directly for the few remaining `OmniboxEditModelIOS`
  // test cases that require explicit control over match content. For new
  // tests, and for non-test code, use `OpenSelection`.
  void OpenMatchForTesting(
      AutocompleteMatch match,
      WindowOpenDisposition disposition,
      const GURL& alternate_nav_url,
      const std::u16string& pasted_text,
      size_t index,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

 protected:
  // Utility method to get current PrefService; protected instead of private
  // because it may be overridden by derived test classes.
  virtual PrefService* GetPrefService();
  virtual const PrefService* GetPrefService() const;

 private:
  friend class OmniboxControllerIOSTest;
  friend class TestOmniboxEditModelIOS;
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelIOSTest, ConsumeCtrlKey);
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelIOSTest,
                           ConsumeCtrlKeyOnRequestFocus);
  FRIEND_TEST_ALL_PREFIXES(OmniboxEditModelIOSTest, ConsumeCtrlKeyOnCtrlAction);

  enum PasteState {
    NONE,     // Most recent edit was not a paste.
    PASTING,  // In the middle of doing a paste. We need this intermediate state
              // because `OnPaste()` does the actual detection of paste, but
              // `OnAfterPossibleChange()` has to update the paste state for
              // every edit. If `OnPaste()` set the state directly to PASTED,
              // `OnAfterPossibleChange()` wouldn't know whether that
              // represented the current edit or a past one.
    PASTED,   // Most recent edit was a paste.
  };

  enum ControlKeyState {
    UP,                // The control key is not depressed.
    DOWN,              // The control key is depressed and should trigger the
                       // "ctrl-enter" behavior when the user hits enter.
    DOWN_AND_CONSUMED  // The control key is depressed, but has been consumed
                       // and should not trigger the "ctrl-enter" behavior.
                       // The control key becomes consumed if it has been used
                       // for another action such as focusing the location bar
                       // with ctrl-l or copying the selected text with ctrl-c.
  };

  AutocompleteController* autocomplete_controller() const;

  // If no query is in progress, starts working on an autocomplete query.
  // Returns true if started; false otherwise.
  bool MaybeStartQueryForPopup();

  // Asks the browser to load the popup's currently selected item, using the
  // supplied disposition.  This may close the popup.
  void AcceptInput(
      WindowOpenDisposition disposition,
      base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // Asks the browser to load `match` or execute one of its actions
  // according to `selection`.
  //
  // OpenMatch() needs to know the original text that drove this action.  If
  // `pasted_text` is non-empty, this is a Paste-And-Go/Search action, and
  // that's the relevant input text.  Otherwise, the relevant input text is
  // either the user text or the display URL, depending on if user input is
  // in progress.
  //
  // `match` is passed by value for two reasons:
  // (1) This function needs to modify `match`, so a const ref isn't
  //     appropriate.  Callers don't actually care about the modifications, so a
  //     pointer isn't required.
  // (2) The passed-in match is, on the caller side, typically coming from data
  //     associated with the popup.  Since this call can close the popup, that
  //     could clear that data, leaving us with a pointer-to-garbage.  So at
  //     some point someone needs to make a copy of the match anyway, to
  //     preserve it past the popup closure.
  void OpenMatch(OmniboxPopupSelection selection,
                 AutocompleteMatch match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const std::u16string& pasted_text,
                 base::TimeTicks match_selection_timestamp = base::TimeTicks());

  // An internal method to set the user text. Notably, this differs from
  // SetUserText because it does not change the user-input-in-progress state.
  void InternalSetUserText(const std::u16string& text);

  // Copies a match corresponding to the current text into `match`, and
  // populates `alternate_nav_url` as well if it's not nullptr. If the popup
  // is closed, the match is generated from the autocomplete classifier.
  void GetInfoForCurrentText(AutocompleteMatch* match,
                             GURL* alternate_nav_url) const;

  // Sets the state of user_input_in_progress_. Returns whether said state
  // changed, so that the caller can evoke NotifyObserversInputInProgress().
  bool SetInputInProgressNoNotify(bool in_progress);

  // Notifies the observers that the state has changed.
  void NotifyObserversInputInProgress(bool in_progress);

  // If focus_state_ does not match `state`, we update it and notify the
  // InstantController about the change (passing along the `reason` for the
  // change). If the caret visibility changes, we call ApplyCaretVisibility() on
  // the view.
  void SetFocusState(OmniboxFocusState state, OmniboxFocusChangeReason reason);

  // Returns view text if there is a view. Until the model is made the
  // primary data source, this should not be called when there's no view.
  std::u16string GetText() const;

  // Closes lens if still needed.
  void MaybeCloseLens();

  // Owns this.
  raw_ptr<OmniboxControllerIOS> controller_;

  // Owns `OmniboxControllerIOS` which owns this.
  raw_ptr<OmniboxViewBase> view_;

  OmniboxFocusState focus_state_ = OMNIBOX_FOCUS_NONE;

  // The initial text representing the current URL suitable for editing.
  std::u16string url_for_editing_;

  // This flag is true when the user has modified the contents of the edit, but
  // not yet accepted them.  We use this to determine whether changes to the
  // page URL should be immediately displayed. This flag *should* be true in a
  // superset of the cases where the popup is open. Except (crbug.com/1340378)
  // for zero suggestions when the popup was opened with ctrl+L or a mouse click
  // (as opposed to the down arrow).
  bool user_input_in_progress_;

  // The text that the user has entered.  This does not include inline
  // autocomplete text that has not yet been accepted.  `user_text_` can
  // contain a string without `user_input_in_progress_` being true.
  std::u16string user_text_;

  // Used to know what should be displayed. Updated when e.g. the popup
  // selection changes, the results change, on navigation, on tab switch etc; it
  // should always be up-to-date.
  AutocompleteMatch current_match_;

  // We keep track of when the user last focused on the omnibox.
  base::TimeTicks last_omnibox_focus_;

  // Indicates whether the current interaction with the Omnibox resulted in
  // navigation (true), or user leaving the omnibox without taking any action
  // (false).
  // The value is initialized when the Omnibox receives focus and available for
  // use when the focus is about to be cleared.
  bool focus_resulted_in_navigation_;

  // We keep track of when the user began modifying the omnibox text.
  // This should be valid whenever user_input_in_progress_ is true.
  base::TimeTicks time_user_first_modified_omnibox_;

  // When the user closes the popup, we need to remember the URL for their
  // desired choice, so that if they hit enter without reopening the popup we
  // know where to go.  We could simply rerun autocomplete in this case, but
  // we'd need to either wait for all results to come in (unacceptably slow) or
  // do the wrong thing when the user had chosen some provider whose results
  // were not returned instantaneously.
  //
  // This variable is only valid when user_input_in_progress_ is true, since
  // when it is false the user has either never input anything (so there won't
  // be a value here anyway) or has canceled their input, which should be
  // treated the same way.  Also, since this is for preserving a desired URL
  // after the popup has been closed, we ignore this if the popup is open, and
  // simply ask the popup for the desired URL directly.  As a result, the
  // contents of this variable only need to be updated when the popup is closed
  // but user_input_in_progress_ is not being cleared.
  std::u16string url_for_remembered_user_selection_;

  // Inline autocomplete is allowed if the user has not just deleted text. In
  // this case, inline_autocompletion_ is appended to the user_text_ and
  // displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text_;
  std::u16string inline_autocompletion_;

  // When the user's last action was to paste, we disallow inline autocomplete
  // (on the theory that the user is trying to paste in a new URL or part of
  // one, and in either case inline autocomplete would get in the way).
  PasteState paste_state_;

  // Whether the control key is depressed.  We track this to avoid calling
  // UpdatePopup() repeatedly if the user holds down the key, and to know
  // whether to trigger "ctrl-enter" behavior.
  ControlKeyState control_key_state_;

  // This is needed to properly update the SearchModel state when the user
  // presses escape.
  bool in_revert_;

  // When exiting the scope or committing the omnibox, it should
  // be closed again, unless lens is invoked by taking a contextual search
  // match. In that case, the omnibox relinquishes the obligation to close so
  // as to not interfere with lens match fulfillment and continued use.
  bool close_lens_;

  // The input that was sent to the AutocompleteController. Since no
  // autocomplete query is started after a tab switch, it is possible for this
  // `input_` to differ from the one currently stored in AutocompleteController.
  AutocompleteInput input_;

  // The popup view is nullptr when there's no popup, and is non-null when
  // a popup view exists (i.e. between calls to `set_popup_view`).
  raw_ptr<OmniboxPopupViewBase> popup_view_ = nullptr;

  // The current popup selection; set to normal kNoMatch when there's no popup.
  OmniboxPopupSelection popup_selection_ =
      OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                            OmniboxPopupSelection::NORMAL);

  // When a result changes, this informs of the URL in the previously selected
  // suggestion whose tab switch button was focused, so that we may compare
  // if equal.
  GURL old_focused_url_;

  base::WeakPtrFactory<OmniboxEditModelIOS> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_EDIT_MODEL_IOS_H_
