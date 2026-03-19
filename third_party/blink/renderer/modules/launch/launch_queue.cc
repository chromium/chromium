// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/launch_queue.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_launch_consumer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"

namespace blink {

const char LaunchQueue::kSupplementName[] = "LaunchQueue";

LaunchQueue::LaunchQueue(LocalDOMWindow& window)
    : Supplement(window), receiver_(this, &window) {}

LaunchQueue::~LaunchQueue() = default;

// static
LaunchQueue* LaunchQueue::launchQueue(LocalDOMWindow& window) {
  auto* supplement = Supplement<LocalDOMWindow>::From<LaunchQueue>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<LaunchQueue>(window);
    ProvideTo(window, supplement);
  }
  return supplement;
}

// static
void LaunchQueue::BindReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::WebLaunchService> receiver) {
  DCHECK(frame);
  auto* launch_queue = launchQueue(*frame->DomWindow());
  // This interface only has a single method with no reply. The calling side
  // doesn't keep this around, so it is re-requested on demand every time;
  // however, there should never be multiple callers bound at a time.
  launch_queue->receiver_.reset();
  launch_queue->receiver_.Bind(
      std::move(receiver), frame->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void LaunchQueue::EnqueueLaunchParams(
    const KURL& launch_url,
    base::TimeTicks time_navigation_started_in_browser,
    bool navigation_started,
    ::blink::Vector<::blink::mojom::blink::FileSystemAccessEntryPtr> files) {
  CHECK(launch_url.IsValid());

  HeapVector<Member<FileSystemHandle>> files_vector;
  if (files.size() > 0) {
    UseCounter::Count(GetSupplementable(), WebFeature::kFileHandlingLaunch);
  }
  for (auto& entry : files) {
    files_vector.push_back(FileSystemHandle::CreateFromMojoEntry(
        std::move(entry), GetSupplementable()));
  }
  LaunchParams* params = MakeGarbageCollected<LaunchParams>(
      launch_url, time_navigation_started_in_browser, navigation_started,
      files_vector);

  if (!consumer_) {
    unconsumed_launch_params_.push_back(params);
    return;
  }

  InvokeConsumerWithParams(params);
}

void LaunchQueue::setConsumer(V8LaunchConsumer* consumer) {
  consumer_ = consumer;

  // Consume all launch params now we have a consumer.
  while (!unconsumed_launch_params_.empty()) {
    // Get the first launch params and the queue and remove it before invoking
    // the consumer, in case the consumer calls |setConsumer|. Each launchParams
    // should be consumed by the most recently set consumer.
    LaunchParams* params = unconsumed_launch_params_.at(0);
    unconsumed_launch_params_.EraseAt(0);
    InvokeConsumerWithParams(params);
  }
}

void LaunchQueue::InvokeConsumerWithParams(LaunchParams* params) {
  if (!params->time_navigation_started_in_browser().is_null()) {
    // Measure how long it took the launch params to be enqueued after the
    // browser process receives the navigation request that created these
    // params.
    base::TimeDelta time_to_navigate =
        base::TimeTicks::Now() - params->time_navigation_started_in_browser();
    base::UmaHistogramMediumTimes(
        "Webapp.NavigationCapturing.LaunchParamsConsumedTime",
        time_to_navigate);

    std::string histogram_variant =
        params->navigation_started() ? ".WithNavigation" : ".WithoutNavigation";
    std::string histogram_name =
        base::StrCat({"Webapp.NavigationCapturing.LaunchParamsConsumedTime",
                      histogram_variant});
    base::UmaHistogramMediumTimes(histogram_name, time_to_navigate);
  }

  consumer_->InvokeAndReportException(nullptr, params);
}

void LaunchQueue::Trace(Visitor* visitor) const {
  visitor->Trace(unconsumed_launch_params_);
  visitor->Trace(consumer_);
  visitor->Trace(receiver_);
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
