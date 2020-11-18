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
// browser_protocol.pdl.
struct InspectorPlayerMessage {
  enum class Level { kError, kWarning, kInfo, kDebug };
  Level level;
  WebString message;
};
using InspectorPlayerMessages = WebVector<InspectorPlayerMessage>;

struct InspectorPlayerProperty {
  WebString name;
  WebString value;
};
using InspectorPlayerProperties = WebVector<InspectorPlayerProperty>;

struct InspectorPlayerEvent {
  base::TimeTicks timestamp;
  WebString value;
};
using InspectorPlayerEvents = WebVector<InspectorPlayerEvent>;

struct InspectorPlayerError {
  enum class Type { kPipelineError, kMediaStatus };
  Type type;
  WebString errorCode;
};
using InspectorPlayerErrors = WebVector<InspectorPlayerError>;

class MediaInspectorContext {
 public:
  virtual WebString CreatePlayer() = 0;

  // These methods DCHECK if the player id is invalid.
  virtual void NotifyPlayerEvents(WebString player_id,
                                  const InspectorPlayerEvents&) = 0;
  virtual void NotifyPlayerErrors(WebString player_id,
                                  const InspectorPlayerErrors&) = 0;
  virtual void NotifyPlayerMessages(WebString player_id,
                                    const InspectorPlayerMessages&) = 0;
  virtual void SetPlayerProperties(WebString player_id,
                                   const InspectorPlayerProperties&) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MEDIA_INSPECTOR_H_
