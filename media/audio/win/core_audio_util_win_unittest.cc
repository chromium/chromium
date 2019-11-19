// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/core_audio_util_win.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_unittest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::ComPtr;
using base::win::ScopedCOMInitializer;

namespace media {

class CoreAudioUtilWinTest : public ::testing::Test {
 protected:
  // The tests must run on a COM thread.
  // If we don't initialize the COM library on a thread before using COM,
  // all function calls will return CO_E_NOTINITIALIZED.
  CoreAudioUtilWinTest() {
    DCHECK(com_init_.Succeeded());
  }
  ~CoreAudioUtilWinTest() override {}

  bool DevicesAvailable() {
    return CoreAudioUtil::IsSupported() &&
           CoreAudioUtil::NumberOfActiveDevices(eCapture) > 0 &&
           CoreAudioUtil::NumberOfActiveDevices(eRender) > 0;
  }

  ScopedCOMInitializer com_init_;
};

TEST_F(CoreAudioUtilWinTest, WaveFormatWrapper) {
  // Use default constructor for WAVEFORMATEX and verify its size.
  WAVEFORMATEX format = {};
  CoreAudioUtil::WaveFormatWrapper wave_format(&format);
  EXPECT_FALSE(wave_format.IsExtensible());
  EXPECT_EQ(wave_format.size(), sizeof(WAVEFORMATEX));
  EXPECT_EQ(wave_format->cbSize, 0);

  // Ensure that the stand-alone WAVEFORMATEX structure has a valid format tag
  // and that all accessors work.
  format.wFormatTag = WAVE_FORMAT_PCM;
  EXPECT_FALSE(wave_format.IsExtensible());
  EXPECT_EQ(wave_format.size(), sizeof(WAVEFORMATEX));
  EXPECT_EQ(wave_format.get()->wFormatTag, WAVE_FORMAT_PCM);
  EXPECT_EQ(wave_format->wFormatTag, WAVE_FORMAT_PCM);

  // Next, ensure that the size is valid. Stand-alone is not extended.
  EXPECT_EQ(wave_format.size(), sizeof(WAVEFORMATEX));

  // Verify format types for the stand-alone version.
  EXPECT_TRUE(wave_format.IsPcm());
  EXPECT_FALSE(wave_format.IsFloat());
  format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
  EXPECT_TRUE(wave_format.IsFloat());
}

TEST_F(CoreAudioUtilWinTest, WaveFormatWrapperExtended) {
  // Use default constructor for WAVEFORMATEXTENSIBLE and verify that it
  // results in same size as for WAVEFORMATEX even if the size of |format_ex|
  // equals the size of WAVEFORMATEXTENSIBLE.
  WAVEFORMATEXTENSIBLE format_ex = {};
  CoreAudioUtil::WaveFormatWrapper wave_format_ex(&format_ex);
  EXPECT_FALSE(wave_format_ex.IsExtensible());
  EXPECT_EQ(wave_format_ex.size(), sizeof(WAVEFORMATEX));
  EXPECT_EQ(wave_format_ex->cbSize, 0);

  // Ensure that the extended structure has a valid format tag and that all
  // accessors work.
  format_ex.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  EXPECT_FALSE(wave_format_ex.IsExtensible());
  EXPECT_EQ(wave_format_ex.size(), sizeof(WAVEFORMATEX));
  EXPECT_EQ(wave_format_ex->wFormatTag, WAVE_FORMAT_EXTENSIBLE);
  EXPECT_EQ(wave_format_ex.get()->wFormatTag, WAVE_FORMAT_EXTENSIBLE);

  // Next, ensure that the size is valid (sum of stand-alone and extended).
  // Now the structure qualifies as extended.
  format_ex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
  EXPECT_TRUE(wave_format_ex.IsExtensible());
  EXPECT_EQ(wave_format_ex.size(), sizeof(WAVEFORMATEXTENSIBLE));
  EXPECT_TRUE(wave_format_ex.GetExtensible());
  EXPECT_EQ(wave_format_ex.GetExtensible()->Format.wFormatTag,
            WAVE_FORMAT_EXTENSIBLE);

  // Verify format types for the extended version.
  EXPECT_FALSE(wave_format_ex.IsPcm());
  format_ex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  EXPECT_TRUE(wave_format_ex.IsPcm());
  EXPECT_FALSE(wave_format_ex.IsFloat());
  format_ex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
  EXPECT_TRUE(wave_format_ex.IsFloat());
}

TEST_F(CoreAudioUtilWinTest, GetIAudioClientVersion) {
  uint32_t client_version = CoreAudioUtil::GetIAudioClientVersion();
  EXPECT_GE(client_version, 1u);
  EXPECT_LE(client_version, 3u);
}

TEST_F(CoreAudioUtilWinTest, NumberOfActiveDevices) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  int render_devices = CoreAudioUtil::NumberOfActiveDevices(eRender);
  EXPECT_GT(render_devices, 0);
  int capture_devices = CoreAudioUtil::NumberOfActiveDevices(eCapture);
  EXPECT_GT(capture_devices, 0);
  int total_devices = CoreAudioUtil::NumberOfActiveDevices(eAll);
  EXPECT_EQ(total_devices, render_devices + capture_devices);
}

