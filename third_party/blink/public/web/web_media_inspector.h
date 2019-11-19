// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEDIA_INSPECTOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEDIA_INSPECTOR_H_

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

// These types should look exactly like the types defined in
// browser_protocol.pdl and should eventually replace the types in
// //src/media/base
struct InspectorPlayerProperty {
  WebString name;
  base::Optional<WebString> value;
};
using InspectorPlayerProperties = WebVector<InspectorPlayerProperty>;

struct InspectorPlayerEvent {
  enum InspectorPlayerEventType { PLAYBACK_EVENT, SYSTEM_EVENT, MESSAGE_EVENT };
  InspectorPlayerEventType type;
  base::TimeTicks timestamp;
  WebString key;
  WebString value;
};
using InspectorPlayerEvents = WebVector<InspectorPlayerEvent>;

class MediaInspectorContext {
 public:
  virtual WebString CreatePlayer() = 0;

  // These methods DCHECK if the player id is invalid.
  virtual void NotifyPlayerEvents(WebString, InspectorPlayerEvents) = 0;
  virtual void SetPlayerProperties(WebString, InspectorPlayerProperties) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEDIA_INSPECTOR_H_
