// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_RENDERER_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/mojom/render_frame_metadata.mojom-forward.h"
#include "components/viz/common/display/renderer_settings.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class LayerTreeFrameSink;
class RenderFrameMetadataObserver;
class TaskGraphRunner;
class UkmRecorderFactory;
}  // namespace cc

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
}  // namespace blink

namespace content {
class RenderWidget;

class CONTENT_EXPORT CompositorDependencies {
 public:
  virtual bool IsLcdTextEnabled() = 0;
  virtual bool IsElasticOverscrollEnabled() = 0;
  virtual bool IsUseZoomForDSFEnabled() = 0;
  virtual bool IsSingleThreaded() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCleanupTaskRunner() = 0;
  virtual blink::scheduler::WebThreadScheduler* GetWebMainThreadScheduler() = 0;
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;
  virtual bool IsScrollAnimatorEnabled() = 0;
  virtual std::unique_ptr<cc::UkmRecorderFactory>
  CreateUkmRecorderFactory() = 0;

  using LayerTreeFrameSinkCallback = base::OnceCallback<void(
      std::unique_ptr<cc::LayerTreeFrameSink>,
      std::unique_ptr<cc::RenderFrameMetadataObserver>)>;
  virtual void RequestNewLayerTreeFrameSink(
      RenderWidget* render_widget,
      const GURL& url,
      LayerTreeFrameSinkCallback callback,
      const char* client_name) = 0;

  virtual ~CompositorDependencies() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_