TEST_F(CoreAudioUtilWinTest, CreateDeviceEnumerator) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  ComPtr<IMMDeviceEnumerator> enumerator =
      CoreAudioUtil::CreateDeviceEnumerator();
  EXPECT_TRUE(enumerator.Get());
}

TEST_F(CoreAudioUtilWinTest, GetDefaultDeviceIDs) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());
  std::string default_device_id = CoreAudioUtil::GetDefaultInputDeviceID();
  EXPECT_FALSE(default_device_id.empty());
  default_device_id = CoreAudioUtil::GetDefaultOutputDeviceID();
  EXPECT_FALSE(default_device_id.empty());
  default_device_id = CoreAudioUtil::GetCommunicationsInputDeviceID();
  EXPECT_FALSE(default_device_id.empty());
  default_device_id = CoreAudioUtil::GetCommunicationsOutputDeviceID();
  EXPECT_FALSE(default_device_id.empty());
}

TEST_F(CoreAudioUtilWinTest, CreateDefaultDevice) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  struct {
    EDataFlow flow;
    ERole role;
  } data[] = {
    {eRender, eConsole},
    {eRender, eCommunications},
    {eRender, eMultimedia},
    {eCapture, eConsole},
    {eCapture, eCommunications},
    {eCapture, eMultimedia}
  };

  // Create default devices for all flow/role combinations above.
  ComPtr<IMMDevice> audio_device;
  for (size_t i = 0; i < base::size(data); ++i) {
    audio_device = CoreAudioUtil::CreateDevice(
        AudioDeviceDescription::kDefaultDeviceId, data[i].flow, data[i].role);
    EXPECT_TRUE(audio_device.Get());
    EXPECT_EQ(data[i].flow, CoreAudioUtil::GetDataFlow(audio_device.Get()));
  }

  // Only eRender and eCapture are allowed as flow parameter.
  audio_device = CoreAudioUtil::CreateDevice(
      AudioDeviceDescription::kDefaultDeviceId, eAll, eConsole);
  EXPECT_FALSE(audio_device.Get());
}

TEST_F(CoreAudioUtilWinTest, CreateDevice) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  // Get name and ID of default device used for playback.
  ComPtr<IMMDevice> default_render_device = CoreAudioUtil::CreateDevice(
      AudioDeviceDescription::kDefaultDeviceId, eRender, eConsole);
  AudioDeviceName default_render_name;
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDeviceName(
      default_render_device.Get(), &default_render_name)));

  // Use the unique ID as input to CreateDevice() and create a corresponding
  // IMMDevice.
  ComPtr<IMMDevice> audio_device = CoreAudioUtil::CreateDevice(
      default_render_name.unique_id, EDataFlow(), ERole());
  EXPECT_TRUE(audio_device.Get());

  // Verify that the two IMMDevice interfaces represents the same endpoint
  // by comparing their unique IDs.
  AudioDeviceName device_name;
  EXPECT_TRUE(SUCCEEDED(
      CoreAudioUtil::GetDeviceName(audio_device.Get(), &device_name)));
  EXPECT_EQ(default_render_name.unique_id, device_name.unique_id);
}

