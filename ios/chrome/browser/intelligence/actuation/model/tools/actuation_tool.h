// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

struct ActuationError;
class Browser;
class ProfileIOS;

namespace web {
class WebState;
}

// Abstract base class for all actuation tools.
class ActuationTool {
 public:
  using ActuationResult = base::expected<void, ActuationError>;
  using ActuationCallback = base::OnceCallback<void(ActuationResult)>;

  // Result of resolving a tab ID to its associated objects.
  struct TabResolutionResult {
    TabResolutionResult();
    TabResolutionResult(const TabResolutionResult&);
    TabResolutionResult& operator=(const TabResolutionResult&);
    ~TabResolutionResult();

    // The browser containing the tab.
    raw_ptr<Browser> browser = nullptr;
    // The index of the tab in the browser's web state list.
    int tab_index = WebStateList::kInvalidIndex;
    // A weak pointer to the tab's WebState.
    base::WeakPtr<web::WebState> web_state;
  };

  virtual ~ActuationTool() = default;

  // Executes the tool.
  virtual void Execute(ActuationCallback callback) = 0;

 protected:
  // Resolves the given `tab_id` to its associated objects in regular Browsers.
  // Returns an ActuationError if the tab or its associated objects are not
  // found.
  static base::expected<TabResolutionResult, ActuationError> ResolveTab(
      int32_t tab_id,
      ProfileIOS* profile);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_TOOLS_ACTUATION_TOOL_H_
