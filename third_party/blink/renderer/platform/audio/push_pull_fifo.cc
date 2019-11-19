// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/push_pull_fifo.h"

#include <algorithm>
#include <memory>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

// Suppress the warning log if over/underflow happens more than 100 times.
const unsigned kMaxMessagesToLog = 100;
}

const size_t PushPullFIFO::kMaxFIFOLength = 65536;

PushPullFIFO::PushPullFIFO(unsigned number_of_channels, size_t fifo_length)
    : fifo_length_(fifo_length) {
  CHECK_LE(fifo_length_, kMaxFIFOLength);
  fifo_bus_ = AudioBus::Create(number_of_channels, fifo_length_);
}

PushPullFIFO::~PushPullFIFO() {
  // Capture metrics only after the FIFO is actually pulled.
  if (pull_count_ == 0)
    return;

  // TODO(hongchan): The fast-shutdown process prevents the data below from
  // being collected correctly. Consider using "outside metric collector" that
  // survives the fast-shutdown.

  // Capture the percentage of underflow happened based on the total pull count.
  // (100 buckets of size 1) This is equivalent of
  // "Media.AudioRendererMissedDeadline" metric for WebAudio.
  DEFINE_STATIC_LOCAL(
      LinearHistogram,
      fifo_underflow_percentage_histogram,
      ("WebAudio.PushPullFIFO.UnderflowPercentage", 1, 100, 101));
  fifo_underflow_percentage_histogram.Count(
      static_cast<int32_t>(100.0 * underflow_count_ / pull_count_));

  // We only collect the underflow count because no overflow can happen in the
  // current implementation. This is similar to
  // "Media.AudioRendererAudioGlitches" metric for WebAudio, which is a simple
  // flag indicates any instance of glitches during FIFO's lifetime.
  DEFINE_STATIC_LOCAL(BooleanHistogram, fifo_underflow_glitches_histogram,
                      ("WebAudio.PushPullFIFO.UnderflowGlitches"));
  fifo_underflow_glitches_histogram.Count(underflow_count_ > 0);
}

// Push the data from |input_bus| to FIFO. The size of push is determined by
// the length of |input_bus|.
void PushPullFIFO::Push(const AudioBus* input_bus) {
  TRACE_EVENT1("webaudio", "PushPullFIFO::Push",
               "input_bus length", input_bus->length());

  MutexLocker locker(lock_);

  CHECK(input_bus);
  CHECK_EQ(input_bus->length(), audio_utilities::kRenderQuantumFrames);
  SECURITY_CHECK(input_bus->length() <= fifo_length_);
  SECURITY_CHECK(index_write_ < fifo_length_);

  const size_t input_bus_length = input_bus->length();
  const size_t remainder = fifo_length_ - index_write_;

  for (unsigned i = 0; i < fifo_bus_->NumberOfChannels(); ++i) {
    float* fifo_bus_channel = fifo_bus_->Channel(i)->MutableData();
    const float* input_bus_channel = input_bus->Channel(i)->Data();
    if (remainder >= input_bus_length) {
      // The remainder is big enough for the input data.
      memcpy(fifo_bus_channel + index_write_, input_bus_channel,
             input_bus_length * sizeof(*fifo_bus_channel));
    } else {
      // The input data overflows the remainder size. Wrap around the index.
      memcpy(fifo_bus_channel + index_write_, input_bus_channel,
             remainder * sizeof(*fifo_bus_channel));
      memcpy(fifo_bus_channel, input_bus_channel + remainder,
             (input_bus_length - remainder) * sizeof(*fifo_bus_channel));
    }
  }

  // Update the write index; wrap it around if necessary.
  index_write_ = (index_write_ + input_bus_length) % fifo_length_;

  // In case of overflow, move the |index_read_| to the updated |index_write_|
  // to avoid reading overwritten frames by the next pull.
  if (input_bus_length > fifo_length_ - frames_available_) {
    index_read_ = index_write_;
    if (++overflow_count_ < kMaxMessagesToLog) {
      LOG(WARNING) << "PushPullFIFO: overflow while pushing ("
                   << "overflowCount=" << overflow_count_
                   << ", availableFrames=" << frames_available_
                   << ", inputFrames=" << input_bus_length
                   << ", fifoLength=" << fifo_length_ << ")";
    }
  }

  // Update the number of frames available in FIFO.
  frames_available_ =
      std::min(frames_available_ + input_bus_length, fifo_length_);
  DCHECK_EQ((index_read_ + frames_available_) % fifo_length_, index_write_);
}

