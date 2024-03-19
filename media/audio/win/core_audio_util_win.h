// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility methods for the Core Audio API on Windows.
// Always ensure that Core Audio is supported before using these methods.
// Use media::CoreAudioUtil::IsSupported() for this purpose.
// Also, all methods must be called on a valid COM thread. This can be done
// by using the base::win::ScopedCOMInitializer helper class.

#ifndef MEDIA_AUDIO_WIN_CORE_AUDIO_UTIL_WIN_H_
#define MEDIA_AUDIO_WIN_CORE_AUDIO_UTIL_WIN_H_

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <stdint.h>
#include <wrl/client.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/audio/audio_device_name.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

using ChannelConfig = uint32_t;

class MEDIA_EXPORT CoreAudioUtil {
 public:
  // Helper class which automates casting between WAVEFORMATEX and
  // WAVEFORMATEXTENSIBLE raw pointers using implicit constructors and
  // operator overloading. Note that, no memory is allocated by this utility
  // structure. It only serves as a handle (or a wrapper) of the structure
  // provided to it at construction.
  class MEDIA_EXPORT WaveFormatWrapper {
   public:
    WaveFormatWrapper(WAVEFORMATEXTENSIBLE* p)
        : ptr_(reinterpret_cast<WAVEFORMATEX*>(p)) {}
    WaveFormatWrapper(WAVEFORMATEX* p) : ptr_(p) {}
    ~WaveFormatWrapper() = default;

    operator WAVEFORMATEX*() const { return ptr_; }
    WAVEFORMATEX* operator->() const { return ptr_; }
    WAVEFORMATEX* get() const { return ptr_; }
    WAVEFORMATEXTENSIBLE* GetExtensible() const;

    bool IsExtensible() const;
    bool IsPcm() const;
    bool IsFloat() const;
    size_t size() const;

   private:
    raw_ptr<WAVEFORMATEX> ptr_;
  };

  CoreAudioUtil() = delete;
  CoreAudioUtil(const CoreAudioUtil&) = delete;
  CoreAudioUtil& operator=(const CoreAudioUtil&) = delete;

  // Returns true if Windows Core Audio is supported.
  // Always verify that this method returns true before using any of the
  // methods in this class.
  // WARNING: This function must be called once from the main thread before
  // it is safe to call from other threads.
  static bool IsSupported();

  // Converts a COM error into a human-readable string.
  static std::string ErrorToString(HRESULT hresult);

  // Prints/logs all fields of the format structure in |format|.
  // Also supports extended versions (WAVEFORMATEXTENSIBLE).
  static std::string WaveFormatToString(WaveFormatWrapper format);

  // Converts between reference time to base::TimeDelta.
  // One reference-time unit is 100 nanoseconds.
  // Example: double s = RefererenceTimeToTimeDelta(t).InMillisecondsF();
  static base::TimeDelta ReferenceTimeToTimeDelta(REFERENCE_TIME time);

  // Returns AUDCLNT_SHAREMODE_EXCLUSIVE if --enable-exclusive-mode is used
  // as command-line flag and AUDCLNT_SHAREMODE_SHARED otherwise (default).
  static AUDCLNT_SHAREMODE GetShareMode();

  // The Windows Multimedia Device (MMDevice) API enables audio clients to
  // discover audio endpoint devices and determine their capabilities.

  // Number of active audio devices in the specified flow data flow direction.
  // Set |data_flow| to eAll to retrieve the total number of active audio
  // devices.
  static int NumberOfActiveDevices(EDataFlow data_flow);

  // Creates an IMMDeviceEnumerator interface which provides methods for
  // enumerating audio endpoint devices.
  static Microsoft::WRL::ComPtr<IMMDeviceEnumerator> CreateDeviceEnumerator();

  // Create an endpoint device specified by |device_id| or a default device
  // specified by data-flow direction and role if
  // AudioDeviceDescription::IsDefaultDevice(|device_id|).
  static Microsoft::WRL::ComPtr<IMMDevice>
  CreateDevice(const std::string& device_id, EDataFlow data_flow, ERole role);

