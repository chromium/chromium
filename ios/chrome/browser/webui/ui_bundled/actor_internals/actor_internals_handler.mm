// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/actor_internals/actor_internals_handler.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/containers/flat_map.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/notreached.h"
#import "base/path_service.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/aggregated_journal_file_serializer.h"
#import "components/actor/core/journal_details_builder.h"
#import "components/actor/public/mojom/actor_internals.mojom.h"
#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/web/public/web_state.h"

namespace {

// Deletes `trace_file` on the thread pool.
void PostDeleteTraceFile(const base::FilePath& trace_file) {
  base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                             base::GetDeleteFileCallback(trace_file));
}

}  // namespace

ActorInternalsHandler::ActorInternalsHandler(
    web::WebState* web_state,
    actor::AggregatedJournal* journal,
    mojo::PendingRemote<actor_internals::mojom::Page> page,
    mojo::PendingReceiver<actor_internals::mojom::PageHandler> receiver)
    : web_state_(web_state ? web_state->GetWeakPtr() : nullptr) {
  handler_ = std::make_unique<actor_internals::ActorInternalsHandler>(
      std::move(page), std::move(receiver), this);

  if (journal) {
    journal_ = journal;
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

ActorInternalsHandler::~ActorInternalsHandler() {
  UIViewController* presenting_view_controller =
      activity_view_controller_.presentingViewController;
  if (presenting_view_controller) {
    [presenting_view_controller dismissViewControllerAnimated:NO
                                                   completion:nil];
  }
  CloseTraceFile(base::BindOnce(&PostDeleteTraceFile, current_trace_file_));
}

// This starts logging to disk and continues until StopLogging is called. A
// share menu is then shown, allowing the user to choose where to save the file.
void ActorInternalsHandler::StartLogging() {
  if (!journal_) {
    return;
  }
  if (file_journal_serializer_) {
    return;
  }

  base::FilePath temp_dir;
  if (!base::PathService::Get(base::DIR_TEMP, &temp_dir)) {
    return;
  }

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  current_trace_file_ = temp_dir.AppendASCII(base::StringPrintf(
      "actor_trace_%04d_%02d_%02d_%02d_%02d_%02d_%03d.pftrace", exploded.year,
      exploded.month, exploded.day_of_month, exploded.hour, exploded.minute,
      exploded.second, exploded.millisecond));

  // Initialize the serializer asynchronously to perform blocking file creation
  // on a background thread pool instead of the main thread.
  file_journal_serializer_ =
      std::make_unique<actor::AggregatedJournalFileSerializer>(*journal_);
  file_journal_serializer_->Init(
      current_trace_file_,
      base::BindOnce(&ActorInternalsHandler::TraceFileInitDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActorInternalsHandler::StopLogging() {
  if (!file_journal_serializer_) {
    return;
  }

  base::FilePath trace_file = std::move(current_trace_file_);
  current_trace_file_.clear();
  CloseTraceFile(base::BindOnce(
      [](base::WeakPtr<ActorInternalsHandler> handler,
         base::FilePath trace_file) {
        if (handler) {
          handler->ShareTraceFile(trace_file);
        } else {
          // Tab was closed before the file was closed. Delete the orphaned
          // file.
          PostDeleteTraceFile(trace_file);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(trace_file)));
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
      entry.screenshot, /*iframe_screenshot=*/std::nullopt));
}

void ActorInternalsHandler::TraceFileInitDone(bool success) {
  if (!success) {
    // If initialization fails, clean up the temporary file and reset state.
    CloseTraceFile(base::BindOnce(&PostDeleteTraceFile, current_trace_file_));
    current_trace_file_.clear();
  }
}

void ActorInternalsHandler::ShareTraceFile(base::FilePath trace_file) {
  // Displays the iOS share sheet to share the trace file.
  NSURL* file_url = base::apple::FilePathToNSURL(trace_file);
  activity_view_controller_ =
      [[UIActivityViewController alloc] initWithActivityItems:@[ file_url ]
                                        applicationActivities:nil];

  base::WeakPtr<ActorInternalsHandler> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  // Set up a completion handler to delete the temporary trace file from the
  // filesystem on the background thread pool once the sharing session finishes.
  activity_view_controller_.completionWithItemsHandler =
      ^(UIActivityType activity_type, BOOL completed, NSArray* returned_items,
        NSError* activity_error) {
        // Capture file_url to avoid capturing C++ objects in Objective-C
        // blocks.
        PostDeleteTraceFile(base::apple::NSURLToFilePath(file_url));
        if (weak_this) {
          weak_this->activity_view_controller_ = nil;
        }
      };

  UIWindow* window = nil;
  UIView* source_view = nil;
  web::WebState* web_state = web_state_.get();
  if (web_state && web_state->GetView()) {
    source_view = web_state->GetView();
    window = web_state->GetView().window;
  }
  if (!window) {
    PostDeleteTraceFile(trace_file);
    return;
  }

  UIViewController* top_view_controller = window.rootViewController;
  if (!top_view_controller || top_view_controller.presentedViewController) {
    // If a view controller is already presented, or if there is no root view
    // controller, do not present the share sheet.
    PostDeleteTraceFile(trace_file);
    return;
  }
  source_view = top_view_controller.view;
  CGRect source_rect =
      CGRectMake(source_view.bounds.size.width / 2.0, 10.0, 1.0, 1.0);

  activity_view_controller_.popoverPresentationController.sourceView =
      source_view;
  activity_view_controller_.popoverPresentationController.sourceRect =
      source_rect;
  activity_view_controller_.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;
  [top_view_controller presentViewController:activity_view_controller_
                                    animated:YES
                                  completion:nil];
}

void ActorInternalsHandler::CloseTraceFile(base::OnceClosure on_file_closed) {
  if (!file_journal_serializer_) {
    return;
  }
  file_journal_serializer_->Shutdown(std::move(on_file_closed));
  // The serializer holds a `SafeRef` to the profile-owned journal, so it must
  // not be handed to a posted task: at termination the main thread no longer
  // runs tasks and pending ones are destroyed after the profile, which trips
  // the `SafeRef` in the serializer's destructor. Destroying it right away is
  // safe since `SequenceBound::Reset()` enqueues the `FileWriter` destruction
  // behind the `Shutdown()` call posted above.
  file_journal_serializer_.reset();
}
