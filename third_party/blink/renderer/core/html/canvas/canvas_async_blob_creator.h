// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_ASYNC_BLOB_CREATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_ASYNC_BLOB_CREATOR_H_

#include <memory>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob_callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/canvas/image_encode_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;

constexpr const char* kSRGBImageColorSpaceName = "srgb";
constexpr const char* kRec2020ImageColorSpaceName = "rec2020";
constexpr const char* kDisplayP3ImageColorSpaceName = "display-p3";

constexpr const char* kRGBA8ImagePixelFormatName = "uint8";
constexpr const char* kRGBA16ImagePixelFormatName = "uint16";

class CORE_EXPORT CanvasAsyncBlobCreator
    : public GarbageCollected<CanvasAsyncBlobCreator> {
 public:
  // This enum is used to back an UMA histogram, and should therefore be treated
  // as append-only. Idle tasks are not implemented for some image types.
  enum IdleTaskStatus {
    kIdleTaskNotStarted = 0,
    kIdleTaskStarted = 1,
    kIdleTaskCompleted = 2,
    kIdleTaskFailed = 3,
    kIdleTaskSwitchedToImmediateTask = 4,
    kIdleTaskNotSupported = 5,
    kMaxValue = kIdleTaskNotSupported,
  };
  enum ToBlobFunctionType {
    kHTMLCanvasToBlobCallback,
    kHTMLCanvasConvertToBlobPromise,
    kOffscreenCanvasConvertToBlobPromise,
    kNumberOfToBlobFunctionTypes
  };

  void ScheduleAsyncBlobCreation(const double& quality);

  CanvasAsyncBlobCreator(scoped_refptr<StaticBitmapImage>,
                         const ImageEncodeOptions* options,
                         ToBlobFunctionType function_type,
                         base::TimeTicks start_time,
                         ExecutionContext*,
                         ScriptPromiseResolver*);
  CanvasAsyncBlobCreator(scoped_refptr<StaticBitmapImage>,
                         const ImageEncodeOptions*,
                         ToBlobFunctionType,
                         V8BlobCallback*,
                         base::TimeTicks start_time,
                         ExecutionContext*,
                         ScriptPromiseResolver* = nullptr);
  virtual ~CanvasAsyncBlobCreator();

  // Methods are virtual for mocking in unit tests
  virtual void SignalTaskSwitchInStartTimeoutEventForTesting() {}
  virtual void SignalTaskSwitchInCompleteTimeoutEventForTesting() {}

  virtual void Trace(Visitor*);

  static sk_sp<SkColorSpace> BlobColorSpaceToSkColorSpace(
      String blob_color_space);

  bool EncodeImageForConvertToBlobTest();
  Vector<unsigned char> GetEncodedImageForConvertToBlobTest() {
    return encoded_image_;
  }

 protected:
  static ImageEncodeOptions* GetImageEncodeOptionsForMimeType(
      ImageEncodingMimeType);
  // Methods are virtual for unit testing
  virtual void ScheduleInitiateEncoding(double quality);
  virtual void IdleEncodeRows(base::TimeTicks deadline);
  virtual void PostDelayedTaskToCurrentThread(const base::Location&,
                                              base::OnceClosure,
                                              double delay_ms);
  virtual void SignalAlternativeCodePathFinishedForTesting() {}
  virtual void CreateBlobAndReturnResult();
  virtual void CreateNullAndReturnResult();

  void InitiateEncoding(double quality, base::TimeTicks deadline);

 protected:
  IdleTaskStatus idle_task_status_;
  bool fail_encoder_initialization_for_test_;
  bool enforce_idle_encoding_for_test_;

 private:
  friend class CanvasAsyncBlobCreatorTest;

  void Dispose();

  scoped_refptr<StaticBitmapImage> image_;
  std::unique_ptr<ImageEncoder> encoder_;
  Vector<unsigned char> encoded_image_;
  int num_rows_completed_;
  Member<ExecutionContext> context_;

  SkPixmap src_data_;
  ImageEncodingMimeType mime_type_;
  Member<const ImageEncodeOptions> encode_options_;
  ToBlobFunctionType function_type_;
  sk_sp<SkData> png_data_helper_;

  // Chrome metrics use
  base::TimeTicks start_time_;
  base::TimeTicks schedule_idle_task_start_time_;
  bool static_bitmap_image_loaded_;

  // Used when CanvasAsyncBlobCreator runs on main thread only
  scoped_refptr<base::SingleThreadTaskRunner> parent_frame_task_runner_;

  // Used for HTMLCanvasElement only
  Member<V8BlobCallback> callback_;

  // Used for OffscreenCanvas only
  Member<ScriptPromiseResolver> script_promise_resolver_;

  bool EncodeImage(const double&);

  // PNG, JPEG
  bool InitializeEncoder(double quality);
  void ForceEncodeRowsOnCurrentThread();  // Similar to IdleEncodeRows
                                          // without deadline

  // WEBP
  void EncodeImageOnEncoderThread(double quality);

  void IdleTaskStartTimeoutEvent(double quality);
  void IdleTaskCompleteTimeoutEvent();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_ASYNC_BLOB_CREATOR_H_
