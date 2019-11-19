// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASESSION_MEDIA_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASESSION_MEDIA_SESSION_H_

#include <memory>
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class ExceptionState;
class MediaMetadata;
class MediaPositionState;
class V8MediaSessionActionHandler;

class MODULES_EXPORT MediaSession final
    : public ScriptWrappable,
      public ContextClient,
      blink::mojom::blink::MediaSessionClient {
  USING_GARBAGE_COLLECTED_MIXIN(MediaSession);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(MediaSession, Dispose);

 public:
  explicit MediaSession(ExecutionContext*);

  void Dispose();

  void setPlaybackState(const String&);
  String playbackState();

  void setMetadata(MediaMetadata*);
  MediaMetadata* metadata() const;

  void setActionHandler(const String& action,
                        V8MediaSessionActionHandler*,
                        ExceptionState&);

  void setPositionState(MediaPositionState*, ExceptionState&);

  // Called by the MediaMetadata owned by |this| when it has updates. Also used
  // internally when a new MediaMetadata object is set.
  void OnMetadataChanged();

  void Trace(blink::Visitor*) override;

 private:
  friend class V8MediaSession;
  friend class MediaSessionTest;

  enum class ActionChangeType {
    kActionEnabled,
    kActionDisabled,
  };

  void NotifyActionChange(const String& action, ActionChangeType);

  void RecalculatePositionState(bool notify);

  // blink::mojom::blink::MediaSessionClient implementation.
  void DidReceiveAction(media_session::mojom::blink::MediaSessionAction,
                        mojom::blink::MediaSessionActionDetailsPtr) override;

  // Returns null when the ExecutionContext is not document.
  mojom::blink::MediaSessionService* GetService();

  mojom::blink::MediaSessionPlaybackState playback_state_;
  media_session::mojom::blink::MediaPositionPtr position_state_;
  double declared_playback_rate_ = 0.0;
  Member<MediaMetadata> metadata_;
  HeapHashMap<String, Member<V8MediaSessionActionHandler>> action_handlers_;
  mojo::Remote<mojom::blink::MediaSessionService> service_;
  mojo::Receiver<blink::mojom::blink::MediaSessionClient> client_receiver_{
      this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASESSION_MEDIA_SESSION_H_
