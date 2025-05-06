// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class OmniboxPopupViewBase.  Each toolkit
// will implement the popup view differently, so that code is inherently
// platform specific.  However, the OmniboxPopupModel needs to do some
// communication with the view.  Since the model is shared between platforms,
// we need to define an interface that all view implementations will share.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_BASE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_BASE_H_

#import <stddef.h>

#import <string_view>

#import "base/callback_list.h"
#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "build/build_config.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"

class OmniboxControllerIOS;
class OmniboxEditModelIOS;
namespace ui {
struct AXNodeData;
}

class OmniboxPopupViewBase {
 public:
  explicit OmniboxPopupViewBase(OmniboxControllerIOS* controller);
  virtual ~OmniboxPopupViewBase();

  virtual OmniboxEditModelIOS* model();
  virtual const OmniboxEditModelIOS* model() const;

  virtual OmniboxControllerIOS* controller();
  virtual const OmniboxControllerIOS* controller() const;

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const = 0;

  // Invalidates one line of the autocomplete popup.
  virtual void InvalidateLine(size_t line) = 0;

  // Invoked when the selection changes. The `line` field in either selection
  // may be OmniboxPopupSelection::kNoMatch. This method is invoked by the
  // model.
  virtual void OnSelectionChanged(OmniboxPopupSelection old_selection,
                                  OmniboxPopupSelection new_selection) {}

  // Redraws the popup window to match any changes in the result set; this may
  // mean opening or closing the window.
  virtual void UpdatePopupAppearance() = 0;

  // Called to inform result view of button focus.
  virtual void ProvideButtonFocusHint(size_t line) = 0;

  // Notification that the icon used for the given match has been updated.
  virtual void OnMatchIconUpdated(size_t match_index) = 0;

  // This method is called when the view should cancel any active drag (e.g.
  // because the user pressed ESC). The view may or may not need to take any
  // action (e.g. releasing mouse capture).  Note that this can be called when
  // no drag is in progress.
  virtual void OnDragCanceled() = 0;

  // Popup equivalent of GetAccessibleNodeData, used only by a unit test.
  virtual void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const = 0;

  // Returns result view button text. This is currently only needed by a single
  // unit test and it would be better to eliminate it than to increase usage.
  virtual std::u16string_view GetAccessibleButtonTextForResult(
      size_t line) const;

  // Updates the result and header views based on the visibility of their group.
  virtual void SetSuggestionGroupVisibility(size_t match_index,
                                            bool suggestion_group_hidden) {}

  // Adds a callback that will be called when the popup window becomes visible.
  base::CallbackListSubscription AddOpenListener(
      base::RepeatingClosure callback);

 protected:
  // Call when the popup will appear to notify listeners.
  void NotifyOpenListeners();

 private:
  base::RepeatingClosureList on_popup_callbacks_;

  // Owned by OmniboxView which owns this.
  const raw_ptr<OmniboxControllerIOS> controller_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_VIEW_BASE_H_
