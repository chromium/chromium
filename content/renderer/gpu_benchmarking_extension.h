// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_BENCHMARKING_EXTENSION_H_
#define CONTENT_RENDERER_GPU_BENCHMARKING_EXTENSION_H_

#include "base/memory/weak_ptr.h"
#include "content/common/input/input_injector.mojom.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace gin {
class Arguments;
}

namespace v8 {
class Isolate;
class Object;
}  // namespace v8

namespace content {

class RenderFrameImpl;

// gin class for gpu benchmarking
class GpuBenchmarking : public gin::Wrappable<GpuBenchmarking> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  GpuBenchmarking(const GpuBenchmarking&) = delete;
  GpuBenchmarking& operator=(const GpuBenchmarking&) = delete;

  static void Install(base::WeakPtr<RenderFrameImpl> frame);

 private:
  explicit GpuBenchmarking(base::WeakPtr<RenderFrameImpl> frame);
  ~GpuBenchmarking() override;
  void EnsureRemoteInterface();

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // JavaScript handlers.
  void SetNeedsDisplayOnAllLayers();
  void SetRasterizeOnlyVisibleContent();
  void PrintToSkPicture(v8::Isolate* isolate, const std::string& dirname);
  void PrintPagesToSkPictures(v8::Isolate* isolate,
                              const std::string& filename);
  void PrintPagesToXPS(v8::Isolate* isolate, const std::string& filename);
  bool GestureSourceTypeSupported(int gesture_source_type);

  // All arguments in these methods are in visual viewport coordinates.
  bool SmoothScrollBy(gin::Arguments* args);
  bool SmoothScrollByXY(gin::Arguments* args);
  bool SmoothDrag(gin::Arguments* args);
  bool Swipe(gin::Arguments* args);
  bool ScrollBounce(gin::Arguments* args);
  bool PinchBy(gin::Arguments* args);
  bool Tap(gin::Arguments* args);

  bool PointerActionSequence(gin::Arguments* args);

  // The offset of the visual viewport *within* the layout viewport, in CSS
  // pixels. i.e. As the user zooms in, these values don't change.
  float VisualViewportX();
  float VisualViewportY();

  // The width and height of the visual viewport in CSS pixels. i.e. As the
  // user zooms in, these get smaller (since the physical viewport is a fixed
  // size, fewer CSS pixels fit into it).
  float VisualViewportHeight();
  float VisualViewportWidth();

  // Returns the page scale factor applied as a result of pinch-zoom.
  float PageScaleFactor();
  // Sets the page scale factor applied as a result of pinch-zoom.
  void SetPageScaleFactor(float scale);

  void SetBrowserControlsShown(bool shown);

  void ClearImageCache();
  int RunMicroBenchmark(gin::Arguments* args);
  bool SendMessageToMicroBenchmark(int id, v8::Local<v8::Object> message);
  bool HasGpuChannel();
  bool HasGpuProcess();
  void CrashGpuProcess();
  void TerminateGpuProcessNormally();
  void GetGpuDriverBugWorkarounds(gin::Arguments* args);

  // Starts/stops the sampling profiler. StartProfiling takes one optional
  // argument, which is a file name for saving the data (relative to `pwd`
  // or %USERDIR%); if omitted, it defaults to "profile.pb".
  //
  // DO NOT USE THIS IN CHROMIUM TESTS -- we don't want to fill up the bots'
  // hard drives with profile data.
  void StartProfiling(gin::Arguments* args);
  void StopProfiling();

  // Freezes a page, used to transition the page to the FROZEN lifecycle state.
  void Freeze();

  // Register a callback that should be fired when the next swap completes.
  // The callback is removed once it's executed.
  bool AddSwapCompletionEventListener(gin::Arguments* args);

  // For Mac only, returns the error code why CoreAnimation Renderer is not used
  // in the requested frame. It's less efficient when this path is not hit.
  // See "ui/gfx/ca_layer_result.h" for error codes.
  int AddCoreAnimationStatusEventListener(gin::Arguments* args);

  // Returns true if the argument is a CanvasImageSource whose image data is
  // stored on the GPU.
  bool IsAcceleratedCanvasImageSource(gin::Arguments* args);

  base::WeakPtr<RenderFrameImpl> render_frame_;
  mojo::Remote<mojom::InputInjector> input_injector_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_BENCHMARKING_EXTENSION_H_
