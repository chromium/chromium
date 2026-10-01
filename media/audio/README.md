# //media/audio

This directory contains Chromium's low-level audio input and output code:
platform-specific streams, device enumeration, and the client-side classes used
to move real-time audio between processes.

## Architecture

Most of this directory talks directly to OS audio APIs, and can't be used from a
sandboxed renderer. Instead:

* `AudioManager` and the platform streams run in the Audio Service
  ([`//services/audio`](../../services/audio/README.md)). By default, the Audio
  Service runs in its own utility process on Windows, macOS and Linux, and in
  the browser process on other platforms.
* Renderers use [`AudioOutputDevice`](audio_output_device.h) (an
  `AudioRendererSink`) for playback, and
  [`AudioInputDevice`](audio_input_device.h) (an `AudioCapturerSource`) for
  capture. Control messages go through the `AudioOutputIPC` and `AudioInputIPC`
  interfaces, which are implemented over Mojo. Audio data goes through shared
  memory, signaled over a `base::SyncSocket` and serviced on an
  [`AudioDeviceThread`](audio_device_thread.h).

## Key classes

* [`AudioManager`](audio_manager.h): Per-process singleton which enumerates
  devices and creates `AudioOutputStream`s and `AudioInputStream`s.
* [`AudioOutputStream` and `AudioInputStream`](audio_io.h): The interfaces
  implemented by each platform. Output uses a pull model
  (`AudioSourceCallback::OnMoreData()`), and input uses a push model
  (`AudioInputCallback::OnData()`).
* [`AudioOutputResampler`](audio_output_resampler.h),
  [`AudioOutputDispatcherImpl`](audio_output_dispatcher_impl.h) and
  [`AudioOutputProxy`](audio_output_proxy.h): Sit between clients and physical
  output streams, and handle format conversion and stream reuse.

## Real-time threads

Audio callbacks (`OnMoreData()`, `OnData()` and
`AudioDeviceThread::Callback::Process()`) typically run on high priority
threads, such as threads created with `base::ThreadType::kRealtimeAudio`, or
threads owned by the OS audio stack. Missing a deadline causes audible glitches.
Code running in these callbacks should:

* Not block: no I/O, synchronous IPC, waiting on events, or contended locks.
* Not allocate: heap allocations can take locks. Allocate buffers ahead of time
  instead (e.g. when opening or initializing a stream).
* Be wary of `raw_ptr`, `base::raw_span`, and `base::SpanReader` or
  `base::SpanWriter` for per-sample operations. Their overhead adds up in tight
  loops.
* Not call back into the stream from `OnError()`, since some implementations
  hold locks while calling it. Post a task instead.
* Not rely on thread-local storage, or alter the thread itself (e.g. by
  initializing COM).

## Platform code

* `android/`: AAudio, with OpenSL ES as a fallback. `AudioTrack` is used for
  bitstream (passthrough) output.
* `apple/`: Audio Unit (AUHAL) based output and input, shared by macOS and iOS.
* `mac/`: macOS-specific AVFoundation output, loopback capture (Core Audio taps
  and ScreenCaptureKit) and device monitoring.
* `ios/`: iOS audio session management.
* `win/`: WASAPI for low latency input, output and loopback, and waveOut for
  other output.
* `cras/`: ChromeOS Audio Server (CRAS).
* `fuchsia/`: `fuchsia.media` `AudioRenderer` and `AudioCapturer`.
* `linux/`, `pulse/` and `alsa/`: PulseAudio, with ALSA as a fallback.

## Debugging

* The Audio tab in `chrome://media-internals` shows active audio controllers
  and streams, with their properties.
* `chrome://webrtc-internals` can enable diagnostic audio recordings.
