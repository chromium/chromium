// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <deque>
#include <string>

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef min
#undef max
#undef PostMessage

// Allow 'this' in initializer list
#pragma warning(disable : 4355)
// Disable warning about behaviour of array initialization.
#pragma warning(disable : 4351)
#endif

namespace {

const uint32_t kBlue = 0xff4040ffu;
const uint32_t kBlack = 0xff000000u;
const size_t kHistogramSize = 256u;

}  // namespace

class VarArrayBufferInstance : public pp::Instance {
 public:
  explicit VarArrayBufferInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        flushing_(false),
        histogram_() {}
  virtual ~VarArrayBufferInstance() {}

 private:
  /// Handler for messages coming in from the browser via postMessage().  The
  /// @a var_message can contain anything: a JSON string; a string that encodes
  /// method names and arguments; etc.
  ///
  /// In this case, we only handle <code>pp::VarArrayBuffer</code>s. When we
  /// receive one, we compute and display a histogram based on its contents.
  ///
  /// @param[in] var_message The message posted by the browser.
  virtual void HandleMessage(const pp::Var& var_message) {
    if (var_message.is_array_buffer()) {
      pp::VarArrayBuffer buffer(var_message);
      ComputeHistogram(buffer);
      DrawHistogram();
    }
  }

  /// Create and return a blank (all-black) <code>pp::ImageData</code> of the
  /// given <code>size</code>.
  pp::ImageData MakeBlankImageData(const pp::Size& size) {
    const bool init_to_zero = false;
    pp::ImageData image_data =
        pp::ImageData(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, init_to_zero);
    uint32_t* image_buffer = static_cast<uint32_t*>(image_data.data());
    for (int i = 0; i < size.GetArea(); ++i)
      image_buffer[i] = kBlack;
    return image_data;
  }

  /// Draw a bar of the appropriate height based on <code>value</code> at
  /// <code>column</code> in <code>image_data</code>. <code>value</code> must be
  /// in the range [0, 1].
  void DrawBar(uint32_t column, double value, pp::ImageData* image_data) {
    assert((value >= 0.0) && (value <= 1.0));
    uint32_t* image_buffer = static_cast<uint32_t*>(image_data->data());
    const uint32_t image_height = image_data->size().height();
    const uint32_t image_width = image_data->size().width();
    assert(column < image_width);
    int bar_height = static_cast<int>(value * image_height);
    for (int i = 0; i < bar_height; ++i) {
      uint32_t row = image_height - 1 - i;
      image_buffer[row * image_width + column] = kBlue;
    }
  }

  void PaintAndFlush(pp::ImageData* image_data) {
    assert(!flushing_);
    graphics_2d_context_.ReplaceContents(image_data);
    graphics_2d_context_.Flush(
        callback_factory_.NewCallback(&VarArrayBufferInstance::DidFlush));
    flushing_ = true;
  }

  /// The callback that gets invoked when a flush completes. This is bound to a
  /// <code>CompletionCallback</code> and passed as a parameter to
  /// <code>Flush</code>.
  void DidFlush(int32_t error_code) {
    flushing_ = false;
    // If there are no images in the queue, we're done for now.
    if (paint_queue_.empty())
      return;
    // Otherwise, pop the next image off the queue and draw it.
    pp::ImageData image_data = paint_queue_.front();
    paint_queue_.pop_front();
    PaintAndFlush(&image_data);
  }

  virtual void DidChangeView(const pp::View& view) {
    if (size_ != view.GetRect().size()) {
      size_ = view.GetRect().size();
      const bool is_always_opaque = true;
      graphics_2d_context_ =
          pp::Graphics2D(this, view.GetRect().size(), is_always_opaque);
      BindGraphics(graphics_2d_context_);
      // The images in our queue are the wrong size, so we won't paint them.
      // We'll only draw the most recently computed histogram.
      paint_queue_.clear();
      DrawHistogram();
    }
  }

  /// Compute and normalize a histogram based on the given VarArrayBuffer.
  void ComputeHistogram(pp::VarArrayBuffer& buffer) {
    std::fill_n(histogram_, kHistogramSize, 0.0);
    uint32_t buffer_size = buffer.ByteLength();
    if (buffer_size == 0)
      return;
    uint8_t* buffer_data = static_cast<uint8_t*>(buffer.Map());
    for (uint32_t i = 0; i < buffer_size; ++i)
      histogram_[buffer_data[i]] += 1.0;
    // Normalize.
    double max = *std::max_element(histogram_, histogram_ + kHistogramSize);
    for (uint32_t i = 0; i < kHistogramSize; ++i)
      histogram_[i] /= max;
  }

  /// Draw the current histogram_ in to an pp::ImageData, then paint and flush
  /// that image. If we're already waiting on a flush, push it on to
  /// <code>paint_queue_</code> to paint later.
  void DrawHistogram() {
    pp::ImageData image_data = MakeBlankImageData(size_);
    for (int i = 0; i < std::min(static_cast<int>(kHistogramSize),
                                 image_data.size().width());
         ++i) {
      DrawBar(i, histogram_[i], &image_data);
    }

    if (!flushing_)
      PaintAndFlush(&image_data);
    else
      paint_queue_.push_back(image_data);
  }

  pp::Graphics2D graphics_2d_context_;
  pp::CompletionCallbackFactory<VarArrayBufferInstance> callback_factory_;

  /// A queue of images to paint. We must maintain a queue because we can not
  /// call pp::Graphics2D::Flush while a Flush is already pending.
  std::deque<pp::ImageData> paint_queue_;

  /// The size of our rectangle in the DOM, as of the last time DidChangeView
  /// was called.
  pp::Size size_;

  /// true iff we are flushing.
  bool flushing_;

  /// Stores the most recent histogram so that we can re-draw it if we get
  /// resized.
  double histogram_[kHistogramSize];
};

class VarArrayBufferModule : public pp::Module {
 public:
  VarArrayBufferModule() : pp::Module() {}
  virtual ~VarArrayBufferModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new VarArrayBufferInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new VarArrayBufferModule(); }
}  // namespace pp
