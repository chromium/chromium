// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_set.h"

#include "base/functional/bind.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/screen_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/screen_details/screen_detailed.h"
#include "third_party/blink/renderer/modules/screen_details/screen_details.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/display/types/display_constants.h"

namespace blink {

namespace {

ScreenDetailed* FindScreenDetailedByDisplayId(
    ScreenDetails* screen_details,
    std::optional<int64_t> display_id) {
  if (display_id == display::kInvalidDisplayId) {
    return nullptr;
  }

  auto screen_iterator = base::ranges::find_if(
      screen_details->screens(),
      [display_id](const ScreenDetailed* screen_detailed) {
        return *display_id == screen_detailed->DisplayId();
      });

  return (screen_iterator != screen_details->screens().end()) ? *screen_iterator
                                                              : nullptr;
}

}  // namespace

MediaStreamSet* MediaStreamSet::Create(
    ExecutionContext* context,
    const MediaStreamDescriptorVector& stream_descriptors,
    UserMediaRequestType request_type,
    MediaStreamSetInitializedCallback callback) {
  DCHECK(IsMainThread());

  return MakeGarbageCollected<MediaStreamSet>(
      context, stream_descriptors, request_type, std::move(callback));
}

MediaStreamSet::MediaStreamSet(
    ExecutionContext* context,
    const MediaStreamDescriptorVector& stream_descriptors,
    UserMediaRequestType request_type,
    MediaStreamSetInitializedCallback callback)
    : ExecutionContextClient(context),
      media_streams_to_initialize_count_(stream_descriptors.size()),
      media_streams_initialized_callback_(std::move(callback)) {
  DCHECK(IsMainThread());

  if (request_type == UserMediaRequestType::kAllScreensMedia) {
    InitializeGetAllScreensMediaStreams(context, stream_descriptors);
    return;
  }

  if (stream_descriptors.empty()) {
    // No streams -> all streams are initialized, meaning the set
    // itself is fully initialized.
    context->GetTaskRunner(TaskType::kInternalMedia)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&MediaStreamSet::OnMediaStreamSetInitialized,
                                 WrapPersistent(this)));
    return;
  }

  // The set will be initialized when all of its streams are initialized.
  // When the last stream is initialized, its callback will trigger
  // a call to OnMediaStreamSetInitialized.
  for (WTF::wtf_size_t stream_index = 0;
       stream_index < stream_descriptors.size(); ++stream_index) {
    MediaStream::Create(context, stream_descriptors[stream_index],
                        /*track=*/nullptr,
                        WTF::BindOnce(&MediaStreamSet::OnMediaStreamInitialized,
                                      WrapPersistent(this)));
  }
}

void MediaStreamSet::Trace(Visitor* visitor) const {
  visitor->Trace(initialized_media_streams_);
  ExecutionContextClient::Trace(visitor);
}

void MediaStreamSet::InitializeGetAllScreensMediaStreams(
    ExecutionContext* context,
    const MediaStreamDescriptorVector& stream_descriptors) {
  DCHECK(IsMainThread());

  LocalDOMWindow* const window = To<LocalDOMWindow>(context);
  DCHECK(window);

  // TODO(crbug.com/1358949): Move the generation of the |ScreenDetails| object
  // next to the generation of the descriptors and store them as members to
  // avoid race conditions. Further, match the getAllScreensMedia API and the
  // window placement API by unique IDs instead of assuming the same order.
  ScreenDetails* const screen_details =
      MakeGarbageCollected<ScreenDetails>(window);
  const bool screen_details_match_descriptors =
      screen_details->screens().size() == stream_descriptors.size();
  for (WTF::wtf_size_t stream_index = 0;
       stream_index < stream_descriptors.size(); ++stream_index) {
    MediaStreamDescriptor* const descriptor = stream_descriptors[stream_index];
    DCHECK_EQ(1u, descriptor->NumberOfVideoComponents());

    ScreenDetailed* screen = FindScreenDetailedByDisplayId(
        screen_details,
        descriptor->VideoComponent(0u)->Source()->GetDisplayId());

    MediaStreamTrack* video_track =
        MakeGarbageCollected<ScreenCaptureMediaStreamTrack>(
            context, descriptor->VideoComponent(0u),
            screen_details_match_descriptors ? screen_details : nullptr,
            screen);
    initialized_media_streams_.push_back(
        MediaStream::Create(context, descriptor, {}, {video_track}));
  }
  context->GetTaskRunner(TaskType::kInternalMedia)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(&MediaStreamSet::OnMediaStreamSetInitialized,
                               WrapPersistent(this)));
}

void MediaStreamSet::OnMediaStreamSetInitialized() {
  DCHECK(IsMainThread());

  std::move(std::move(media_streams_initialized_callback_))
      .Run(initialized_media_streams_);
}

// TODO(crbug.com/1300883): Clean up other streams if one stream capture
// results in an error. This is only required for getAllScreensMedia.
// Currently existing functionality generates only one stream which is not
// affected by this change.
void MediaStreamSet::OnMediaStreamInitialized(
    MediaStream* initialized_media_stream) {
  DCHECK(IsMainThread());
  DCHECK_LT(initialized_media_streams_.size(),
            media_streams_to_initialize_count_);

  initialized_media_streams_.push_back(initialized_media_stream);
  if (initialized_media_streams_.size() == media_streams_to_initialize_count_) {
    OnMediaStreamSetInitialized();
  }
}

}  // namespace blink
