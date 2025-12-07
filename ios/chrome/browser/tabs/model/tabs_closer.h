// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TABS_CLOSER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TABS_CLOSER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/saved_tab_groups/public/types.h"

namespace base {
class Uuid;
}
class Browser;

// TabsCloser implements the "close all tabs" operation with support for undo.
class TabsCloser {
 public:
  // Policy controlling whether the "close all tabs" consider only regular
  // tabs or all tabs (including pinned tabs).
  enum class ClosePolicy {
    kAllTabs,
    kRegularTabs,
  };

  // Constructs an instance with a `browser` and a `policy`.
  TabsCloser(Browser* browser, ClosePolicy policy);

  TabsCloser(const TabsCloser&) = delete;
  TabsCloser& operator=(const TabsCloser&) = delete;

  ~TabsCloser();

  // Returns true if there are tabs that can be closed, according to the policy.
  bool CanCloseTabs() const;

  // Closes all tabs according to the policy, returning the number of closed
  // tabs. It is an error to call this method if `CanCloseTabs()` is `false`.
  int CloseTabs();

  // Returns whether there are tabs that can be restored.
  bool CanUndoCloseTabs() const;

  // Reopens closed tabs, returning the number of restored tabs. It is an error
  // to call this method if `CanUndoCloseTabs()` is `false`.
  int UndoCloseTabs();

  // Drops undo information, returning the number of deletions confirmed.
  // It is an error to call this method if `CanUndoCloseTabs()` is `false`.
  int ConfirmDeletion();

 private:
  class UndoStorage;

  raw_ptr<Browser> browser_ = nullptr;
  std::unique_ptr<UndoStorage> state_;
  std::map<tab_groups::LocalTabGroupID, base::Uuid> local_to_saved_group_ids_;
  const ClosePolicy close_policy_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_CLOSER_H_
