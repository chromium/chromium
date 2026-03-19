// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_QUEUE_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom-blink.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

class V8LaunchConsumer;

class LaunchQueue final : public ScriptWrappable,
                          public mojom::blink::WebLaunchService,
                          public Supplement<LocalDOMWindow> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit LaunchQueue(LocalDOMWindow&);
  ~LaunchQueue() override;

  static const char kSupplementName[];

  static void BindReceiver(
      LocalFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::WebLaunchService>);

  // From the partial Window IDL interface:
  static LaunchQueue* launchQueue(LocalDOMWindow&);

  // blink::mojom::WebLaunchService:
  void EnqueueLaunchParams(
      const KURL& launch_url,
      base::TimeTicks time_navigation_started_in_browser,
      bool navigation_started,
      ::blink::Vector<::blink::mojom::blink::FileSystemAccessEntryPtr> files)
      override;

  // IDL implementation:
  void setConsumer(V8LaunchConsumer*);

  // ScriptWrappable:
  void Trace(Visitor* visitor) const override;

 private:
  void InvokeConsumerWithParams(LaunchParams* params);

  HeapVector<Member<LaunchParams>> unconsumed_launch_params_;
  Member<V8LaunchConsumer> consumer_;
  HeapMojoAssociatedReceiver<mojom::blink::WebLaunchService,
                             LaunchQueue,
                             HeapMojoWrapperMode::kForceWithoutContextObserver>
      receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_LAUNCH_QUEUE_H_
