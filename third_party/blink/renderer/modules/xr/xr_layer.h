// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_H_

#include <optional>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

class XrLayerClient;
class XRSession;
struct XRSharedImageData;

enum class XRLayerType {
  kWebGLLayer,
  kProjectionLayer,
  kQuadLayer,
  kCylinderLayer,
  kEquirectLayer,
  kCubeLayer
};

class XRLayer : public EventTarget {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRLayer(XRSession*);
  ~XRLayer() override = default;

  XRSession* session() const { return session_.Get(); }

  virtual void OnFrameStart() = 0;
  virtual void OnFrameEnd() = 0;
  virtual void OnResize() = 0;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  device::LayerId layer_id() const { return layer_id_; }
  virtual XRLayerType LayerType() const = 0;

  const XRSharedImageData& SharedImage() const;
  bool HasSharedImage() const;

  void SetModified(bool modified);
  bool IsModified() const;

  void SetNeedsRedraw(bool needsRedraw);
  bool needsRedraw() const;
  void MaybeDispatchRedrawEvent();

  // Mojom backend.
  void CreateLayerBackend();
  bool IsBackendActive() const;
  void DestroyBackend();

  virtual XrLayerClient* LayerClient() = 0;

  void Trace(Visitor*) const override;

 protected:
  virtual bool IsRedrawEventSupported() const;
  virtual device::mojom::blink::XRCompositionLayerDataPtr CreateLayerData()
      const = 0;

 private:
  void OnBackendLayerCreated(
      device::mojom::blink::CreateCompositionLayerResult result);

  const Member<XRSession> session_;
  const device::LayerId layer_id_;
  bool is_modified_{false};

  bool is_backend_active_{false};
  bool needs_redraw_{false};
  bool should_dispatch_redraw_event_{false};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_H_