  // These functions return the device id of the default or communications
  // input/output device, or an empty string if no such device exists or if the
  // device has been disabled.
  static std::string GetDefaultInputDeviceID();
  static std::string GetDefaultOutputDeviceID();
  static std::string GetCommunicationsInputDeviceID();
  static std::string GetCommunicationsOutputDeviceID();

  // Returns the unique ID and user-friendly name of a given endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}", and
  //          "Microphone (Realtek High Definition Audio)".
  static HRESULT GetDeviceName(IMMDevice* device, AudioDeviceName* name);

  // Returns the device ID/path of the controller (a.k.a. physical device that
  // |device| is connected to.  This ID will be the same for all devices from
  // the same controller so it is useful for doing things like determining
  // whether a set of output and input devices belong to the same controller.
  // The device enumerator is required as well as the device itself since
  // looking at the device topology is required and we need to open up
  // associated devices to determine the controller id.
  // If the ID could not be determined for some reason, an empty string is
  // returned.
  static std::string GetAudioControllerID(IMMDevice* device,
      IMMDeviceEnumerator* enumerator);

  // Accepts an id of an input device and finds a matching output device id.
  // If the associated hardware does not have an audio output device (e.g.
  // a webcam with a mic), an empty string is returned.
  static std::string GetMatchingOutputDeviceID(
      const std::string& input_device_id);

  // Gets the user-friendly name of the endpoint device which is represented
  // by a unique id in |device_id|.
  static std::string GetFriendlyName(const std::string& device_id,
                                     EDataFlow data_flow,
                                     ERole role);

  // Query if the audio device is a rendering device or a capture device.
  static EDataFlow GetDataFlow(IMMDevice* device);

  // The Windows Audio Session API (WASAPI) enables client applications to
  // manage the flow of audio data between the application and an audio endpoint
  // device.

  // Create an IAudioClient instance for a specific device or the default
  // device if AudioDeviceDescription::IsDefaultDevice(device_id).
  static Microsoft::WRL::ComPtr<IAudioClient>
  CreateClient(const std::string& device_id, EDataFlow data_flow, ERole role);
  static Microsoft::WRL::ComPtr<IAudioClient3>
  CreateClient3(const std::string& device_id, EDataFlow data_flow, ERole role);

  // Get the mix format that the audio engine uses internally for processing
  // of shared-mode streams. This format is not necessarily a format that the
  // audio endpoint device supports. The WAVEFORMATEXTENSIBLE structure can
  // specify both the mapping of channels to speakers and the number of bits of
  // precision in each sample. The first member of the WAVEFORMATEXTENSIBLE
  // structure is a WAVEFORMATEX structure and its wFormatTag will be set to
  // WAVE_FORMAT_EXTENSIBLE if the output structure is extended.
  // FormatIsExtensible() can be used to determine if that is the case or not.
  static HRESULT GetSharedModeMixFormat(IAudioClient* client,
                                        WAVEFORMATEXTENSIBLE* format);

  // Returns true if the specified |client| supports the format in |format|
  // for the given |share_mode| (shared or exclusive).
  static bool IsFormatSupported(IAudioClient* client,
                                AUDCLNT_SHAREMODE share_mode,
                                WaveFormatWrapper format);

  // Returns true if the specified |channel_layout| is supported for the
  // default IMMDevice where flow direction and role is define by |data_flow|
  // and |role|. If this method returns true for a certain channel layout, it
  // means that SharedModeInitialize() will succeed using a format based on
  // the preferred format where the channel layout has been modified.
  static bool IsChannelLayoutSupported(const std::string& device_id,
                                       EDataFlow data_flow,
                                       ERole role,
                                       ChannelLayout channel_layout);

