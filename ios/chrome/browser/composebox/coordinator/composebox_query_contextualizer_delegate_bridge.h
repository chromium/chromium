// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_QUERY_CONTEXTUALIZER_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_QUERY_CONTEXTUALIZER_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <vector>

#import "base/functional/callback.h"
#import "components/contextual_tasks/public/query_contextualizer.h"
#import "url/gurl.h"

// Forward declarations.
class SessionID;
namespace lens {
struct ContextualInputData;
struct ImageEncodingOptions;
}  // namespace lens
namespace contextual_search {
class ContextualSearchSessionHandle;
}

// Protocol for the C++ QueryContextualizerDelegateBridge to communicate with
// Objective-C.
@protocol ComposeboxQueryContextualizerDelegate <NSObject>

- (GURL)tabURLForTabID:(contextual_tasks::QueryContextualizer::TabId)tabID;

- (SessionID)tabSessionIDForTabID:
    (contextual_tasks::QueryContextualizer::TabId)tabID;

- (void)
    getPageContextForTabID:(contextual_tasks::QueryContextualizer::TabId)tabID
                  callback:(base::OnceCallback<void(
                                std::unique_ptr<lens::ContextualInputData>)>)
                               callback;

- (bool)isTabValid:(contextual_tasks::QueryContextualizer::TabId)tabID;

- (std::optional<lens::ImageEncodingOptions>)tabViewportEncodingOptions;

- (contextual_search::ContextualSearchSessionHandle*)getOrCreateSessionHandle;

@end

// Bridge class to forward QueryContextualizer::Delegate calls to an Objective-C
// protocol.
class QueryContextualizerDelegateBridge
    : public contextual_tasks::QueryContextualizer::Delegate {
 public:
  explicit QueryContextualizerDelegateBridge(
      id<ComposeboxQueryContextualizerDelegate> delegate);
  ~QueryContextualizerDelegateBridge() override;

  GURL GetTabUrl(contextual_tasks::QueryContextualizer::TabId id) override;

  SessionID GetTabSessionId(
      contextual_tasks::QueryContextualizer::TabId id) override;

  void GetPageContext(
      contextual_tasks::QueryContextualizer::TabId id,
      base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
          callback) override;

  bool IsTabValid(contextual_tasks::QueryContextualizer::TabId id) override;

  std::optional<lens::ImageEncodingOptions>
  GetTabViewportEncodingOptionsForQueryContextualizer() override;

  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateSessionHandleForQueryContextualizer() override;

  void GetRelevantTabsForQuery(
      const std::string& query_text,
      const std::vector<GURL>& attached_context_urls,
      base::OnceCallback<void(
          std::vector<contextual_tasks::QueryContextualizer::TabId>)> callback)
      override;

 private:
  __weak id<ComposeboxQueryContextualizerDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_QUERY_CONTEXTUALIZER_DELEGATE_BRIDGE_H_
