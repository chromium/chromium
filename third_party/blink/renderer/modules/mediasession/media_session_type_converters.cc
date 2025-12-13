// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_session_type_converters.h"

namespace mojo {

using blink::V8MediaSessionEnterPictureInPictureReason;
using blink::mojom::blink::MediaSessionEnterPictureInPictureReason;

blink::V8MediaSessionEnterPictureInPictureReason::Enum
TypeConverter<V8MediaSessionEnterPictureInPictureReason::Enum,
              MediaSessionEnterPictureInPictureReason>::
    Convert(MediaSessionEnterPictureInPictureReason mojom_reason) {
  switch (mojom_reason) {
    case MediaSessionEnterPictureInPictureReason::kOther:
      return V8MediaSessionEnterPictureInPictureReason::Enum::kOther;
    case MediaSessionEnterPictureInPictureReason::kUserAction:
      return V8MediaSessionEnterPictureInPictureReason::Enum::kUseraction;
    case MediaSessionEnterPictureInPictureReason::kContentOccluded:
      return V8MediaSessionEnterPictureInPictureReason::Enum::kContentoccluded;
  }
  NOTREACHED();
}

const blink::MediaSessionActionDetails*
TypeConverter<const blink::MediaSessionActionDetails*,
              blink::mojom::blink::MediaSessionActionDetailsPtr>::
    ConvertWithV8Action(
        const blink::mojom::blink::MediaSessionActionDetailsPtr& details,
        blink::V8MediaSessionAction::Enum action) {
  blink::MediaSessionActionDetails* blink_details;

  if (details && details->is_seek_to()) {
    blink_details = TypeConverter<
        blink::MediaSessionSeekToActionDetails*,
        blink::mojom::blink::MediaSessionActionDetailsPtr>::Convert(details);
  } else if (details && details->is_enter_picture_in_picture()) {
    blink_details =
        mojo::ConvertTo<blink::MediaSessionEnterPictureInPictureActionDetails*>(
            details);
  } else {
    DCHECK(!details);
    blink_details = blink::MediaSessionActionDetails::Create();
  }

  blink_details->setAction(action);

  return blink_details;
}

blink::MediaSessionSeekToActionDetails*
TypeConverter<blink::MediaSessionSeekToActionDetails*,
              blink::mojom::blink::MediaSessionActionDetailsPtr>::
    Convert(const blink::mojom::blink::MediaSessionActionDetailsPtr& details) {
  auto* blink_details = blink::MediaSessionSeekToActionDetails::Create();
  blink_details->setSeekTime(details->get_seek_to()->seek_time.InSecondsF());
  blink_details->setFastSeek(details->get_seek_to()->fast_seek);
  return blink_details;
}

blink::MediaSessionEnterPictureInPictureActionDetails*
TypeConverter<blink::MediaSessionEnterPictureInPictureActionDetails*,
              blink::mojom::blink::MediaSessionActionDetailsPtr>::
    Convert(const blink::mojom::blink::MediaSessionActionDetailsPtr& details) {
  auto* blink_details =
      blink::MediaSessionEnterPictureInPictureActionDetails::Create();
  blink_details->setEnterPictureInPictureReason(
      mojo::ConvertTo<V8MediaSessionEnterPictureInPictureReason::Enum>(
          details->get_enter_picture_in_picture()->reason));
  return blink_details;
}

media_session::mojom::blink::MediaPositionPtr TypeConverter<
    media_session::mojom::blink::MediaPositionPtr,
    blink::MediaPositionState*>::Convert(const blink::MediaPositionState*
                                             position) {
  return media_session::mojom::blink::MediaPosition::New(
      position->hasPlaybackRate() ? position->playbackRate() : 1.0,
      position->duration() == std::numeric_limits<double>::infinity()
          ? base::TimeDelta::Max()
          : base::Seconds(position->duration()),
      position->hasPosition() ? base::Seconds(position->position())
                              : base::TimeDelta(),
      base::TimeTicks::Now());
}

}  // namespace mojo