  // For a shared-mode stream, the audio engine periodically processes the
  // data in the endpoint buffer at the period obtained in |device_period|.
  // For an exclusive mode stream, |device_period| corresponds to the minimum
  // time interval between successive processing by the endpoint device.
  // This period plus the stream latency between the buffer and endpoint device
  // represents the minimum possible latency that an audio application can
  // achieve. The time in |device_period| is expressed in 100-nanosecond units.
  static HRESULT GetDevicePeriod(IAudioClient* client,
                                 AUDCLNT_SHAREMODE share_mode,
                                 REFERENCE_TIME* device_period);

  // Get the preferred audio parameters for the given |device_id|. The acquired
  // values should only be utilized for shared mode streamed since there are no
  // preferred settings for an exclusive mode stream.
  static HRESULT GetPreferredAudioParameters(const std::string& device_id,
                                             bool is_output_device,
                                             AudioParameters* params,
                                             bool is_offload_stream = false);

  // Retrieves an integer mask which corresponds to the channel layout the
  // audio engine uses for its internal processing/mixing of shared-mode
  // streams. This mask indicates which channels are present in the multi-
  // channel stream. The least significant bit corresponds with the Front Left
  // speaker, the next least significant bit corresponds to the Front Right
  // speaker, and so on, continuing in the order defined in KsMedia.h.
  // See http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx
  // for more details.
  static ChannelConfig GetChannelConfig(const std::string& device_id,
                                        EDataFlow data_flow);

  // After activating an IAudioClient interface on an audio endpoint device,
  // the client must initialize it once, and only once, to initialize the audio
  // stream between the client and the device. In shared mode, the client
  // connects indirectly through the audio engine which does the mixing.
  // In exclusive mode, the client connects directly to the audio hardware.
  // If a valid event is provided in |event_handle|, the client will be
  // initialized for event-driven buffer handling. If |event_handle| is set to
  // NULL, event-driven buffer handling is not utilized.
  // If |enable_audio_offload| is true, the buffer will be set to a larger one
  // as required by audio offloading feature.
  // This function will initialize the audio client as part of the default
  // audio session if NULL is passed for |session_guid|, otherwise the client
  // will be associated with the specified session.
  static HRESULT SharedModeInitialize(IAudioClient* client,
                                      WaveFormatWrapper format,
                                      HANDLE event_handle,
                                      uint32_t requested_buffer_size,
                                      uint32_t* endpoint_buffer_size,
                                      const GUID* session_guid,
                                      bool enable_audio_offload = false);

  // Create an IAudioRenderClient client for an existing IAudioClient given by
  // |client|. The IAudioRenderClient interface enables a client to write
  // output data to a rendering endpoint buffer.
  static Microsoft::WRL::ComPtr<IAudioRenderClient> CreateRenderClient(
      IAudioClient* client);

  // Create an IAudioCaptureClient client for an existing IAudioClient given by
  // |client|. The IAudioCaptureClient interface enables a client to read
  // input data from a capture endpoint buffer.
  static Microsoft::WRL::ComPtr<IAudioCaptureClient> CreateCaptureClient(
      IAudioClient* client);

  // Fills up the endpoint rendering buffer with silence for an existing
  // IAudioClient given by |client| and a corresponding IAudioRenderClient
  // given by |render_client|.
  static bool FillRenderEndpointBufferWithSilence(
      IAudioClient* client,
      IAudioRenderClient* render_client);

  // Enable audio offload on the client if supported. Returning true only when
  // the client supports audio offload, and at the same time the offload pin
  // for client's output is selected. For more details of audio offload, refer
  // to:
  // https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/hardware-offloaded-audio-processing
  static bool EnableOffloadForClient(IAudioClient* client);

  // Check if audio offload can be enabled for client.
  static bool IsAudioOffloadSupported(IAudioClient* client);
};

// The special audio session identifier we use when opening up the default
// communication device.  This has the effect that a separate volume control
// will be shown in the system's volume mixer and control over ducking and
// visually observing the behavior of ducking, is easier.
// Use with |SharedModeInitialize|.
extern const GUID kCommunicationsSessionId;

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_CORE_AUDIO_UTIL_WIN_H_
