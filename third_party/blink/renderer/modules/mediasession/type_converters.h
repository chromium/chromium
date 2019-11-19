// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASESSION_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASESSION_TYPE_CONVERTERS_H_

#include "third_party/blink/public/mojom/mediasession/media_session.mojom-blink.h"
#include "third_party/blink/renderer/modules/mediasession/media_position_state.h"
#include "third_party/blink/renderer/modules/mediasession/media_session_action_details.h"
#include "third_party/blink/renderer/modules/mediasession/media_session_seek_to_action_details.h"

namespace mojo {

template <>
struct TypeConverter<const blink::MediaSessionActionDetails*,
                     blink::mojom::blink::MediaSessionActionDetailsPtr> {
  static const blink::MediaSessionActionDetails* ConvertWithActionName(
      const blink::mojom::blink::MediaSessionActionDetailsPtr& details,
      const WTF::AtomicString& action_name);
};

template <>
struct TypeConverter<blink::MediaSessionSeekToActionDetails*,
                     blink::mojom::blink::MediaSessionActionDetailsPtr> {
  static blink::MediaSessionSeekToActionDetails* Convert(
      const blink::mojom::blink::MediaSessionActionDetailsPtr& details);
};

template <>
struct TypeConverter<media_session::mojom::blink::MediaPositionPtr,
                     blink::MediaPositionState*> {
  static media_session::mojom::blink::MediaPositionPtr Convert(
      const blink::MediaPositionState* position);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASESSION_TYPE_CONVERTERS_H_