TEST_F(CoreAudioUtilWinTest, GetDefaultDeviceName) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  struct {
    EDataFlow flow;
    ERole role;
  } data[] = {
    {eRender, eConsole},
    {eRender, eCommunications},
    {eCapture, eConsole},
    {eCapture, eCommunications}
  };

  // Get name and ID of default devices for all flow/role combinations above.
  ComPtr<IMMDevice> audio_device;
  AudioDeviceName device_name;
  for (size_t i = 0; i < base::size(data); ++i) {
    audio_device = CoreAudioUtil::CreateDevice(
        AudioDeviceDescription::kDefaultDeviceId, data[i].flow, data[i].role);
    EXPECT_TRUE(SUCCEEDED(
        CoreAudioUtil::GetDeviceName(audio_device.Get(), &device_name)));
    EXPECT_FALSE(device_name.device_name.empty());
    EXPECT_FALSE(device_name.unique_id.empty());
  }
}

TEST_F(CoreAudioUtilWinTest, GetAudioControllerID) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  ComPtr<IMMDeviceEnumerator> enumerator(
      CoreAudioUtil::CreateDeviceEnumerator());
  ASSERT_TRUE(enumerator.Get());

  // Enumerate all active input and output devices and fetch the ID of
  // the associated device.
  EDataFlow flows[] = { eRender , eCapture };
  for (size_t i = 0; i < base::size(flows); ++i) {
    ComPtr<IMMDeviceCollection> collection;
    ASSERT_TRUE(SUCCEEDED(enumerator->EnumAudioEndpoints(
        flows[i], DEVICE_STATE_ACTIVE, collection.GetAddressOf())));
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT j = 0; j < count; ++j) {
      ComPtr<IMMDevice> device;
      collection->Item(j, device.GetAddressOf());
      std::string controller_id(
          CoreAudioUtil::GetAudioControllerID(device.Get(), enumerator.Get()));
      EXPECT_FALSE(controller_id.empty());
    }
  }
}

TEST_F(CoreAudioUtilWinTest, GetFriendlyName) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  // Get name and ID of default device used for recording.
  ComPtr<IMMDevice> audio_device = CoreAudioUtil::CreateDevice(
      AudioDeviceDescription::kDefaultDeviceId, eCapture, eConsole);
  AudioDeviceName device_name;
  HRESULT hr = CoreAudioUtil::GetDeviceName(audio_device.Get(), &device_name);
  EXPECT_TRUE(SUCCEEDED(hr));

  // Use unique ID as input to GetFriendlyName() and compare the result
  // with the already obtained friendly name for the default capture device.
  std::string friendly_name =
      CoreAudioUtil::GetFriendlyName(device_name.unique_id, eCapture, eConsole);
  EXPECT_EQ(friendly_name, device_name.device_name);

  // Same test as above but for playback.
  audio_device = CoreAudioUtil::CreateDevice(
      AudioDeviceDescription::kDefaultDeviceId, eRender, eConsole);
  hr = CoreAudioUtil::GetDeviceName(audio_device.Get(), &device_name);
  EXPECT_TRUE(SUCCEEDED(hr));
  friendly_name =
      CoreAudioUtil::GetFriendlyName(device_name.unique_id, eRender, eConsole);
  EXPECT_EQ(friendly_name, device_name.device_name);
}

TEST_F(CoreAudioUtilWinTest, CreateClient) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  EDataFlow data[] = {eRender, eCapture};

  for (size_t i = 0; i < base::size(data); ++i) {
    ComPtr<IAudioClient> client = CoreAudioUtil::CreateClient(
        AudioDeviceDescription::kDefaultDeviceId, data[i], eConsole);
    EXPECT_TRUE(client.Get());
  }
}

TEST_F(CoreAudioUtilWinTest, CreateClient3) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable() &&
                          CoreAudioUtil::GetIAudioClientVersion() >= 3);

  EDataFlow data[] = {eRender, eCapture};

  for (size_t i = 0; i < base::size(data); ++i) {
    ComPtr<IAudioClient3> client3 = CoreAudioUtil::CreateClient3(
        AudioDeviceDescription::kDefaultDeviceId, data[i], eConsole);
    EXPECT_TRUE(client3.Get());
  }

  // Use ComPtr notation to achieve the same thing as above. ComPtr::As wraps
  // QueryInterface calls on existing COM objects. In this case we use an
  // existing IAudioClient to obtain the IAudioClient3 interface.
  for (size_t i = 0; i < base::size(data); ++i) {
    ComPtr<IAudioClient> client = CoreAudioUtil::CreateClient(
        AudioDeviceDescription::kDefaultDeviceId, data[i], eConsole);
    EXPECT_TRUE(client.Get());
    ComPtr<IAudioClient3> client3;
    EXPECT_TRUE(SUCCEEDED(client.As(&client3)));
    EXPECT_TRUE(client3.Get());
  }
}

