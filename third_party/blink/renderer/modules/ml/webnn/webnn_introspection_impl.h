// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_WEBNN_INTROSPECTION_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_WEBNN_INTROSPECTION_IMPL_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"
#include "third_party/blink/public/mojom/webnn/webnn_introspection.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT WebNNIntrospectionImpl
    : public blink::mojom::blink::WebNNIntrospection {
 public:
  static WebNNIntrospectionImpl& GetInstance();

  static void BindReceiver(
      mojo::PendingReceiver<blink::mojom::blink::WebNNIntrospection> receiver);

  WebNNIntrospectionImpl(const WebNNIntrospectionImpl&) = delete;
  WebNNIntrospectionImpl& operator=(const WebNNIntrospectionImpl&) = delete;

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  bool IsGraphRecordingEnabled() const;

  void SetClient(
      ::mojo::PendingRemote<blink::mojom::blink::WebNNIntrospectionClient>
          client) override;

  void OnGraphRecorded(::mojo_base::BigBuffer json_data) const;
#endif

 private:
  WebNNIntrospectionImpl() : receiver_(this) {}

  mojo::Receiver<blink::mojom::blink::WebNNIntrospection> receiver_;

  mojo::Remote<blink::mojom::blink::WebNNIntrospectionClient> client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_WEBNN_INTROSPECTION_IMPL_H_