// Pull the data out of FIFO to |output_bus|. If remaining frame in the FIFO
// is less than the frames to pull, provides remaining frame plus the silence.
size_t PushPullFIFO::Pull(AudioBus* output_bus, size_t frames_requested) {
  TRACE_EVENT2("webaudio", "PushPullFIFO::Pull",
               "output_bus length", output_bus->length(),
               "frames_requested", frames_requested);

  MutexLocker locker(lock_);

#if defined(OS_ANDROID)
  if (!output_bus) {
    // Log when outputBus or FIFO object is invalid. (crbug.com/692423)
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] |outputBus| is invalid.";
    // Silently return to avoid crash.
    return 0;
  }

  // The following checks are in place to catch the inexplicable crash.
  // (crbug.com/692423)
  if (frames_requested > output_bus->length()) {
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] framesRequested > outputBus->length() ("
                 << frames_requested << " > " << output_bus->length() << ")";
  }
  if (frames_requested > fifo_length_) {
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] framesRequested > fifo_length_ (" << frames_requested
                 << " > " << fifo_length_ << ")";
  }
  if (index_read_ >= fifo_length_) {
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] index_read_ >= fifo_length_ (" << index_read_
                 << " >= " << fifo_length_ << ")";
  }
#endif

  CHECK(output_bus);
  SECURITY_CHECK(frames_requested <= output_bus->length());
  SECURITY_CHECK(frames_requested <= fifo_length_);
  SECURITY_CHECK(index_read_ < fifo_length_);

  const size_t remainder = fifo_length_ - index_read_;
  const size_t frames_to_fill = std::min(frames_available_, frames_requested);

  for (unsigned i = 0; i < fifo_bus_->NumberOfChannels(); ++i) {
    const float* fifo_bus_channel = fifo_bus_->Channel(i)->Data();
    float* output_bus_channel = output_bus->Channel(i)->MutableData();

    // Fill up the output bus with the available frames first.
    if (remainder >= frames_to_fill) {
      // The remainder is big enough for the frames to pull.
      memcpy(output_bus_channel, fifo_bus_channel + index_read_,
             frames_to_fill * sizeof(*fifo_bus_channel));
    } else {
      // The frames to pull is bigger than the remainder size.
      // Wrap around the index.
      memcpy(output_bus_channel, fifo_bus_channel + index_read_,
             remainder * sizeof(*fifo_bus_channel));
      memcpy(output_bus_channel + remainder, fifo_bus_channel,
             (frames_to_fill - remainder) * sizeof(*fifo_bus_channel));
    }

    // The frames available was not enough to fulfill the requested frames. Fill
    // the rest of the channel with silence.
    if (frames_requested > frames_to_fill) {
      memset(output_bus_channel + frames_to_fill, 0,
             (frames_requested - frames_to_fill) * sizeof(*output_bus_channel));
    }
  }

  // Update the read index; wrap it around if necessary.
  index_read_ = (index_read_ + frames_to_fill) % fifo_length_;

  // In case of underflow, move the |indexWrite| to the updated |indexRead|.
  if (frames_requested > frames_to_fill) {
    index_write_ = index_read_;
    if (underflow_count_++ < kMaxMessagesToLog) {
      LOG(WARNING) << "PushPullFIFO: underflow while pulling ("
                   << "underflowCount=" << underflow_count_
                   << ", availableFrames=" << frames_available_
                   << ", requestedFrames=" << frames_requested
                   << ", fifoLength=" << fifo_length_ << ")";
    }
  }

  // Update the number of frames in FIFO.
  frames_available_ -= frames_to_fill;
  DCHECK_EQ((index_read_ + frames_available_) % fifo_length_, index_write_);

  pull_count_++;

  // |frames_requested > frames_available_| means the frames in FIFO is not
  // enough to fulfill the requested frames from the audio device.
  return frames_requested > frames_available_
      ? frames_requested - frames_available_
      : 0;
}

const PushPullFIFOStateForTest PushPullFIFO::GetStateForTest() {
  MutexLocker locker(lock_);
  return {length(),     NumberOfChannels(), frames_available_, index_read_,
          index_write_, overflow_count_,    underflow_count_};
}

}  // namespace blink
