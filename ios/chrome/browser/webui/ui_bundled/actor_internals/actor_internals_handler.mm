// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/actor_internals/actor_internals_handler.h"

#import "base/containers/flat_map.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/journal_details_builder.h"

ActorInternalsHandler::ActorInternalsHandler(
    actor::AggregatedJournal* journal,
    mojo::PendingRemote<actor_internals::mojom::Page> page,
    mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver) {
  handler_ = std::make_unique<actor_internals::ActorInternalsHandler>(
      std::move(page), std::move(receiver), this);

  if (journal) {
    journal_observation_.Observe(journal);

    // Push any logs that are already in the journal.
    for (auto it = journal->Items(); it; ++it) {
      const std::unique_ptr<actor::AggregatedJournal::Entry>* item = *it;
      if (item && *item) {
        WillAddJournalEntry(**item);
      }
    }
  }
}

ActorInternalsHandler::~ActorInternalsHandler() = default;

void ActorInternalsHandler::StartLogging() {
  // TODO(crbug.com/382311179): Support trace log serialization on iOS.
  // This should present a file picker (e.g. UIDocumentPickerViewController) to
  // choose where to save the trace, and serialize the logs directly to that
  // location.
}

void ActorInternalsHandler::StopLogging() {
  // TODO(crbug.com/382311179): Support stopping trace log serialization.
}

void ActorInternalsHandler::WillAddJournalEntry(
    const actor::AggregatedJournal::Entry& entry) {
  base::flat_map<std::string, std::string> details;
  for (const auto& detail : entry.data->details) {
    details[detail->key] = detail->value;
  }

  handler_->OnJournalEntryAdded(actor_internals::mojom::JournalEntry::New(
      entry.url, entry.data->event,
      std::string(actor::JournalEntryTypeToString(entry.data->type)),
      std::move(details), entry.data->timestamp, entry.data->task_id.value(),
      actor::TrackToString(entry.data->track_uuid, entry.data->task_id),
      entry.screenshot));
}
