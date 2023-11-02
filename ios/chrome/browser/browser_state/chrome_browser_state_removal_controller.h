// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_REMOVAL_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_REMOVAL_CONTROLLER_H_

#include <string>

// Controls the removal of extra browser states.
class ChromeBrowserStateRemovalController {
 public:
  static ChromeBrowserStateRemovalController* GetInstance();

  // Removes the browser states marked as not to keep if they exist. It also
  // converts the most recently used bookmarks file to an HTML representation.
  void RemoveBrowserStatesIfNecessary();

  // Returns whether a browser state has been removed. The value is conserved
  // across application restarts.
  bool HasBrowserStateBeenRemoved();

  // Returns the GAIA Id of the removed browser state if it was authenticated.
  // The value should not be trusted unless HasBrowserStateBeenRemoved() returns
  // true.
  const std::string& removed_browser_state_gaia_id() const {
    return removed_browser_state_gaia_id_;
  }

  // Returns whether the last used browser sate was changed during this session.
  bool has_changed_last_used_browser_state() const {
    return has_changed_last_used_browser_state_;
  }

 private:
  ChromeBrowserStateRemovalController();
  ~ChromeBrowserStateRemovalController();

  // Returns the relative path of the browser state path to keep. This value
  // was stored from the user choice.
  std::string GetBrowserStatePathToKeep();

  // Sets whether a browser state has been removed. The value is conserved
  // across application restarts.
  void SetHasBrowserStateBeenRemoved(bool value);

  // Returns the relative path of the last browser state used (during the
  // previous application run).
  std::string GetLastBrowserStatePathUsed();

  // Sets the relative path of the last browser state used.
  void SetLastBrowserStatePathUsed(const std::string& browser_state_path);

  // The GAIA Id of the removed browser state (if any).
  std::string removed_browser_state_gaia_id_;

  // Whether the last used browser state was changed.
  bool has_changed_last_used_browser_state_;
};

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_REMOVAL_CONTROLLER_H_