TEST_F(CoreAudioUtilWinTest, GetSharedModeMixFormat) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  ComPtr<IAudioClient> client = CoreAudioUtil::CreateClient(
      AudioDeviceDescription::kDefaultDeviceId, eRender, eConsole);
  EXPECT_TRUE(client.Get());

  // Perform a simple sanity test of the acquired format structure.
  WAVEFORMATEXTENSIBLE format;
  EXPECT_TRUE(
      SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client.Get(), &format)));
  CoreAudioUtil::WaveFormatWrapper wformat(&format);
  EXPECT_GE(wformat->nChannels, 1);
  EXPECT_GE(wformat->nSamplesPerSec, 8000u);
  EXPECT_GE(wformat->wBitsPerSample, 16);
  if (wformat.IsExtensible()) {
    EXPECT_EQ(wformat->wFormatTag, WAVE_FORMAT_EXTENSIBLE);
    EXPECT_GE(wformat->cbSize, 22);
    EXPECT_GE(wformat.GetExtensible()->Samples.wValidBitsPerSample, 16);
  } else {
    EXPECT_EQ(wformat->cbSize, 0);
  }
}

TEST_F(CoreAudioUtilWinTest, IsChannelLayoutSupported) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  // The preferred channel layout should always be supported. Being supported
  // means that it is possible to initialize a shared mode stream with the
  // particular channel layout.
  AudioParameters mix_params;
  HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(
      AudioDeviceDescription::kDefaultDeviceId, true, &mix_params);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(mix_params.IsValid());
  EXPECT_TRUE(CoreAudioUtil::IsChannelLayoutSupported(
      std::string(), eRender, eConsole, mix_params.channel_layout()));

  // Check if it is possible to modify the channel layout to stereo for a
  // device which reports that it prefers to be opened up in an other
  // channel configuration.
  if (mix_params.channel_layout() != CHANNEL_LAYOUT_STEREO) {
    ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
    // TODO(henrika): it might be too pessimistic to assume false as return
    // value here.
    EXPECT_FALSE(CoreAudioUtil::IsChannelLayoutSupported(
        std::string(), eRender, eConsole, channel_layout));
  }
}

TEST_F(CoreAudioUtilWinTest, GetDevicePeriod) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  EDataFlow data[] = {eRender, eCapture};

  // Verify that the device periods are valid for the default render and
  // capture devices.
  for (size_t i = 0; i < base::size(data); ++i) {
    ComPtr<IAudioClient> client;
    REFERENCE_TIME shared_time_period = 0;
    REFERENCE_TIME exclusive_time_period = 0;
    client = CoreAudioUtil::CreateClient(
        AudioDeviceDescription::kDefaultDeviceId, data[i], eConsole);
    EXPECT_TRUE(client.Get());
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDevicePeriod(
        client.Get(), AUDCLNT_SHAREMODE_SHARED, &shared_time_period)));
    EXPECT_GT(shared_time_period, 0);
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDevicePeriod(
        client.Get(), AUDCLNT_SHAREMODE_EXCLUSIVE, &exclusive_time_period)));
    EXPECT_GT(exclusive_time_period, 0);
    EXPECT_LE(exclusive_time_period, shared_time_period);
  }
}

TEST_F(CoreAudioUtilWinTest, GetPreferredAudioParameters) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  EDataFlow data[] = {eRender, eCapture};

  // Verify that the preferred audio parameters are OK for the default render
  // and capture devices.
  for (size_t i = 0; i < base::size(data); ++i) {
    AudioParameters params;
    const bool is_output_device = (data[i] == eRender);
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
        AudioDeviceDescription::kDefaultDeviceId, is_output_device, &params)));
    EXPECT_TRUE(params.IsValid());
    if (!is_output_device) {
      // Loopack devices are supported for input streams.
      EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
          AudioDeviceDescription::kLoopbackInputDeviceId, is_output_device,
          &params)));
      EXPECT_TRUE(params.IsValid());
    }
  }
}

