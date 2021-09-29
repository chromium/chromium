// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_AUDIO_RENDERER_SINK_H_
#define MEDIA_BASE_MOCK_AUDIO_RENDERER_SINK_H_

#include <string>

#include "base/macros.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockAudioRendererSink : public SwitchableAudioRendererSink {
 public:
  MockAudioRendererSink();
  explicit MockAudioRendererSink(OutputDeviceStatus device_status);
  MockAudioRendererSink(const std::string& device_id,
                        OutputDeviceStatus device_status);
  MockAudioRendererSink(const std::string& device_id,
                        OutputDeviceStatus device_status,
                        const AudioParameters& device_output_params);

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Play, void());
  MOCK_METHOD1(SetVolume, bool(double volume));
  MOCK_METHOD0(CurrentThreadIsRenderingThread, bool());

  OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;

  bool IsOptimizedForHardwareParameters() override;

  void SwitchOutputDevice(const std::string& device_id,
                          OutputDeviceStatusCB callback) override;
  void Initialize(const AudioParameters& params,
                  RenderCallback* renderer) override;
  AudioRendererSink::RenderCallback* callback() { return callback_; }

 protected:
  ~MockAudioRendererSink() override;

 private:
  RenderCallback* callback_;
  OutputDeviceInfo output_device_info_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererSink);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_AUDIO_RENDERER_SINK_H_
