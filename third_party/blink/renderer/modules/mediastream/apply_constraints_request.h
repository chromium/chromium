// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_APPLY_CONSTRAINTS_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_APPLY_CONSTRAINTS_REQUEST_H_

#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ScriptPromiseResolver;

class MODULES_EXPORT ApplyConstraintsRequest final
    : public GarbageCollected<ApplyConstraintsRequest> {
 public:
  static ApplyConstraintsRequest* CreateForTesting(const WebMediaStreamTrack&,
                                                   const WebMediaConstraints&);

  ApplyConstraintsRequest(const WebMediaStreamTrack&,
                          const WebMediaConstraints&,
                          ScriptPromiseResolver*);

  WebMediaStreamTrack Track() const;
  WebMediaConstraints Constraints() const;

  void RequestSucceeded();
  void RequestFailed(const String& constraint, const String& message);

  virtual void Trace(blink::Visitor*);

 private:
  WebMediaStreamTrack track_;
  WebMediaConstraints constraints_;
  Member<ScriptPromiseResolver> resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_APPLY_CONSTRAINTS_REQUEST_H_
