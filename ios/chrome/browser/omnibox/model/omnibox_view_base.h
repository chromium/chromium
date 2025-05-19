// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_BASE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_BASE_H_

#import <stddef.h>

#import <memory>
#import <string>

#import "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "third_party/skia/include/core/SkColor.h"
#import "ui/base/models/image_model.h"
#import "ui/base/window_open_disposition.h"
#import "ui/gfx/image/image_skia.h"
#import "ui/gfx/native_widget_types.h"
#import "ui/gfx/range/range.h"

class OmniboxControllerIOS;
class OmniboxEditModelIOS;

class OmniboxViewBase {
 public:
  // Represents the changes between two State objects.  This is used by the
  // model to determine how its internal state should be updated after the view
  // state changes.  See OmniboxEditModelIOS::OnAfterPossibleChange().
  struct StateChanges {
    // `old_text` and `new_text` are not owned.
    raw_ptr<const std::u16string> old_text;
    raw_ptr<const std::u16string> new_text;
    size_t new_sel_start;
    size_t new_sel_end;
    bool selection_differs;
    bool text_differs;
    bool just_deleted_text;
  };

  virtual ~OmniboxViewBase();
  OmniboxViewBase(const OmniboxViewBase&) = delete;
  OmniboxViewBase& operator=(const OmniboxViewBase&) = delete;

  OmniboxEditModelIOS* model();
  const OmniboxEditModelIOS* model() const;

  OmniboxControllerIOS* controller();
  const OmniboxControllerIOS* controller() const;

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  virtual std::u16string GetText() const = 0;

  // The user text is the text the user has manually keyed in.  When present,
  // this is shown in preference to the permanent text; hitting escape will
  // revert to the permanent text.
  void SetUserText(const std::u16string& text);
  virtual void SetUserText(const std::u16string& text, bool update_popup);

  // Sets the window text and the caret position. `notify_text_changed` is true
  // if the model should be notified of the change. Clears the additional text.
  virtual void SetWindowTextAndCaretPos(const std::u16string& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed) = 0;

  // Sets the caret position. Removes any selection. Clamps the requested caret
  // position to the length of the current text.
  virtual void SetCaretPos(size_t caret_pos) = 0;

  // Sets the omnibox adjacent additional text label in the location bar view.
  virtual void SetAdditionalText(const std::u16string& text) = 0;

  // Fills `start` and `end` with the indexes of the current selection's bounds.
  // It is not guaranteed that `*start < *end`, as the selection can be
  // directed.  If there is no selection, `start` and `end` will both be equal
  // to the current cursor position.
  virtual void GetSelectionBounds(size_t* start, size_t* end) const = 0;

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  virtual void RevertAll();

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  virtual void UpdatePopup() = 0;

  // Closes the autocomplete popup, if it's open. The name `ClosePopup`
  // conflicts with the OSX class override as that has a base class that also
  // defines a method with that name.
  virtual void CloseOmniboxPopup();

  // Called when the inline autocomplete text in the model may have changed.
  // `user_text` is the portion of omnibox text the user typed.
  // `inline`_autocompletion` is the autocompleted part.
  virtual void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion) = 0;

  // Checkpoints the current edit state before an operation that might trigger
  // a new autocomplete run to open or modify the popup. Call this before
  // user-initiated edit actions that trigger autocomplete, but *not* for
  // automatic changes to the textfield that should not affect autocomplete.
  virtual void OnBeforePossibleChange() = 0;

  // OnAfterPossibleChange() returns true if there was a change that caused it
  // to call UpdatePopup().
  virtual bool OnAfterPossibleChange() = 0;

  // Returns `text` with any leading javascript schemas stripped.
  static std::u16string StripJavascriptSchemas(const std::u16string& text);

  // Automatically collapses internal whitespace as follows:
  // * Leading and trailing whitespace are often copied accidentally and rarely
  //   affect behavior, so they are stripped.  If this collapses the whole
  //   string, returns a space, since pasting nothing feels broken.
  // * Internal whitespace sequences not containing CR/LF may be integral to the
  //   meaning of the string and are preserved exactly.  The presence of any of
  //   these also suggests the input is more likely a search than a navigation,
  //   which affects the next bullet.
  // * Internal whitespace sequences containing CR/LF have likely been split
  //   across lines by terminals, email programs, etc., and are collapsed.  If
  //   there are any internal non-CR/LF whitespace sequences, the input is more
  //   likely search data (e.g. street addresses), so collapse these to a single
  //   space.  If not, the input might be a navigation (e.g. a line-broken URL),
  //   so collapse these away entirely.
  //
  // Finally, calls StripJavascriptSchemas() on the resulting string.
  static std::u16string SanitizeTextForPaste(const std::u16string& text);

 protected:
  // Tracks important state that may change between OnBeforePossibleChange() and
  // OnAfterPossibleChange().
  struct State {
    std::u16string text;
    size_t sel_start;
    size_t sel_end;
  };

  explicit OmniboxViewBase(std::unique_ptr<OmniboxClient> client);

  // Fills `state` with the current text state.
  void GetState(State* state);

  // Returns the delta between `before` and `after`.
  StateChanges GetStateChanges(const State& before, const State& after);

  // Internally invoked whenever the text changes in some way.
  virtual void TextChanged();

 private:
  friend class TestOmniboxViewBase;

  std::unique_ptr<OmniboxControllerIOS> controller_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_BASE_H_
