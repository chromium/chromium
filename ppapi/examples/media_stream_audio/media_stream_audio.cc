// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "ppapi/cpp/audio_buffer.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/media_stream_audio_track.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/utility/completion_callback_factory.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// This example demonstrates receiving audio samples from an AndioMediaTrack
// and visualizing them.

namespace {

const uint32_t kColorRed = 0xFFFF0000;
const uint32_t kColorGreen = 0xFF00FF00;
const uint32_t kColorGrey1 = 0xFF202020;
const uint32_t kColorGrey2 = 0xFF404040;
const uint32_t kColorGrey3 = 0xFF606060;

class MediaStreamAudioInstance : public pp::Instance {
 public:
  explicit MediaStreamAudioInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        first_buffer_(true),
        sample_count_(0),
        channel_count_(0),
        timer_interval_(0),
        pending_paint_(false),
        waiting_for_flush_completion_(false) {
  }

  virtual ~MediaStreamAudioInstance() {
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    if (position.size() == size_)
      return;

    size_ = position.size();
    device_context_ = pp::Graphics2D(this, size_, false);
    if (!BindGraphics(device_context_))
      return;

    Paint();
  }

  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_dictionary())
      return;
    pp::VarDictionary var_dictionary_message(var_message);
    pp::Var var_track = var_dictionary_message.Get("track");
    if (!var_track.is_resource())
      return;

    pp::Resource resource_track = var_track.AsResource();
    audio_track_ = pp::MediaStreamAudioTrack(resource_track);
    audio_track_.GetBuffer(callback_factory_.NewCallbackWithOutput(
          &MediaStreamAudioInstance::OnGetBuffer));
  }

 private:
  void ScheduleNextTimer() {
    PP_DCHECK(timer_interval_ > 0);
    pp::Module::Get()->core()->CallOnMainThread(
        timer_interval_,
        callback_factory_.NewCallback(&MediaStreamAudioInstance::OnTimer),
        0);
  }

  void OnTimer(int32_t) {
    ScheduleNextTimer();
    Paint();
  }

  void DidFlush(int32_t result) {
    waiting_for_flush_completion_ = false;
    if (pending_paint_)
      Paint();
  }

  void Paint() {
    if (waiting_for_flush_completion_) {
      pending_paint_ = true;
      return;
    }

    pending_paint_ = false;

    if (size_.IsEmpty())
      return;  // Nothing to do.

    pp::ImageData image = PaintImage(size_);
    if (!image.is_null()) {
      device_context_.ReplaceContents(&image);
      waiting_for_flush_completion_ = true;
      device_context_.Flush(
          callback_factory_.NewCallback(&MediaStreamAudioInstance::DidFlush));
    }
  }

  pp::ImageData PaintImage(const pp::Size& size) {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, false);
    if (image.is_null())
      return image;

    // Clear to dark grey.
    for (int y = 0; y < size.height(); y++) {
      for (int x = 0; x < size.width(); x++)
        *image.GetAddr32(pp::Point(x, y)) = kColorGrey1;
    }

    int mid_height = size.height() / 2;
    int max_amplitude = size.height() * 4 / 10;

    // Draw some lines.
    for (int x = 0; x < size.width(); x++) {
      *image.GetAddr32(pp::Point(x, mid_height)) = kColorGrey3;
      *image.GetAddr32(pp::Point(x, mid_height + max_amplitude)) = kColorGrey2;
      *image.GetAddr32(pp::Point(x, mid_height - max_amplitude)) = kColorGrey2;
    }


    // Draw our samples.
    for (int x = 0, i = 0;
         x < std::min(size.width(), static_cast<int>(sample_count_));
         x++, i += channel_count_) {
      for (uint32_t ch = 0; ch < std::min(channel_count_, 2U); ++ch) {
        int y = samples_[i + ch] * max_amplitude /
                (std::numeric_limits<int16_t>::max() + 1) + mid_height;
        *image.GetAddr32(pp::Point(x, y)) = (ch == 0 ? kColorRed : kColorGreen);
      }
    }

    return image;
  }

  // Callback that is invoked when new buffers are received.
  void OnGetBuffer(int32_t result, pp::AudioBuffer buffer) {
    if (result != PP_OK)
      return;

    PP_DCHECK(buffer.GetSampleSize() == PP_AUDIOBUFFER_SAMPLESIZE_16_BITS);
    const char* data = static_cast<const char*>(buffer.GetDataBuffer());
    uint32_t channels = buffer.GetNumberOfChannels();
    uint32_t samples = buffer.GetNumberOfSamples() / channels;

    if (channel_count_ != channels || sample_count_ != samples) {
      channel_count_ = channels;
      sample_count_ = samples;

      samples_.resize(sample_count_ * channel_count_);
      timer_interval_ = (sample_count_ * 1000) / buffer.GetSampleRate() + 5;
      // Start the timer for the first buffer.
      if (first_buffer_) {
        first_buffer_ = false;
        ScheduleNextTimer();
      }
    }

    memcpy(samples_.data(), data,
        sample_count_ * channel_count_ * sizeof(int16_t));

    audio_track_.RecycleBuffer(buffer);
    audio_track_.GetBuffer(callback_factory_.NewCallbackWithOutput(
        &MediaStreamAudioInstance::OnGetBuffer));

  }

  pp::MediaStreamAudioTrack audio_track_;
  pp::CompletionCallbackFactory<MediaStreamAudioInstance> callback_factory_;

  bool first_buffer_;
  uint32_t sample_count_;
  uint32_t channel_count_;
  std::vector<int16_t> samples_;

  int32_t timer_interval_;

  // Painting stuff.
  pp::Size size_;
  pp::Graphics2D device_context_;
  bool pending_paint_;
  bool waiting_for_flush_completion_;
};

class MediaStreamAudioModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MediaStreamAudioInstance(instance);
  }
};

}  // namespace

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MediaStreamAudioModule();
}

}  // namespace pp
