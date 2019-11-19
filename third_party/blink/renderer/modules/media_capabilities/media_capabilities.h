// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_

#include "media/base/video_codecs.h"  // for media::VideoCodecProfile
#include "media/mojo/mojom/video_decode_perf_history.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/modules/media_capabilities/video_configuration.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExecutionContext;
class MediaDecodingConfiguration;
class MediaEncodingConfiguration;
class MediaKeySystemAccess;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT MediaCapabilities final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MediaCapabilities();

  ScriptPromise decodingInfo(ScriptState*, const MediaDecodingConfiguration*);
  ScriptPromise encodingInfo(ScriptState*, const MediaEncodingConfiguration*);

 private:
  // Binds to the VideoDecodePerfHistory service. Returns whether it was
  // successful. Returns true if it was already bound.
  bool EnsureService(ExecutionContext*);

  ScriptPromise GetEmeSupport(ScriptState*,
                              media::VideoCodecProfile,
                              const MediaDecodingConfiguration*);
  void GetPerfInfo(media::VideoCodecProfile,
                   const VideoConfiguration*,
                   ScriptPromiseResolver*,
                   MediaKeySystemAccess*);

  void OnPerfInfo(ScriptPromiseResolver*,
                  MediaKeySystemAccess*,
                  bool is_smooth,
                  bool is_power_efficient);

  mojo::Remote<media::mojom::blink::VideoDecodePerfHistory>
      decode_history_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CAPABILITIES_MEDIA_CAPABILITIES_H_
