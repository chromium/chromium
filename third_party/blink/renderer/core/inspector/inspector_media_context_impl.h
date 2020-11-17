// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_CONTEXT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_CONTEXT_IMPL_H_

#include "third_party/blink/public/web/web_media_inspector.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class ExecutionContext;

struct MediaPlayer final : public GarbageCollected<MediaPlayer> {
  void Trace(Visitor*) const {}

  WebString player_id;
  Vector<InspectorPlayerError> errors;
  Vector<InspectorPlayerEvent> events;
  Vector<InspectorPlayerMessage> messages;
  HashMap<String, InspectorPlayerProperty> properties;
};

class CORE_EXPORT MediaInspectorContextImpl final
    : public GarbageCollected<MediaInspectorContextImpl>,
      public Supplement<ExecutionContext>,
      public MediaInspectorContext {
 public:
  static const char kSupplementName[];

  static MediaInspectorContextImpl* From(ExecutionContext&);

  explicit MediaInspectorContextImpl(ExecutionContext&);

  // MediaInspectorContext methods.
  WebString CreatePlayer() override;
  void NotifyPlayerErrors(WebString playerId,
                          const InspectorPlayerErrors&) override;
  void NotifyPlayerEvents(WebString playerId,
                          const InspectorPlayerEvents&) override;
  void NotifyPlayerMessages(WebString playerId,
                            const InspectorPlayerMessages&) override;
  void SetPlayerProperties(WebString playerId,
                           const InspectorPlayerProperties&) override;

  // GarbageCollected methods.
  void Trace(Visitor*) const override;

  Vector<WebString> AllPlayerIds();
  const MediaPlayer& MediaPlayerFromId(const WebString&);

 private:
  HeapHashMap<String, Member<MediaPlayer>> players_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_CONTEXT_IMPL_H_
