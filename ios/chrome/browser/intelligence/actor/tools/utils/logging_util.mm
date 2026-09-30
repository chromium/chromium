// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"

#import "base/check.h"
#import "base/strings/stringprintf.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/journal_details_builder.h"
#import "url/gurl.h"

namespace actor {

void LogJournalEvent(
    AggregatedJournal& journal,
    const GURL& url,
    ActorTaskId task_id,
    std::string_view event_name,
    const std::vector<std::pair<std::string, std::string>>& details) {
  JournalDetailsBuilder builder;
  for (const auto& [key, value] : details) {
    builder.Add(key, value);
  }

  journal.Log(url, task_id, event_name, std::move(builder).Build());
}

void LogToolExecutionResult(AggregatedJournal& journal,
                            const GURL& url,
                            ActorTaskId task_id,
                            std::string_view event_name,
                            const ToolExecutionResult& result,
                            std::string_view success_details_key) {
  std::vector<mojom::JournalDetailsPtr> details;

  if (!result.IsOk()) {
    details = JournalDetailsBuilder()
                  .AddError(GetToolExecutionResultMessage(result))
                  .Build();
  } else {
    details =
        JournalDetailsBuilder()
            .Add(success_details_key, GetToolExecutionResultMessage(result))
            .Build();
  }

  journal.Log(url, task_id, event_name, std::move(details));
}

std::unique_ptr<AggregatedJournal::PendingAsyncEntry> StartAsyncJournalEntry(
    AggregatedJournal& journal,
    const GURL& url,
    ActorTaskId task_id,
    const std::string& tool_name,
    const std::string& event_name,
    std::string_view tool_debug_string) {
  return journal.CreatePendingAsyncEntry(
      url, task_id, journal.AllocateDynamicTrackUUID(),
      base::StringPrintf("%s: %s", event_name.c_str(), tool_name.c_str()),
      JournalDetailsBuilder().Add("tool", tool_debug_string).Build());
}

void EndAsyncJournalEntry(AggregatedJournal::PendingAsyncEntry* entry,
                          const ToolExecutionResult& result) {
  CHECK(entry);

  JournalDetailsBuilder builder;
  if (result.IsOk()) {
    builder.Add("result", "success");
  } else {
    builder.AddError(GetToolExecutionResultMessage(result));
  }

  entry->EndEntry(std::move(builder).Build());
}

}  // namespace actor