TEST_F(CoreAudioUtilWinTest, GetChannelConfig) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  EDataFlow data_flows[] = {eRender, eCapture};

  for (auto data_flow : data_flows) {
    ChannelConfig config1 =
        CoreAudioUtil::GetChannelConfig(std::string(), data_flow);
    EXPECT_NE(config1, CHANNEL_LAYOUT_NONE);
    EXPECT_NE(config1, CHANNEL_LAYOUT_UNSUPPORTED);
    ChannelConfig config2 = CoreAudioUtil::GetChannelConfig(
        AudioDeviceDescription::kDefaultDeviceId, data_flow);
    EXPECT_EQ(config1, config2);
    // For loopback input devices, verify that the channel configuration is
    // same as for the default output device.
    if (data_flow == eCapture) {
      config1 = CoreAudioUtil::GetChannelConfig(
          AudioDeviceDescription::kLoopbackInputDeviceId, data_flow);
      config2 = CoreAudioUtil::GetChannelConfig(
          AudioDeviceDescription::kDefaultDeviceId, eRender);
      EXPECT_EQ(config1, config2);
    }
  }
}

TEST_F(CoreAudioUtilWinTest, SharedModeInitialize) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  ComPtr<IAudioClient> client;
  client = CoreAudioUtil::CreateClient(AudioDeviceDescription::kDefaultDeviceId,
                                       eRender, eConsole);
  EXPECT_TRUE(client.Get());

  WAVEFORMATEXTENSIBLE format;
  EXPECT_TRUE(
      SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client.Get(), &format)));

  // Perform a shared-mode initialization without event-driven buffer handling.
  uint32_t endpoint_buffer_size = 0;
  HRESULT hr = CoreAudioUtil::SharedModeInitialize(
      client.Get(), &format, NULL, 0, &endpoint_buffer_size, NULL);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(endpoint_buffer_size, 0u);

  // It is only possible to create a client once.
  hr = CoreAudioUtil::SharedModeInitialize(client.Get(), &format, NULL, 0,
                                           &endpoint_buffer_size, NULL);
  EXPECT_FALSE(SUCCEEDED(hr));
  EXPECT_EQ(hr, AUDCLNT_E_ALREADY_INITIALIZED);

  // Verify that it is possible to reinitialize the client after releasing it.
  client = CoreAudioUtil::CreateClient(AudioDeviceDescription::kDefaultDeviceId,
                                       eRender, eConsole);
  EXPECT_TRUE(client.Get());
  hr = CoreAudioUtil::SharedModeInitialize(client.Get(), &format, NULL, 0,
                                           &endpoint_buffer_size, NULL);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(endpoint_buffer_size, 0u);

  // Use a non-supported format and verify that initialization fails.
  // A simple way to emulate an invalid format is to use the shared-mode
  // mixing format and modify the preferred sample.
  client = CoreAudioUtil::CreateClient(AudioDeviceDescription::kDefaultDeviceId,
                                       eRender, eConsole);
  EXPECT_TRUE(client.Get());
  format.Format.nSamplesPerSec = format.Format.nSamplesPerSec + 1;
  EXPECT_FALSE(CoreAudioUtil::IsFormatSupported(
      client.Get(), AUDCLNT_SHAREMODE_SHARED, &format));
  hr = CoreAudioUtil::SharedModeInitialize(client.Get(), &format, NULL, 0,
                                           &endpoint_buffer_size, NULL);
  EXPECT_TRUE(FAILED(hr));
  EXPECT_EQ(hr, E_INVALIDARG);

  // Finally, perform a shared-mode initialization using event-driven buffer
  // handling. The event handle will be signaled when an audio buffer is ready
  // to be processed by the client (not verified here).
  // The event handle should be in the nonsignaled state.
  base::win::ScopedHandle event_handle(::CreateEvent(NULL, TRUE, FALSE, NULL));
  client = CoreAudioUtil::CreateClient(AudioDeviceDescription::kDefaultDeviceId,
                                       eRender, eConsole);
  EXPECT_TRUE(client.Get());
  EXPECT_TRUE(
      SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client.Get(), &format)));
  EXPECT_TRUE(CoreAudioUtil::IsFormatSupported(
      client.Get(), AUDCLNT_SHAREMODE_SHARED, &format));
  hr = CoreAudioUtil::SharedModeInitialize(client.Get(), &format,
                                           event_handle.Get(), 0,
                                           &endpoint_buffer_size, NULL);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(endpoint_buffer_size, 0u);
}

