// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_LOGGING_UTIL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_LOGGING_UTIL_H_

#import <string>
#import <vector>

#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "url/gurl.h"

class AggregatedJournal;

namespace actor {

// Logs a generic event with key-value details to the AggregatedJournal.
void LogJournalEvent(
    AggregatedJournal& journal,
    const GURL& url,
    ActorTaskId task_id,
    std::string_view event_name,
    const std::vector<std::pair<std::string, std::string>>& details = {});

// Logs a ToolExecutionResult immediately to the AggregatedJournal.
//
// If the result is OK the success message is logged with `success_details_key`.
// Otherwise, it logs the error message with the 'error' key.
void LogToolExecutionResult(AggregatedJournal& journal,
                            const GURL& url,
                            ActorTaskId task_id,
                            std::string_view event_name,
                            const ToolExecutionResult& result,
                            std::string_view success_details_key = "details");

// Starts an async event during tool execution.
std::unique_ptr<AggregatedJournal::PendingAsyncEntry> StartAsyncJournalEntry(
    AggregatedJournal& journal,
    const GURL& url,
    ActorTaskId task_id,
    const std::string& tool_name,
    const std::string& event_name,
    std::string_view tool_debug_string);

// Ends an async event with the given result.
void EndAsyncJournalEntry(AggregatedJournal::PendingAsyncEntry* entry,
                          const ToolExecutionResult& result);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_LOGGING_UTIL_H_
