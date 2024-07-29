// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/win/waveout_output_win.h"

#include <atomic>

#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_io.h"
#include "media/audio/win/audio_manager_win.h"

namespace media {

// Some general thoughts about the waveOut API which is badly documented :
// - We use CALLBACK_EVENT mode in which XP signals events such as buffer
//   releases.
// - We use RegisterWaitForSingleObject() so one of threads in thread pool
//   automatically calls our callback that feeds more data to Windows.
// - Windows does not provide a way to query if the device is playing or paused
//   thus it forces you to maintain state, which naturally is not exactly
//   synchronized to the actual device state.

// Sixty four MB is the maximum buffer size per AudioOutputStream.
static const uint32_t kMaxOpenBufferSize = 1024 * 1024 * 64;

// See Also
// http://www.thx.com/consumer/home-entertainment/home-theater/surround-sound-speaker-set-up/
// http://en.wikipedia.org/wiki/Surround_sound

static const int kMaxChannelsToMask = 8;
static const unsigned int kChannelsToMask[kMaxChannelsToMask + 1] = {
  0,
  // 1 = Mono
  SPEAKER_FRONT_CENTER,
  // 2 = Stereo
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT,
  // 3 = Stereo + Center
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER,
  // 4 = Quad
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
  // 5 = 5.0
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
  // 6 = 5.1
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,
  // 7 = 6.1
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
  SPEAKER_BACK_CENTER,
  // 8 = 7.1
  SPEAKER_FRONT_LEFT  | SPEAKER_FRONT_RIGHT |
  SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
  SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
  SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT
  // TODO(fbarchard): Add additional masks for 7.2 and beyond.
};

inline size_t PCMWaveOutAudioOutputStream::BufferSize() const {
  // Round size of buffer up to the nearest 16 bytes.
  return (sizeof(WAVEHDR) + buffer_size_ + 15u) & static_cast<size_t>(~15);
}

inline WAVEHDR* PCMWaveOutAudioOutputStream::GetBuffer(int n) const {
  DCHECK_GE(n, 0);
  DCHECK_LT(n, num_buffers_);
  return reinterpret_cast<WAVEHDR*>(&buffers_[n * BufferSize()]);
}

constexpr SampleFormat kSampleFormat = kSampleFormatS16;

PCMWaveOutAudioOutputStream::PCMWaveOutAudioOutputStream(
    AudioManagerWin* manager,
    const AudioParameters& params,
    int num_buffers,
    UINT device_id)
    : state_(PCMA_BRAND_NEW),
      manager_(manager),
      callback_(nullptr),
      num_buffers_(num_buffers),
      buffer_size_(params.GetBytesPerBuffer(kSampleFormat)),
      volume_(1),
      channels_(params.channels()),
      pending_bytes_(0),
      device_id_(device_id),
      waveout_(NULL),
      waiting_handle_(NULL),
      audio_bus_(AudioBus::Create(params)) {
  format_.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format_.Format.nChannels = params.channels();
  format_.Format.nSamplesPerSec = params.sample_rate();
  format_.Format.wBitsPerSample = SampleFormatToBitsPerChannel(kSampleFormat);
  format_.Format.cbSize = sizeof(format_) - sizeof(WAVEFORMATEX);
  // The next are computed from above.
  format_.Format.nBlockAlign = (format_.Format.nChannels *
                                format_.Format.wBitsPerSample) / 8;
  format_.Format.nAvgBytesPerSec = format_.Format.nBlockAlign *
                                   format_.Format.nSamplesPerSec;
  if (params.channels() > kMaxChannelsToMask) {
    format_.dwChannelMask = kChannelsToMask[kMaxChannelsToMask];
  } else {
    format_.dwChannelMask = kChannelsToMask[params.channels()];
  }
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
  format_.Samples.wValidBitsPerSample = format_.Format.wBitsPerSample;
}

PCMWaveOutAudioOutputStream::~PCMWaveOutAudioOutputStream() {
  DCHECK(NULL == waveout_);
}

bool PCMWaveOutAudioOutputStream::Open() {
  if (state_ != PCMA_BRAND_NEW)
    return false;
  if (BufferSize() * num_buffers_ > kMaxOpenBufferSize)
    return false;
  if (num_buffers_ < 2 || num_buffers_ > 5)
    return false;

  // Create buffer event.
  buffer_event_.Set(::CreateEvent(NULL,    // Security attributes.
                                  FALSE,   // It will auto-reset.
                                  FALSE,   // Initial state.
                                  NULL));  // No name.
  if (!buffer_event_.Get())
    return false;

  // Open the device.
  // We'll be getting buffer_event_ events when it's time to refill the buffer.
  MMRESULT result = ::waveOutOpen(
      &waveout_,
      device_id_,
      reinterpret_cast<LPCWAVEFORMATEX>(&format_),
      reinterpret_cast<DWORD_PTR>(buffer_event_.Get()),
      NULL,
      CALLBACK_EVENT);
  if (result != MMSYSERR_NOERROR)
    return false;

  SetupBuffers();
  state_ = PCMA_READY;
  return true;
}

void PCMWaveOutAudioOutputStream::SetupBuffers() {
  buffers_ = std::make_unique<char[]>(BufferSize() * num_buffers_);
  for (int ix = 0; ix != num_buffers_; ++ix) {
    WAVEHDR* buffer = GetBuffer(ix);
    buffer->lpData = reinterpret_cast<char*>(buffer) + sizeof(WAVEHDR);
    buffer->dwBufferLength = buffer_size_;
    buffer->dwBytesRecorded = 0;
    buffer->dwFlags = WHDR_DONE;
    buffer->dwLoops = 0;
    // Tell windows sound drivers about our buffers. Not documented what
    // this does but we can guess that causes the OS to keep a reference to
    // the memory pages so the driver can use them without worries.
    ::waveOutPrepareHeader(waveout_, buffer, sizeof(WAVEHDR));
  }
}

void PCMWaveOutAudioOutputStream::FreeBuffers() {
  for (int ix = 0; ix != num_buffers_; ++ix) {
    ::waveOutUnprepareHeader(waveout_, GetBuffer(ix), sizeof(WAVEHDR));
  }
  buffers_.reset();
}

// Initially we ask the source to fill up all audio buffers. If we don't do
// this then we would always get the driver callback when it is about to run
// samples and that would leave too little time to react.
void PCMWaveOutAudioOutputStream::Start(AudioSourceCallback* callback) {
  if (state_ != PCMA_READY)
    return;
  callback_ = callback;

  // Reset buffer event, it can be left in the arbitrary state if we
  // previously stopped the stream. Can happen because we are stopping
  // callbacks before stopping playback itself.
  if (!::ResetEvent(buffer_event_.Get())) {
    HandleError(MMSYSERR_ERROR);
    return;
  }

  // Start watching for buffer events.
  if (!::RegisterWaitForSingleObject(&waiting_handle_,
                                     buffer_event_.Get(),
                                     &BufferCallback,
                                     this,
                                     INFINITE,
                                     WT_EXECUTEDEFAULT)) {
    HandleError(MMSYSERR_ERROR);
    waiting_handle_ = NULL;
    return;
  }

  state_ = PCMA_PLAYING;

  // Queue the buffers.
  pending_bytes_ = 0;
  for (int ix = 0; ix != num_buffers_; ++ix) {
    WAVEHDR* buffer = GetBuffer(ix);
    QueueNextPacket(buffer);  // Read more data.
    pending_bytes_ += buffer->dwBufferLength;
  }

  // From now on |pending_bytes_| would be accessed by callback thread.
  // Most likely waveOutPause() or waveOutRestart() has its own memory barrier,
  // but issuing our own is safer.
  std::atomic_thread_fence(std::memory_order_seq_cst);

  MMRESULT result = ::waveOutPause(waveout_);
  if (result != MMSYSERR_NOERROR) {
    HandleError(result);
    return;
  }

  // Send the buffers to the audio driver. Note that the device is paused
  // so we avoid entering the callback method while still here.
  for (int ix = 0; ix != num_buffers_; ++ix) {
    result = ::waveOutWrite(waveout_, GetBuffer(ix), sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
      HandleError(result);
      break;
    }
  }
  result = ::waveOutRestart(waveout_);
  if (result != MMSYSERR_NOERROR) {
    HandleError(result);
    return;
  }
}

// Stopping is tricky if we want it be fast.
// For now just do it synchronously and avoid all the complexities.
// TODO(enal): if we want faster Stop() we can create singleton that keeps track
//             of all currently playing streams. Then you don't have to wait
//             till all callbacks are completed. Of course access to singleton
//             should be under its own lock, and checking the liveness and
//             acquiring the lock on stream should be done atomically.
void PCMWaveOutAudioOutputStream::Stop() {
  if (state_ != PCMA_PLAYING)
    return;
  state_ = PCMA_STOPPING;
  std::atomic_thread_fence(std::memory_order_seq_cst);

  // Stop watching for buffer event, waits until outstanding callbacks finish.
  if (waiting_handle_) {
    if (!::UnregisterWaitEx(waiting_handle_, INVALID_HANDLE_VALUE))
      HandleError(::GetLastError());
    waiting_handle_ = NULL;
  }

  // Stop playback.
  MMRESULT res = ::waveOutReset(waveout_);
  if (res != MMSYSERR_NOERROR)
    HandleError(res);

  // Wait for lock to ensure all outstanding callbacks have completed.
  base::AutoLock auto_lock(lock_);

  // waveOutReset() leaves buffers in the unpredictable state, causing
  // problems if we want to close, release, or reuse them. Fix the states.
  for (int ix = 0; ix != num_buffers_; ++ix)
    GetBuffer(ix)->dwFlags = WHDR_PREPARED;

  // Don't use callback after Stop().
  callback_ = nullptr;

  state_ = PCMA_READY;
}

// We can Close in any state except that trying to close a stream that is
// playing Windows generates an error. We cannot propagate it to the source,
// as callback_ is set to NULL. Just print it and hope somebody somehow
// will find it...
void PCMWaveOutAudioOutputStream::Close() {
  // Force Stop() to ensure it's safe to release buffers and free the stream.
  Stop();

  if (waveout_) {
    FreeBuffers();

    // waveOutClose() generates a WIM_CLOSE callback.  In case Start() was never
    // called, force a reset to ensure close succeeds.
    MMRESULT res = ::waveOutReset(waveout_);
    DCHECK_EQ(res, static_cast<MMRESULT>(MMSYSERR_NOERROR));
    res = ::waveOutClose(waveout_);
    DCHECK_EQ(res, static_cast<MMRESULT>(MMSYSERR_NOERROR));
    state_ = PCMA_CLOSED;
    waveout_ = NULL;
  }

  // Tell the audio manager that we have been released. This can result in
  // the manager destroying us in-place so this needs to be the last thing
  // we do on this function.
  manager_->ReleaseOutputStream(this);
}

// This stream is always used with sub second buffer sizes, where it's
// sufficient to simply always flush upon Start().
void PCMWaveOutAudioOutputStream::Flush() {}

void PCMWaveOutAudioOutputStream::SetVolume(double volume) {
  if (!waveout_)
    return;
  volume_ = static_cast<float>(volume);
}

void PCMWaveOutAudioOutputStream::GetVolume(double* volume) {
  if (!waveout_)
    return;
  *volume = volume_;
}

void PCMWaveOutAudioOutputStream::HandleError(MMRESULT error) {
  DLOG(WARNING) << "PCMWaveOutAudio error " << error;
  // TODO(dalecurtis): See about sending a translated |error| code.
  if (callback_)
    callback_->OnError(AudioSourceCallback::ErrorType::kUnknown);
}

void PCMWaveOutAudioOutputStream::QueueNextPacket(WAVEHDR *buffer) {
  DCHECK_EQ(channels_, format_.Format.nChannels);
  // Call the source which will fill our buffer with pleasant sounds and
  // return to us how many bytes were used.
  // TODO(fbarchard): Handle used 0 by queueing more.

  // TODO(sergeyu): Specify correct hardware delay for |delay|.
  const base::TimeDelta delay =
      base::Microseconds(pending_bytes_ * base::Time::kMicrosecondsPerSecond /
                         format_.Format.nAvgBytesPerSec);
  int frames_filled = callback_->OnMoreData(delay, base::TimeTicks::Now(), {},
                                            audio_bus_.get());
  uint32_t used = frames_filled * audio_bus_->channels() *
                  format_.Format.wBitsPerSample / 8;

  if (used <= buffer_size_) {
    // Note: If this ever changes to output raw float the data must be clipped
    // and sanitized since it may come from an untrusted source such as NaCl.
    audio_bus_->Scale(volume_);

    DCHECK_EQ(format_.Format.wBitsPerSample, 16);
    audio_bus_->ToInterleaved<SignedInt16SampleTypeTraits>(
        frames_filled, reinterpret_cast<int16_t*>(buffer->lpData));

    buffer->dwBufferLength = used * format_.Format.nChannels / channels_;
  } else {
    HandleError(0);
    return;
  }
  buffer->dwFlags = WHDR_PREPARED;
}

// One of the threads in our thread pool asynchronously calls this function when
// buffer_event_ is signalled. Search through all the buffers looking for freed
// ones, fills them with data, and "feed" the Windows.
// Note: by searching through all the buffers we guarantee that we fill all the
//       buffers, even when "event loss" happens, i.e. if Windows signals event
//       when it did not flip into unsignaled state from the previous signal.
void NTAPI PCMWaveOutAudioOutputStream::BufferCallback(PVOID lpParameter,
                                                       BOOLEAN timer_fired) {
  TRACE_EVENT0("audio", "PCMWaveOutAudioOutputStream::BufferCallback");

  DCHECK(!timer_fired);
  PCMWaveOutAudioOutputStream* stream =
      reinterpret_cast<PCMWaveOutAudioOutputStream*>(lpParameter);

  // Lock the stream so callbacks do not interfere with each other.
  // Several callbacks can be called simultaneously by different threads in the
  // thread pool if some of the callbacks are slow, or system is very busy and
  // scheduled callbacks are not called on time.
  base::AutoLock auto_lock(stream->lock_);
  if (stream->state_ != PCMA_PLAYING)
    return;

  for (int ix = 0; ix != stream->num_buffers_; ++ix) {
    WAVEHDR* buffer = stream->GetBuffer(ix);
    if (buffer->dwFlags & WHDR_DONE) {
      // Before we queue the next packet, we need to adjust the number of
      // pending bytes since the last write to hardware.
      stream->pending_bytes_ -= buffer->dwBufferLength;
      stream->QueueNextPacket(buffer);

      // QueueNextPacket() can take a long time, especially if several of them
      // were called back-to-back. Check if we are stopping now.
      if (stream->state_ != PCMA_PLAYING)
        return;

      // Time to send the buffer to the audio driver. Since we are reusing
      // the same buffers we can get away without calling waveOutPrepareHeader.
      MMRESULT result = ::waveOutWrite(stream->waveout_,
                                       buffer,
                                       sizeof(WAVEHDR));
      if (result != MMSYSERR_NOERROR)
        stream->HandleError(result);
      stream->pending_bytes_ += buffer->dwBufferLength;
    }
  }
}

}  // namespace media
