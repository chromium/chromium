// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/dev/audio_input_dev.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/lock.h"

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

namespace {

// This sample frequency is guaranteed to work.
const PP_AudioSampleRate kSampleFrequency = PP_AUDIOSAMPLERATE_44100;
const uint32_t kSampleCount = 1024;
const uint32_t kChannelCount = 1;
const char* const kDelimiter = "#__#";

}  // namespace

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        sample_count_(0),
        channel_count_(0),
        samples_(NULL),
        latency_(0),
        timer_interval_(0),
        pending_paint_(false),
        waiting_for_flush_completion_(false) {
  }
  virtual ~MyInstance() {
    device_detector_.MonitorDeviceChange(NULL, NULL);
    audio_input_.Close();

    // The audio input thread has exited before the previous call returned, so
    // it is safe to do so now.
    delete[] samples_;
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    sample_count_ = pp::AudioConfig::RecommendSampleFrameCount(this,
                                                               kSampleFrequency,
                                                               kSampleCount);
    PP_DCHECK(sample_count_ > 0);
    channel_count_ = kChannelCount;
    samples_ = new int16_t[sample_count_ * channel_count_];
    memset(samples_, 0, sample_count_ * channel_count_ * sizeof(int16_t));

    device_detector_ = pp::AudioInput_Dev(this);

    // Try to ensure that we pick up a new set of samples between each
    // timer-generated repaint.
    timer_interval_ = (sample_count_ * 1000) / kSampleFrequency + 5;
    ScheduleNextTimer();

    return true;
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

  virtual void HandleMessage(const pp::Var& message_data) {
    if (message_data.is_string()) {
      std::string event = message_data.AsString();
      if (event == "PageInitialized") {
        int32_t result = device_detector_.MonitorDeviceChange(
            &MyInstance::MonitorDeviceChangeCallback, this);
        if (result != PP_OK)
          PostMessage(pp::Var("MonitorDeviceChangeFailed"));

        pp::CompletionCallbackWithOutput<std::vector<pp::DeviceRef_Dev> >
            callback = callback_factory_.NewCallbackWithOutput(
                &MyInstance::EnumerateDevicesFinished);
        result = device_detector_.EnumerateDevices(callback);
        if (result != PP_OK_COMPLETIONPENDING)
          PostMessage(pp::Var("EnumerationFailed"));
      } else if (event == "UseDefault") {
        Open(pp::DeviceRef_Dev());
      } else if (event == "Stop") {
        Stop();
      } else if (event == "Start") {
        Start();
      } else if (event.find("Monitor:") == 0) {
        std::string index_str = event.substr(strlen("Monitor:"));
        int index = atoi(index_str.c_str());
        if (index >= 0 && index < static_cast<int>(monitor_devices_.size()))
          Open(monitor_devices_[index]);
        else
          PP_NOTREACHED();
      } else if (event.find("Enumerate:") == 0) {
        std::string index_str = event.substr(strlen("Enumerate:"));
        int index = atoi(index_str.c_str());
        if (index >= 0 && index < static_cast<int>(enumerate_devices_.size()))
          Open(enumerate_devices_[index]);
        else
          PP_NOTREACHED();
      }
    }
  }

 private:
  void ScheduleNextTimer() {
    PP_DCHECK(timer_interval_ > 0);
    pp::Module::Get()->core()->CallOnMainThread(
        timer_interval_,
        callback_factory_.NewCallback(&MyInstance::OnTimer),
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
          callback_factory_.NewCallback(&MyInstance::DidFlush));
    }
  }

  pp::ImageData PaintImage(const pp::Size& size) {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, size, false);
    if (image.is_null())
      return image;

    // Clear to dark grey.
    for (int y = 0; y < size.height(); y++) {
      for (int x = 0; x < size.width(); x++)
        *image.GetAddr32(pp::Point(x, y)) = 0xff202020;
    }

    int mid_height = size.height() / 2;
    int max_amplitude = size.height() * 4 / 10;

    // Draw some lines.
    for (int x = 0; x < size.width(); x++) {
      *image.GetAddr32(pp::Point(x, mid_height)) = 0xff606060;
      *image.GetAddr32(pp::Point(x, mid_height + max_amplitude)) = 0xff404040;
      *image.GetAddr32(pp::Point(x, mid_height - max_amplitude)) = 0xff404040;
    }

    {
      pp::AutoLock auto_lock(lock_);

      // Draw the latency as a red bar at the bottom.
      PP_DCHECK(latency_ >= 0);
      int latency_bar_length = latency_ < 1 ?
          static_cast<int>(size.width() * latency_) : size.width();
      for (int x = 0; x < latency_bar_length; ++x) {
        *image.GetAddr32(pp::Point(x, mid_height + max_amplitude)) = 0xffff0000;
      }

      // Draw our samples.
      for (int x = 0, i = 0;
           x < std::min(size.width(), static_cast<int>(sample_count_));
           x++, i += channel_count_) {
        int y = samples_[i] * max_amplitude /
                (std::numeric_limits<int16_t>::max() + 1) + mid_height;
        *image.GetAddr32(pp::Point(x, y)) = 0xffffffff;
      }
    }

    return image;
  }

  void Open(const pp::DeviceRef_Dev& device) {
    audio_input_.Close();
    audio_input_ = pp::AudioInput_Dev(this);

    pp::AudioConfig config = pp::AudioConfig(this,
                                             kSampleFrequency,
                                             sample_count_);
    pp::CompletionCallback callback = callback_factory_.NewCallback(
        &MyInstance::OpenFinished);
    int32_t result = audio_input_.Open(device, config, CaptureCallback, this,
                                       callback);
    if (result != PP_OK_COMPLETIONPENDING)
      PostMessage(pp::Var("OpenFailed"));
  }

  void Stop() {
    if (!audio_input_.StopCapture())
      PostMessage(pp::Var("StopFailed"));
  }

  void Start() {
    if (!audio_input_.StartCapture())
      PostMessage(pp::Var("StartFailed"));
  }

  void EnumerateDevicesFinished(int32_t result,
                                std::vector<pp::DeviceRef_Dev>& devices) {
    if (result == PP_OK) {
      enumerate_devices_.swap(devices);
      std::string device_names = "Enumerate:";
      for (size_t index = 0; index < enumerate_devices_.size(); ++index) {
        pp::Var name = enumerate_devices_[index].GetName();
        PP_DCHECK(name.is_string());

        if (index != 0)
          device_names += kDelimiter;
        device_names += name.AsString();
      }
      PostMessage(pp::Var(device_names));
    } else {
      PostMessage(pp::Var("EnumerationFailed"));
    }
  }

  void OpenFinished(int32_t result) {
    if (result == PP_OK) {
      if (!audio_input_.StartCapture())
        PostMessage(pp::Var("StartFailed"));
    } else {
      PostMessage(pp::Var("OpenFailed"));
    }
  }

  static void CaptureCallback(const void* samples,
                              uint32_t num_bytes,
                              PP_TimeDelta latency,
                              void* ctx) {
    MyInstance* thiz = static_cast<MyInstance*>(ctx);
    pp::AutoLock auto_lock(thiz->lock_);
    thiz->latency_ = latency;
    uint32_t buffer_size =
        thiz->sample_count_ * thiz->channel_count_ * sizeof(int16_t);
    PP_DCHECK(num_bytes <= buffer_size);
    PP_DCHECK(num_bytes % (thiz->channel_count_ * sizeof(int16_t)) == 0);
    memcpy(thiz->samples_, samples, num_bytes);
    memset(reinterpret_cast<char*>(thiz->samples_) + num_bytes, 0,
           buffer_size - num_bytes);
  }

  static void MonitorDeviceChangeCallback(void* user_data,
                                          uint32_t device_count,
                                          const PP_Resource devices[]) {
    MyInstance* thiz = static_cast<MyInstance*>(user_data);

    std::string device_names = "Monitor:";
    thiz->monitor_devices_.clear();
    thiz->monitor_devices_.reserve(device_count);
    for (size_t index = 0; index < device_count; ++index) {
      thiz->monitor_devices_.push_back(pp::DeviceRef_Dev(devices[index]));
      pp::Var name = thiz->monitor_devices_.back().GetName();
      PP_DCHECK(name.is_string());

      if (index != 0)
        device_names += kDelimiter;
      device_names += name.AsString();
    }
    thiz->PostMessage(pp::Var(device_names));
  }

  pp::CompletionCallbackFactory<MyInstance> callback_factory_;

  uint32_t sample_count_;
  uint32_t channel_count_;
  int16_t* samples_;

  PP_TimeDelta latency_;

  int32_t timer_interval_;

  // Painting stuff.
  pp::Size size_;
  pp::Graphics2D device_context_;
  bool pending_paint_;
  bool waiting_for_flush_completion_;

  // There is no need to have two resources to do capturing and device detecting
  // separately. However, this makes the code of monitoring device change
  // easier.
  pp::AudioInput_Dev audio_input_;
  pp::AudioInput_Dev device_detector_;

  std::vector<pp::DeviceRef_Dev> enumerate_devices_;
  std::vector<pp::DeviceRef_Dev> monitor_devices_;

  // Protects |samples_| and |latency_|.
  pp::Lock lock_;
};

class MyModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp
