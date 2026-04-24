// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_query_contextualizer_delegate_bridge.h"

#import "components/sessions/core/session_id.h"

QueryContextualizerDelegateBridge::QueryContextualizerDelegateBridge(
    id<ComposeboxQueryContextualizerDelegate> delegate)
    : delegate_(delegate) {}

QueryContextualizerDelegateBridge::~QueryContextualizerDelegateBridge() =
    default;

GURL QueryContextualizerDelegateBridge::GetTabUrl(
    contextual_tasks::QueryContextualizer::TabId id) {
  return [delegate_ tabURLForTabID:id];
}

SessionID QueryContextualizerDelegateBridge::GetTabSessionId(
    contextual_tasks::QueryContextualizer::TabId id) {
  return [delegate_ tabSessionIDForTabID:id];
}

void QueryContextualizerDelegateBridge::GetPageContext(
    contextual_tasks::QueryContextualizer::TabId id,
    base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
        callback) {
  [delegate_ getPageContextForTabID:id callback:std::move(callback)];
}

bool QueryContextualizerDelegateBridge::IsTabValid(
    contextual_tasks::QueryContextualizer::TabId id) {
  return [delegate_ isTabValid:id];
}

std::optional<lens::ImageEncodingOptions> QueryContextualizerDelegateBridge::
    GetTabViewportEncodingOptionsForQueryContextualizer() {
  return [delegate_ tabViewportEncodingOptions];
}

contextual_search::ContextualSearchSessionHandle*
QueryContextualizerDelegateBridge::
    GetOrCreateSessionHandleForQueryContextualizer() {
  return [delegate_ getOrCreateSessionHandle];
}

void QueryContextualizerDelegateBridge::GetRelevantTabsForQuery(
    const std::string& query_text,
    const std::vector<GURL>& attached_context_urls,
    base::OnceCallback<void(
        std::vector<contextual_tasks::QueryContextualizer::TabId>)> callback) {
  std::move(callback).Run({});
}
