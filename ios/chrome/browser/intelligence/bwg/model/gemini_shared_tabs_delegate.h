// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_DELEGATE_H_

#import <Foundation/Foundation.h>

#import <set>

#import "ios/web/public/web_state_id.h"

namespace ios::provider {
enum class GeminiPageContextAttachmentState;
}  // namespace ios::provider

namespace web {
class WebState;
}  // namespace web

@class GeminiPageContext;

// Delegate for GeminiContainerMediator to communicate with the instance
// managing the shared tabs (shared by default or by user).
// As shared tabs have longer lifecycle then Gemini itself
// GeminiContainerMediator cannot own shared tabs or its' managing instance.
// Therefore, shared tabs are owned and managed by an instance necessary
// for it's lifecylce and communicate with GeminiContainerMediator through
// this interface.
class GeminiSharedTabsDelegate {
 public:
  virtual ~GeminiSharedTabsDelegate() = default;

  // Returns the currently inactive shared tabs for the session.
  virtual NSArray<GeminiPageContext*>* GetInactiveSharedTabs() const = 0;

  // Adds the active page context to the list of shared tabs.
  virtual void SaveActivePageContextToSharedTabs(
      GeminiPageContext* active_page_context) = 0;

  // Clears the set of all shared tabs if it doesn't include `active_web_state`.
  virtual void UpdateSharedTabsForActiveWebState(
      web::WebState* active_web_state) = 0;

  // Removes a tab from selected tabs and propagates attached tabs to Gemini.
  virtual void DetachTabWithID(NSString* tab_id) = 0;

  // Changes the attachment state of the given tab without propagating it to the
  // provider.
  virtual void UpdateLocalTabAttachmentState(
      NSString* tab_id,
      ios::provider::GeminiPageContextAttachmentState new_state) = 0;

  // Returns the number of all currently shared tabs (active and inactive ones).
  virtual NSUInteger SharedTabsCount() const = 0;

  // Updates shared tabs when the tab picker selection changes.
  virtual void OnTabPickerSelectionChanged(
      std::set<web::WebStateID> selected_tabs) = 0;

  // Returns the set of shared tab IDs eligible to be pre-selected in the tab
  // picker.
  virtual std::set<web::WebStateID> GetSelectedTabsForTabPicker() const = 0;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_DELEGATE_H_