TEST_F(CoreAudioUtilWinTest, CreateRenderAndCaptureClients) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  EDataFlow data[] = {eRender, eCapture};

  WAVEFORMATEXTENSIBLE format;
  uint32_t endpoint_buffer_size = 0;

  for (size_t i = 0; i < base::size(data); ++i) {
    ComPtr<IAudioClient> client;
    ComPtr<IAudioRenderClient> render_client;
    ComPtr<IAudioCaptureClient> capture_client;

    client = CoreAudioUtil::CreateClient(
        AudioDeviceDescription::kDefaultDeviceId, data[i], eConsole);
    EXPECT_TRUE(client.Get());
    EXPECT_TRUE(SUCCEEDED(
        CoreAudioUtil::GetSharedModeMixFormat(client.Get(), &format)));
    if (data[i] == eRender) {
      // It is not possible to create a render client using an unitialized
      // client interface.
      render_client = CoreAudioUtil::CreateRenderClient(client.Get());
      EXPECT_FALSE(render_client.Get());

      // Do a proper initialization and verify that it works this time.
      CoreAudioUtil::SharedModeInitialize(client.Get(), &format, NULL, 0,
                                          &endpoint_buffer_size, NULL);
      render_client = CoreAudioUtil::CreateRenderClient(client.Get());
      EXPECT_TRUE(render_client.Get());
      EXPECT_GT(endpoint_buffer_size, 0u);
    } else if (data[i] == eCapture) {
      // It is not possible to create a capture client using an unitialized
      // client interface.
      capture_client = CoreAudioUtil::CreateCaptureClient(client.Get());
      EXPECT_FALSE(capture_client.Get());

      // Do a proper initialization and verify that it works this time.
      CoreAudioUtil::SharedModeInitialize(client.Get(), &format, NULL, 0,
                                          &endpoint_buffer_size, NULL);
      capture_client = CoreAudioUtil::CreateCaptureClient(client.Get());
      EXPECT_TRUE(capture_client.Get());
      EXPECT_GT(endpoint_buffer_size, 0u);
    }
  }
}

TEST_F(CoreAudioUtilWinTest, FillRenderEndpointBufferWithSilence) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  // Create default clients using the default mixing format for shared mode.
  ComPtr<IAudioClient> client(CoreAudioUtil::CreateClient(
      AudioDeviceDescription::kDefaultDeviceId, eRender, eConsole));
  EXPECT_TRUE(client.Get());

  WAVEFORMATEXTENSIBLE format;
  uint32_t endpoint_buffer_size = 0;
  EXPECT_TRUE(
      SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client.Get(), &format)));
  CoreAudioUtil::SharedModeInitialize(client.Get(), &format, NULL, 0,
                                      &endpoint_buffer_size, NULL);
  EXPECT_GT(endpoint_buffer_size, 0u);

  ComPtr<IAudioRenderClient> render_client(
      CoreAudioUtil::CreateRenderClient(client.Get()));
  EXPECT_TRUE(render_client.Get());

  // The endpoint audio buffer should not be filled up by default after being
  // created.
  UINT32 num_queued_frames = 0;
  client->GetCurrentPadding(&num_queued_frames);
  EXPECT_EQ(num_queued_frames, 0u);

  // Fill it up with zeros and verify that the buffer is full.
  // It is not possible to verify that the actual data consists of zeros
  // since we can't access data that has already been sent to the endpoint
  // buffer.
  EXPECT_TRUE(CoreAudioUtil::FillRenderEndpointBufferWithSilence(
      client.Get(), render_client.Get()));
  client->GetCurrentPadding(&num_queued_frames);
  EXPECT_EQ(num_queued_frames, endpoint_buffer_size);
}

// This test can only run on a machine that has audio hardware
// that has both input and output devices.
TEST_F(CoreAudioUtilWinTest, GetMatchingOutputDeviceID) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  bool found_a_pair = false;

  ComPtr<IMMDeviceEnumerator> enumerator(
      CoreAudioUtil::CreateDeviceEnumerator());
  ASSERT_TRUE(enumerator.Get());

  // Enumerate all active input and output devices and fetch the ID of
  // the associated device.
  ComPtr<IMMDeviceCollection> collection;
  ASSERT_TRUE(SUCCEEDED(enumerator->EnumAudioEndpoints(
      eCapture, DEVICE_STATE_ACTIVE, collection.GetAddressOf())));
  UINT count = 0;
  collection->GetCount(&count);
  for (UINT i = 0; i < count && !found_a_pair; ++i) {
    ComPtr<IMMDevice> device;
    collection->Item(i, device.GetAddressOf());
    base::win::ScopedCoMem<WCHAR> wide_id;
    device->GetId(&wide_id);
    std::string id;
    base::WideToUTF8(wide_id, wcslen(wide_id), &id);
    found_a_pair = !CoreAudioUtil::GetMatchingOutputDeviceID(id).empty();
  }

  EXPECT_TRUE(found_a_pair);
}

