// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Needed on Windows to get |M_PI| from math.h.
#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <limits>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/audio.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/view.h"

// Separate left and right frequency to make sure we didn't swap L & R.
// Sounds pretty horrible, though...
const double kLeftFrequency = 400;
const double kRightFrequency = 1000;

// This sample frequency is guaranteed to work.
const PP_AudioSampleRate kDefaultSampleRate = PP_AUDIOSAMPLERATE_44100;
const uint32_t kDefaultSampleCount = 4096;

const char kSampleRateAttributeName[] = "samplerate";

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        visible_(false),
        sample_rate_(kDefaultSampleRate),
        sample_count_(0),
        audio_wave_l_(0.0),
        audio_wave_r_(0.0) {
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    for (uint32_t i = 0; i < argc; i++) {
      if (strcmp(kSampleRateAttributeName, argn[i]) == 0) {
        int value = atoi(argv[i]);
        if (value > 0 && value <= 1000000)
          sample_rate_ = static_cast<PP_AudioSampleRate>(value);
        else
          return false;
      }
    }

    pp::AudioConfig config;
    sample_count_ = pp::AudioConfig::RecommendSampleFrameCount(
        this, sample_rate_, kDefaultSampleCount);
    config = pp::AudioConfig(this, sample_rate_, sample_count_);
    audio_ = pp::Audio(this, config, SineWaveCallbackTrampoline, this);
    return audio_.StartPlayback();
  }

  virtual void DidChangeView(const pp::View& view) {
    // The frequency will change depending on whether the page is in the
    // foreground or background.
    visible_ = view.IsPageVisible();
  }

 private:
  static void SineWaveCallbackTrampoline(void* samples,
                                         uint32_t num_bytes,
                                         void* thiz) {
    static_cast<MyInstance*>(thiz)->SineWaveCallback(samples, num_bytes);
  }

  void SineWaveCallback(void* samples, uint32_t num_bytes) {
    double delta_l = 2.0 * M_PI * kLeftFrequency /
                     static_cast<double>(sample_rate_) / (visible_ ? 1 : 2);
    double delta_r = 2.0 * M_PI * kRightFrequency /
                     static_cast<double>(sample_rate_) / (visible_ ? 1 : 2);

    // Use per channel audio wave value to avoid clicks on buffer boundries.
    double wave_l = audio_wave_l_;
    double wave_r = audio_wave_r_;
    const int16_t max_int16 = std::numeric_limits<int16_t>::max();
    int16_t* buf = reinterpret_cast<int16_t*>(samples);
    for (size_t sample = 0; sample < sample_count_; ++sample) {
      *buf++ = static_cast<int16_t>(sin(wave_l) * max_int16);
      *buf++ = static_cast<int16_t>(sin(wave_r) * max_int16);
      // Add delta, keep within -2 * M_PI .. 2 * M_PI to preserve precision.
      wave_l += delta_l;
      if (wave_l > 2.0 * M_PI)
        wave_l -= 2.0 * M_PI;
      wave_r += delta_r;
      if (wave_r > 2.0 * M_PI)
        wave_r -= 2.0 * M_PI;
    }
    // Store current value to use as starting point for next callback.
    audio_wave_l_ = wave_l;
    audio_wave_r_ = wave_r;
  }

  bool visible_;

  PP_AudioSampleRate sample_rate_;
  uint32_t sample_count_;

  pp::Audio audio_;

  // Current audio wave position, used to prevent sine wave skips
  // on buffer boundaries.
  double audio_wave_l_;
  double audio_wave_r_;
};

class MyModule : public pp::Module {
 public:
  // Override CreateInstance to create your customized Instance object.
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
