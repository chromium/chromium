// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_DEVICE_SERVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_DEVICE_SERVICE_H_

#include "third_party/blink/public/mojom/device/device.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class ExecutionContext;
class ScriptPromiseResolver;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT DeviceService final : public ScriptWrappable,
                                           public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Web-based getter for navigator.device.
  static DeviceService* device(Navigator&);

  ExecutionContext* GetExecutionContext() const;

  explicit DeviceService(Navigator&);
  DeviceService(const DeviceService&) = delete;
  DeviceService& operator=(const DeviceService&) = delete;

  void Trace(Visitor*) const override;

 private:
  // Lazily binds mojo interface.
  mojom::blink::DeviceAPIService* GetService();

  void OnServiceConnectionError();

  HeapMojoRemote<mojom::blink::DeviceAPIService,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      device_api_service_;
  HeapHashSet<Member<ScriptPromiseResolver>> pending_promises_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICE_DEVICE_SERVICE_H_
