// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TABS_CLOSER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TABS_CLOSER_H_

#include <memory>

#include "base/memory/raw_ptr.h"

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

  // Closes all regular tabs, returning the number of closed tabs.
  // It is an error to call this method if `CanCloseTabs()` returns false.
  int CloseTabs();

  // Undo closing regular tabs, returning the number of restored tabs.
  // It is an error to call this method if `CanUndoCloseTabs()` returns false.
  int UndoCloseTabs();

  // Drop undo information, returning the number of deletion confirmed.
  // It is an error to call this method if `CanUndoCloseTabs()` returns false.
  int ConfirmDeletion();

  // Returns true if there are tabs that can be closed (pinned tabs are
  // considered as non-closable).
  bool CanCloseTabs() const;

  // Returns whether there are tabs that can be restored.
  bool CanUndoCloseTabs() const;

 private:
  class UndoStorage;

  raw_ptr<Browser> browser_ = nullptr;
  std::unique_ptr<UndoStorage> state_;
  const ClosePolicy close_policy_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_CLOSER_H_
