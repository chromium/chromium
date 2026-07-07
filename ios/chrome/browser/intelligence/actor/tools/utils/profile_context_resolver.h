// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_PROFILE_CONTEXT_RESOLVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_PROFILE_CONTEXT_RESOLVER_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

class ProfileIOS;
class UrlLoadingBrowserAgent;

namespace web {
class WebState;
}  // namespace web

namespace actor {

// A utility class that provides accessors to data tied to a `ProfileIOS`.
class ProfileContextResolver {
 public:
  explicit ProfileContextResolver(ProfileIOS* profile);
  ~ProfileContextResolver();

  // Result of resolving a tab ID to its associated objects.
  struct TabResolutionResult {
    TabResolutionResult();
    TabResolutionResult(const TabResolutionResult&);
    TabResolutionResult& operator=(const TabResolutionResult&);
    TabResolutionResult(TabResolutionResult&&);
    TabResolutionResult& operator=(TabResolutionResult&&);
    ~TabResolutionResult();

    base::WeakPtr<UrlLoadingBrowserAgent> url_loader;
    int tab_index = WebStateList::kInvalidIndex;
    base::WeakPtr<web::WebState> web_state;
    base::WeakPtr<WebStateList> web_state_list;
  };

  // Resolves the given `tab_id` to its associated objects.
  base::expected<TabResolutionResult, ToolExecutionResult> ResolveTab(
      int32_t tab_id) const;

 private:
  // The ProfileIOS is guaranteed to outlive this resolver instance.
  raw_ptr<ProfileIOS> profile_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_PROFILE_CONTEXT_RESOLVER_H_
