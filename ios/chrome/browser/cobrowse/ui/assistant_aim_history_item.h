// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HISTORY_ITEM_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HISTORY_ITEM_H_

#include <string>

// Represents a history item in the Assistant AIM.
struct AssistantAIMHistoryItem {
  std::string task_id;
  std::string title;
};

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_HISTORY_ITEM_H_
