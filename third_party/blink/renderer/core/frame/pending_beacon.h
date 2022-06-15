// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"

namespace blink {

class BeaconOptions;

// Implementation of the Pending Beacon API.
// https://github.com/darrenw/docs/blob/main/explainers/beacon_api.md
class CORE_EXPORT PendingBeacon : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PendingBeacon* Create(ExecutionContext* context,
                               const String& targetURL);

  static PendingBeacon* Create(ExecutionContext* context,
                               const String& targetURL,
                               BeaconOptions* options);

  explicit PendingBeacon(ExecutionContext* context);

  void setUrl(const String& url);
  const String& url() { return url_; }

  void setPageHideTimeout(int32_t pageHideTimeout);
  int32_t pageHideTimeout() { return page_hide_timeout_; }

  void setMethod(const String& method);
  const String& method() { return method_; }

  const String& state() { return state_; }

  void deactivate();

  void setData(const V8UnionReadableStreamOrXMLHttpRequestBodyInit* data);

  void sendNow();

  void Trace(Visitor*) const override;

 private:
  int32_t page_hide_timeout_;
  String url_;
  String method_;
  String state_;
  HeapMojoRemote<mojom::blink::PendingBeacon> remote_;
};

}  // namespace blink

#endif  // #define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PENDING_BEACON_H_
