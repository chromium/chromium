// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_WEB_LAUNCH_SERVICE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_WEB_LAUNCH_SERVICE_IMPL_H_

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LocalFrame;
class LocalDOMWindow;

// Implementation of WebLaunchService, to allow the renderer to receive launch
// parameters from the browser process.
class MODULES_EXPORT WebLaunchServiceImpl final
    : public mojom::blink::WebLaunchService {
 public:
  static void Create(
      LocalFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::WebLaunchService>);
  explicit WebLaunchServiceImpl(LocalDOMWindow& frame);
  ~WebLaunchServiceImpl() override;

  // blink::mojom::WebLaunchService:
  void SetLaunchFiles(
      WTF::Vector<mojom::blink::NativeFileSystemEntryPtr>) override;

 private:
  WeakPersistent<LocalDOMWindow> window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_WEB_LAUNCH_SERVICE_IMPL_H_
