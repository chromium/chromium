// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/omnibox/browser/location_bar_model.h"

class OmniboxControllerIOS;
class OmniboxEditModelIOS;
@class OmniboxTextController;
@class OmniboxTextFieldIOS;

// Wraps a UITextField and interfaces with the rest of the autocomplete system.
class OmniboxViewIOS {
 public:
  // Retains `field`.
  OmniboxViewIOS(OmniboxTextFieldIOS* field);
  OmniboxViewIOS(const OmniboxViewIOS&) = delete;
  OmniboxViewIOS& operator=(const OmniboxViewIOS&) = delete;
  virtual ~OmniboxViewIOS();

  void SetOmniboxEditModel(OmniboxEditModelIOS* edit_model);

  void SetOmniboxController(OmniboxControllerIOS* omnibox_controller);

  void SetOmniboxTextController(OmniboxTextController* controller) {
    omnibox_text_controller_ = controller;
  }

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  virtual std::u16string GetText() const;

  // The user text is the text the user has manually keyed in. When present,
  // this is shown in preference to the permanent text; hitting escape will
  // revert to the permanent text.
  void SetUserText(const std::u16string& text);
  virtual void SetUserText(const std::u16string& text, bool update_popup);

  // Sets the window text and the caret position. `notify_text_changed` is true
  // if the model should be notified of the change. Clears the additional text.
  virtual void SetWindowTextAndCaretPos(const std::u16string& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed);

  // Sets the caret position. Removes any selection. Clamps the requested caret
  // position to the length of the current text.
  virtual void SetCaretPos(size_t caret_pos);

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  virtual void RevertAll();

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  virtual void UpdatePopup();

  // Closes the autocomplete popup, if it's open. The name `ClosePopup`
  // conflicts with the OSX class override as that has a base class that also
  // defines a method with that name.
  virtual void CloseOmniboxPopup();

  // Called when the inline autocomplete text in the model may have changed.
  // `user_text` is the portion of omnibox text the user typed.
  // `inline`_autocompletion` is the autocompleted part.
  virtual void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& user_text,
      const std::u16string& inline_autocompletion);

  // Sets the omnibox adjacent additional text label in the location bar view.
  virtual void SetAdditionalText(const std::u16string& text);

  // Called when autocomplete text is accepted. (e.g. tap on autocomplete text,
  // tap on left/right arrow key).
  void OnAcceptAutocomplete();

 private:
  friend class TestOmniboxViewIOS;

  // Internally invoked whenever the text changes in some way.
  virtual void TextChanged();

  base::WeakPtr<OmniboxControllerIOS> controller_;
  OmniboxTextFieldIOS* field_;

  // TODO(crbug.com/379693750): This is a monster hack, needed because closing
  // the popup ends up inadvertently triggering a new round of autocomplete. Fix
  // the underlying problem, which is that textDidChange: is called when closing
  // the popup, and then remove this hack.
  BOOL ignore_popup_updates_;

  /// Controller that will replace OmniboxViewIOS at the end of the refactoring
  /// crbug.com/390409559.
  __weak OmniboxTextController* omnibox_text_controller_;

  base::WeakPtr<OmniboxEditModelIOS> model_;

  // Used to cancel clipboard callbacks if this is deallocated;
  base::WeakPtrFactory<OmniboxViewIOS> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_VIEW_IOS_H_
