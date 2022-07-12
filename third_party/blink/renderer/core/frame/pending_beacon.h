// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_

#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class BeaconData;
class BeaconOptions;

// Implementation of the Pending Beacon API.
// https://github.com/WICG/unload-beacon/blob/main/README.md
class CORE_EXPORT PendingBeacon : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PendingBeacon* Create(ExecutionContext* context,
                               const String& targetURL);

  static PendingBeacon* Create(ExecutionContext* context,
                               const String& targetURL,
                               BeaconOptions* options);

  explicit PendingBeacon(ExecutionContext* context,
                         String url,
                         String method,
                         int32_t page_hide_timeout);

  const String& url() { return url_; }

  int32_t pageHideTimeout() { return page_hide_timeout_; }

  const String& method() { return method_; }

  bool isPending() { return is_pending_; }

  void deactivate();

  void setData(const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data);

  void sendNow();

  void Trace(Visitor*) const override;

 private:
  void SetDataInternal(const BeaconData& beacon_data);

  HeapMojoRemote<mojom::blink::PendingBeacon> remote_;
  const String url_;
  const String method_;
  const int32_t page_hide_timeout_;
  bool is_pending_ = true;
};

}  // namespace blink

#endif  // #define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
