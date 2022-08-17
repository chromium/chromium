// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_

#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class BeaconData;
class ExceptionState;
class ExecutionContext;

// Implementation of the PendingBeacon API.
// https://github.com/WICG/unload-beacon/blob/main/README.md
class CORE_EXPORT PendingBeacon : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  const String& url() { return url_; }

  int32_t backgroundTimeout() const {
    return base::checked_cast<int32_t>(background_timeout_.InMilliseconds());
  }
  void setBackgroundTimeout(int32_t background_timeout);

  int32_t timeout() const {
    return base::checked_cast<int32_t>(timeout_.InMilliseconds());
  }
  void setTimeout(int32_t timeout);

  const String& method() const { return method_; }

  bool pending() const { return pending_; }

  void deactivate();

  void sendNow();

  void Trace(Visitor*) const override;

 protected:
  explicit PendingBeacon(ExecutionContext* context,
                         const String& url,
                         const String& method,
                         int32_t background_timeout,
                         int32_t timeout);

  void SetURLInternal(const String& url);
  void SetDataInternal(const BeaconData& beacon_data,
                       ExceptionState& exception_state);

 private:
  Member<ExecutionContext> ec_;
  HeapMojoRemote<mojom::blink::PendingBeacon> remote_;
  String url_;
  const String method_;
  base::TimeDelta background_timeout_;
  base::TimeDelta timeout_;
  bool pending_ = true;
};

}  // namespace blink

#endif  // #define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