TEST_F(CoreAudioUtilWinTest, CheckGetPreferredAudioParametersUMAStats) {
  base::HistogramTester tester;
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  // Check that when input stream parameters are created, hr values are not
  // erroneously tracked in output stream parameters UMA histograms
  AudioParameters input_params;
  HRESULT hr = CoreAudioUtil::GetPreferredAudioParameters(
      AudioDeviceDescription::kDefaultDeviceId, false, &input_params);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(input_params.IsValid());
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "CreateDeviceEnumeratorResult",
      0);
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "CreateDeviceResult",
      0);
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "CreateClientResult",
      0);
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "GetMixFormatResult",
      0);
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "GetDevicePeriodResult",
      0);

  // Check that when output stream parameters are created, hr values for all
  // expected steps are tracked in UMA histograms
  AudioParameters output_params;
  hr = CoreAudioUtil::GetPreferredAudioParameters(
      AudioDeviceDescription::kDefaultDeviceId, true, &output_params);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_TRUE(output_params.IsValid());

  AudioParameters::HardwareCapabilities output_hardware_capabilities =
      output_params.hardware_capabilities().value_or(
          AudioParameters::HardwareCapabilities());

  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "CreateDeviceEnumeratorResult",
      1);
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "CreateDeviceResult",
      1);
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "CreateClientResult",
      1);
  tester.ExpectTotalCount(
      "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
      "GetMixFormatResult",
      1);

  // If we have a min_frames_per_buffer then it came from the new API.
  if (!output_hardware_capabilities.min_frames_per_buffer) {
    tester.ExpectTotalCount(
        "Media.AudioOutputStreamProxy.GetPreferredOutputStreamParametersWin."
        "GetDevicePeriodResult",
        1);
  }
}

TEST_F(CoreAudioUtilWinTest, SharedModeLowerBufferSize) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  AudioParameters params;
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(
      AudioDeviceDescription::kDefaultDeviceId, true, &params)));

  AudioParameters::HardwareCapabilities hardware_capabilities =
      params.hardware_capabilities().value_or(
          AudioParameters::HardwareCapabilities());

  // If min_frames_per_buffer is 0 then we don't support the IAudioClient3
  // low-latency API.
  ABORT_AUDIO_TEST_IF_NOT(hardware_capabilities.min_frames_per_buffer > 0);

  // Nothing to test if the default is already the minimum.
  ABORT_AUDIO_TEST_IF_NOT(hardware_capabilities.min_frames_per_buffer <
                          params.frames_per_buffer());

  ComPtr<IAudioClient> default_client;
  default_client = CoreAudioUtil::CreateClient(
      AudioDeviceDescription::kDefaultDeviceId, eRender, eConsole);
  EXPECT_TRUE(default_client.Get());

  WAVEFORMATEXTENSIBLE format;
  EXPECT_TRUE(SUCCEEDED(
      CoreAudioUtil::GetSharedModeMixFormat(default_client.Get(), &format)));

  uint32_t default_endpoint_buffer_size = 0;
  HRESULT hr = CoreAudioUtil::SharedModeInitialize(
      default_client.Get(), &format, NULL, 0, &default_endpoint_buffer_size,
      NULL);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(default_endpoint_buffer_size, 0u);

  ComPtr<IAudioClient> low_latency_client;
  low_latency_client = CoreAudioUtil::CreateClient(
      AudioDeviceDescription::kDefaultDeviceId, eRender, eConsole);
  EXPECT_TRUE(low_latency_client.Get());

  uint32_t low_latency_endpoint_buffer_size = 0;
  hr = CoreAudioUtil::SharedModeInitialize(
      low_latency_client.Get(), &format, NULL,
      hardware_capabilities.min_frames_per_buffer,
      &low_latency_endpoint_buffer_size, NULL);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(low_latency_endpoint_buffer_size, 0u);

  EXPECT_LT(low_latency_endpoint_buffer_size, default_endpoint_buffer_size);
}

}  // namespace media
