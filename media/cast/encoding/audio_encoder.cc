// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cast/encoding/audio_encoder.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_sample_types.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"
#include "third_party/opus/src/include/opus.h"

#if BUILDFLAG(IS_APPLE)
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace media {
namespace cast {

namespace {

const int kUnderrunSkipThreshold = 3;
const int kDefaultFramesPerSecond = 100;

}  // namespace

// Base class that handles the common problem of feeding one or more AudioBus'
// data into a buffer and then, once the buffer is full, encoding the signal and
// emitting a SenderEncodedFrame via the FrameEncodedCallback.
//
// Subclasses complete the implementation by handling the actual encoding
// details.
class AudioEncoder::ImplBase
    : public base::RefCountedThreadSafe<AudioEncoder::ImplBase> {
 public:
  ImplBase(const scoped_refptr<CastEnvironment>& cast_environment,
           AudioCodec codec,
           int num_channels,
           int sampling_rate,
           int samples_per_frame,
           int bitrate,
           FrameEncodedCallback callback)
      : cast_environment_(cast_environment),
        codec_(codec),
        num_channels_(num_channels),
        samples_per_frame_(samples_per_frame),
        bitrate_(bitrate),
        callback_(std::move(callback)),
        operational_status_(STATUS_UNINITIALIZED),
        frame_duration_(base::Seconds(static_cast<double>(samples_per_frame_) /
                                      sampling_rate)),
        buffer_fill_end_(0),
        frame_id_(FrameId::first()),
        samples_dropped_from_buffer_(0) {
    // Support for max sampling rate of 48KHz, 2 channels, 100 ms duration.
    const int kMaxSamplesTimesChannelsPerFrame = 48 * 2 * 100;
    if (num_channels_ <= 0 || samples_per_frame_ <= 0 ||
        frame_duration_.is_zero() ||
        samples_per_frame_ * num_channels_ > kMaxSamplesTimesChannelsPerFrame) {
      operational_status_ = STATUS_INVALID_CONFIGURATION;
    }
  }

  ImplBase(const ImplBase&) = delete;
  ImplBase& operator=(const ImplBase&) = delete;

  OperationalStatus InitializationResult() const { return operational_status_; }

  int samples_per_frame() const { return samples_per_frame_; }

  base::TimeDelta frame_duration() const { return frame_duration_; }

  // Returns the current bitrate that the audio encoder is configured to use. If
  // the encoder doesn't support getting the bitrate, returns 0.
  virtual int GetBitrate() const { return 0; }

  void EncodeAudio(std::unique_ptr<AudioBus> audio_bus,
                   const base::TimeTicks recorded_time) {
    DCHECK_EQ(operational_status_, STATUS_INITIALIZED);
    DCHECK(!recorded_time.is_null());

    // Determine whether |recorded_time| is consistent with the amount of audio
    // data having been processed in the past.  Resolve the underrun problem by
    // dropping data from the internal buffer and skipping ahead the next
    // frame's RTP timestamp by the estimated number of frames missed.  On the
    // other hand, don't attempt to resolve overruns: A receiver should
    // gracefully deal with an excess of audio data.
    base::TimeDelta buffer_fill_duration =
        buffer_fill_end_ * frame_duration_ / samples_per_frame_;
    if (!frame_capture_time_.is_null()) {
      const base::TimeDelta amount_ahead_by =
          recorded_time - (frame_capture_time_ + buffer_fill_duration);
      const int64_t num_frames_missed = amount_ahead_by.IntDiv(frame_duration_);
      if (num_frames_missed > kUnderrunSkipThreshold) {
        samples_dropped_from_buffer_ += buffer_fill_end_;
        buffer_fill_end_ = 0;
        buffer_fill_duration = base::TimeDelta();
        frame_rtp_timestamp_ +=
            RtpTimeDelta::FromTicks(num_frames_missed * samples_per_frame_);
        DVLOG(1) << "Skipping RTP timestamp ahead to account for "
                 << num_frames_missed * samples_per_frame_
                 << " samples' worth of underrun.";
        TRACE_EVENT_INSTANT2("cast.stream", "Audio Skip",
                             TRACE_EVENT_SCOPE_THREAD, "frames missed",
                             num_frames_missed, "samples dropped",
                             samples_dropped_from_buffer_);
      }
    }
    frame_capture_time_ = recorded_time - buffer_fill_duration;

    // Encode all audio in |audio_bus| into zero or more frames.
    int src_pos = 0;
    while (src_pos < audio_bus->frames()) {
      // Note: This is used to compute the encoder utilization and so it uses
      // the real-world clock instead of the CastEnvironment clock, the latter
      // of which might be simulated.
      const base::TimeTicks start_time = base::TimeTicks::Now();

      const int num_samples_to_xfer = std::min(
          samples_per_frame_ - buffer_fill_end_, audio_bus->frames() - src_pos);
      DCHECK_EQ(audio_bus->channels(), num_channels_);
      TransferSamplesIntoBuffer(audio_bus.get(), src_pos, buffer_fill_end_,
                                num_samples_to_xfer);
      src_pos += num_samples_to_xfer;
      buffer_fill_end_ += num_samples_to_xfer;

      if (buffer_fill_end_ < samples_per_frame_)
        break;

      auto audio_frame = std::make_unique<SenderEncodedFrame>();
      audio_frame->dependency =
          openscreen::cast::EncodedFrame::Dependency::kKeyFrame;
      audio_frame->frame_id = frame_id_;
      audio_frame->referenced_frame_id = frame_id_;
      audio_frame->rtp_timestamp = frame_rtp_timestamp_;
      audio_frame->reference_time = frame_capture_time_;

      // TODO(crbug.com/40280546): get accurate timestamps for both
      // capture begin and capture end.
      audio_frame->capture_begin_time = frame_capture_time_;
      audio_frame->capture_end_time = frame_capture_time_;

      TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
          "cast.stream", "Audio Encode", TRACE_ID_LOCAL(audio_frame.get()),
          "frame_id", frame_id_.lower_32_bits(), "rtp_timestamp",
          frame_rtp_timestamp_.lower_32_bits());
      if (EncodeFromFilledBuffer(&audio_frame->data)) {
        // Compute encoder utilization as the real-world time elapsed divided
        // by the signal duration.
        audio_frame->encoder_utilization =
            (base::TimeTicks::Now() - start_time) / frame_duration_;
        audio_frame->encoder_bitrate = bitrate_;
        TRACE_EVENT_NESTABLE_ASYNC_END1(
            "cast.stream", "Audio Encode", TRACE_ID_LOCAL(audio_frame.get()),
            "encoder_utilization", audio_frame->encoder_utilization);

        audio_frame->encode_completion_time =
            cast_environment_->Clock()->NowTicks();
        cast_environment_->PostTask(
            CastEnvironment::MAIN, FROM_HERE,
            base::BindOnce(callback_, std::move(audio_frame),
                           samples_dropped_from_buffer_));
        samples_dropped_from_buffer_ = 0;
      }

      // Reset the internal buffer, frame ID, and timestamps for the next frame.
      buffer_fill_end_ = 0;
      ++frame_id_;
      frame_rtp_timestamp_ += RtpTimeDelta::FromTicks(samples_per_frame_);
      frame_capture_time_ += frame_duration_;
    }
  }

 protected:
  friend class base::RefCountedThreadSafe<ImplBase>;
  virtual ~ImplBase() = default;

  virtual void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                         int source_offset,
                                         int buffer_fill_offset,
                                         int num_samples) = 0;
  virtual bool EncodeFromFilledBuffer(std::string* out) = 0;

  const scoped_refptr<CastEnvironment> cast_environment_;
  const AudioCodec codec_;
  const int num_channels_;
  const int samples_per_frame_;
  const int bitrate_;
  const FrameEncodedCallback callback_;

  // Subclass' ctor is expected to set this to STATUS_INITIALIZED.
  OperationalStatus operational_status_;

  // The duration of one frame of encoded audio samples. Derived from
  // |samples_per_frame_| and the sampling rate.
  const base::TimeDelta frame_duration_;

 private:
  // In the case where a call to EncodeAudio() cannot completely fill the
  // buffer, this points to the position at which to populate data in a later
  // call.
  int buffer_fill_end_;

  // A counter used to label EncodedFrames.
  FrameId frame_id_;

  // The RTP timestamp for the next frame of encoded audio.  This is defined as
  // the number of audio samples encoded so far, plus the estimated number of
  // samples that were missed due to data underruns.  A receiver uses this value
  // to detect gaps in the audio signal data being provided.
  RtpTimeTicks frame_rtp_timestamp_;

  // The local system time associated with the start of the next frame of
  // encoded audio.  This value is passed on to a receiver as a reference clock
  // timestamp for the purposes of synchronizing audio and video.  Its
  // progression is expected to drift relative to the elapsed time implied by
  // the RTP timestamps.
  base::TimeTicks frame_capture_time_;

  // Set to non-zero to indicate the next output frame skipped over audio
  // samples in order to recover from an input underrun.
  int samples_dropped_from_buffer_;
};

class AudioEncoder::OpusImpl final : public AudioEncoder::ImplBase {
 public:
  OpusImpl(const scoped_refptr<CastEnvironment>& cast_environment,
           int num_channels,
           int sampling_rate,
           int bitrate,
           FrameEncodedCallback callback)
      : ImplBase(cast_environment,
                 AudioCodec::kOpus,
                 num_channels,
                 sampling_rate,
                 sampling_rate / kDefaultFramesPerSecond, /* 10 ms frames */
                 bitrate,
                 std::move(callback)),
        encoder_memory_(new uint8_t[opus_encoder_get_size(num_channels)]),
        opus_encoder_(reinterpret_cast<OpusEncoder*>(encoder_memory_.get())),
        buffer_(new float[num_channels * samples_per_frame_]) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED ||
        sampling_rate % samples_per_frame_ != 0 ||
        !IsValidFrameDuration(frame_duration_)) {
      return;
    }
    if (opus_encoder_init(opus_encoder_, sampling_rate, num_channels,
                          OPUS_APPLICATION_AUDIO) != OPUS_OK) {
      ImplBase::operational_status_ = STATUS_INVALID_CONFIGURATION;
      return;
    }
    ImplBase::operational_status_ = STATUS_INITIALIZED;

    if (bitrate <= 0) {
      // Note: As of 2013-10-31, the encoder in "auto bitrate" mode would use a
      // variable bitrate up to 102kbps for 2-channel, 48 kHz audio and a 10 ms
      // frame size.  The opus library authors may, of course, adjust this in
      // later versions.
      bitrate = OPUS_AUTO;
    }
    CHECK_EQ(opus_encoder_ctl(opus_encoder_.get(), OPUS_SET_BITRATE(bitrate)),
             OPUS_OK);
  }

  OpusImpl(const OpusImpl&) = delete;
  OpusImpl& operator=(const OpusImpl&) = delete;

  int GetBitrate() const override {
    int bitrate = 0;
    CHECK_EQ(opus_encoder_ctl(opus_encoder_.get(), OPUS_GET_BITRATE(&bitrate)),
             OPUS_OK);
    return bitrate;
  }

 private:
  ~OpusImpl() final = default;

  void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                 int source_offset,
                                 int buffer_fill_offset,
                                 int num_samples) final {
    DCHECK_EQ(audio_bus->channels(), num_channels_);
    float* dest = buffer_.get() + (buffer_fill_offset * num_channels_);
    audio_bus->ToInterleavedPartial<Float32SampleTypeTraits>(source_offset,
                                                             num_samples, dest);
  }

  bool EncodeFromFilledBuffer(std::string* out) final {
    out->resize(kOpusMaxPayloadSize);
    const opus_int32 result = opus_encode_float(
        opus_encoder_, buffer_.get(), samples_per_frame_,
        reinterpret_cast<uint8_t*>(std::data(*out)), kOpusMaxPayloadSize);
    if (result > 1) {
      out->resize(result);
      return true;
    } else if (result < 0) {
      LOG(ERROR) << "Error code from opus_encode_float(): " << result;
      return false;
    } else {
      // Do nothing: The documentation says that a return value of zero or
      // one byte means the packet does not need to be transmitted.
      return false;
    }
  }

  static bool IsValidFrameDuration(base::TimeDelta duration) {
    // See https://tools.ietf.org/html/rfc6716#section-2.1.4
    return duration == base::Microseconds(2500) ||
           duration == base::Milliseconds(5) ||
           duration == base::Milliseconds(10) ||
           duration == base::Milliseconds(20) ||
           duration == base::Milliseconds(40) ||
           duration == base::Milliseconds(60);
  }

  const std::unique_ptr<uint8_t[]> encoder_memory_;
  const raw_ptr<OpusEncoder> opus_encoder_;
  const std::unique_ptr<float[]> buffer_;

  // This is the recommended value, according to documentation in
  // third_party/opus/src/include/opus.h, so that the Opus encoder does not
  // degrade the audio due to memory constraints.
  //
  // Note: Whereas other RTP implementations do not, the cast library is
  // perfectly capable of transporting larger than MTU-sized audio frames.
  static const int kOpusMaxPayloadSize = 4000;
};

#if BUILDFLAG(IS_APPLE)
class AudioEncoder::AppleAacImpl final : public AudioEncoder::ImplBase {
  // AAC-LC has two access unit sizes (960 and 1024). The Apple encoder only
  // supports the latter.
  static const int kAccessUnitSamples = 1024;

  // Size of an ADTS header (w/o checksum). See
  // http://wiki.multimedia.cx/index.php?title=ADTS
  static const int kAdtsHeaderSize = 7;

 public:
  AppleAacImpl(const scoped_refptr<CastEnvironment>& cast_environment,
               int num_channels,
               int sampling_rate,
               int bitrate,
               FrameEncodedCallback callback)
      : ImplBase(cast_environment,
                 AudioCodec::kAAC,
                 num_channels,
                 sampling_rate,
                 kAccessUnitSamples,
                 bitrate,
                 std::move(callback)),
        input_buffer_(AudioBus::Create(num_channels, kAccessUnitSamples)),
        input_bus_(AudioBus::CreateWrapper(num_channels)),
        max_access_unit_size_(0),
        output_buffer_(nullptr),
        converter_(nullptr),
        file_(nullptr),
        num_access_units_(0) {
    if (ImplBase::operational_status_ != STATUS_UNINITIALIZED) {
      return;
    }
    if (!Initialize(sampling_rate, bitrate)) {
      ImplBase::operational_status_ = STATUS_INVALID_CONFIGURATION;
      return;
    }
    ImplBase::operational_status_ = STATUS_INITIALIZED;
  }

  AppleAacImpl(const AppleAacImpl&) = delete;
  AppleAacImpl& operator=(const AppleAacImpl&) = delete;

 private:
  ~AppleAacImpl() override { Teardown(); }

  // Destroys the existing audio converter and file, if any.
  void Teardown() {
    if (converter_) {
      AudioConverterDispose(converter_);
      converter_ = nullptr;
    }
    if (file_) {
      AudioFileClose(file_);
      file_ = nullptr;
    }
  }

  // Initializes the audio converter and file. Calls Teardown to destroy any
  // existing state. This is so that Initialize() may be called to setup another
  // converter after a non-resumable interruption.
  bool Initialize(int sampling_rate, int bitrate) {
    // Teardown previous audio converter and file.
    Teardown();

    // Input data comes from AudioBus objects, which carry non-interleaved
    // packed native-endian float samples. Note that in Core Audio, a frame is
    // one sample across all channels at a given point in time. When describing
    // a non-interleaved samples format, the "per frame" fields mean "per
    // channel" or "per stream", with the exception of |mChannelsPerFrame|. For
    // uncompressed formats, one packet contains one frame.
    AudioStreamBasicDescription in_asbd;
    in_asbd.mSampleRate = sampling_rate;
    in_asbd.mFormatID = kAudioFormatLinearPCM;
    in_asbd.mFormatFlags =
        AudioFormatFlags{kAudioFormatFlagsNativeFloatPacked} |
        kAudioFormatFlagIsNonInterleaved;
    in_asbd.mChannelsPerFrame = num_channels_;
    in_asbd.mBitsPerChannel = sizeof(float) * 8;
    in_asbd.mFramesPerPacket = 1;
    in_asbd.mBytesPerPacket = in_asbd.mBytesPerFrame = sizeof(float);
    in_asbd.mReserved = 0;

    // Request AAC-LC encoding, with no downmixing or downsampling.
    AudioStreamBasicDescription out_asbd;
    memset(&out_asbd, 0, sizeof(AudioStreamBasicDescription));
    out_asbd.mSampleRate = sampling_rate;
    out_asbd.mFormatID = kAudioFormatMPEG4AAC;
    out_asbd.mChannelsPerFrame = num_channels_;
    UInt32 prop_size = sizeof(out_asbd);
    if (AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 0, nullptr,
                               &prop_size, &out_asbd) != noErr) {
      return false;
    }

    if (AudioConverterNew(&in_asbd, &out_asbd, &converter_) != noErr) {
      return false;
    }

    // The converter will fully specify the output format and update the
    // relevant fields of the structure, which we can now query.
    prop_size = sizeof(out_asbd);
    if (AudioConverterGetProperty(converter_,
                                  kAudioConverterCurrentOutputStreamDescription,
                                  &prop_size, &out_asbd) != noErr) {
      return false;
    }

    // If bitrate is <= 0, allow the encoder to pick a suitable value.
    // Otherwise, set the bitrate (which can fail if the value is not suitable
    // or compatible with the output sampling rate or channels).
    if (bitrate > 0) {
      prop_size = sizeof(int);
      if (AudioConverterSetProperty(converter_, kAudioConverterEncodeBitRate,
                                    prop_size, &bitrate) != noErr) {
        return false;
      }
    }

    // Figure out the maximum size of an access unit that the encoder can
    // produce. |mBytesPerPacket| will be 0 for variable size configurations,
    // in which case we must query the value.
    uint32_t max_access_unit_size = out_asbd.mBytesPerPacket;
    if (max_access_unit_size == 0) {
      prop_size = sizeof(max_access_unit_size);
      if (AudioConverterGetProperty(
              converter_, kAudioConverterPropertyMaximumOutputPacketSize,
              &prop_size, &max_access_unit_size) != noErr) {
        return false;
      }
    }

    // This is the only location where the implementation modifies
    // |max_access_unit_size_|.
    const_cast<uint32_t&>(max_access_unit_size_) = max_access_unit_size;

    // Allocate a buffer to store one access unit. This is the only location
    // where the implementation modifies |access_unit_buffer_|.
    const_cast<std::unique_ptr<uint8_t[]>&>(access_unit_buffer_)
        .reset(new uint8_t[max_access_unit_size]);

    // Initialize the converter ABL. Note that the buffer size has to be set
    // before every encode operation, since the field is modified to indicate
    // the size of the output data (on input it indicates the buffer capacity).
    converter_abl_.mNumberBuffers = 1;
    converter_abl_.mBuffers[0].mNumberChannels = num_channels_;
    converter_abl_.mBuffers[0].mData = access_unit_buffer_.get();

    // The "magic cookie" is an encoder state vector required for decoding and
    // packetization. It is queried now from |converter_| then set on |file_|
    // after initialization.
    UInt32 cookie_size;
    if (AudioConverterGetPropertyInfo(converter_,
                                      kAudioConverterCompressionMagicCookie,
                                      &cookie_size, nullptr) != noErr) {
      return false;
    }
    auto cookie_data = std::make_unique<uint8_t[]>(cookie_size);
    if (AudioConverterGetProperty(converter_,
                                  kAudioConverterCompressionMagicCookie,
                                  &cookie_size, cookie_data.get()) != noErr) {
      return false;
    }

    if (AudioFileInitializeWithCallbacks(
            this, &FileReadCallback, &FileWriteCallback, &FileGetSizeCallback,
            &FileSetSizeCallback, kAudioFileAAC_ADTSType, &out_asbd, 0,
            &file_) != noErr) {
      return false;
    }

    if (AudioFileSetProperty(file_, kAudioFilePropertyMagicCookieData,
                             cookie_size, cookie_data.get()) != noErr) {
      return false;
    }

    // Initially the input bus points to the input buffer. See the comment on
    // |input_bus_| for more on this optimization.
    input_bus_->set_frames(kAccessUnitSamples);
    for (int ch = 0; ch < input_buffer_->channels(); ++ch) {
      input_bus_->SetChannelData(ch, input_buffer_->channel(ch));
    }

    return true;
  }

  void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                 int source_offset,
                                 int buffer_fill_offset,
                                 int num_samples) final {
    DCHECK_EQ(audio_bus->channels(), input_buffer_->channels());

    // See the comment on |input_bus_| for more on this optimization. Note that
    // we cannot elide the copy if the source offset would result in an
    // unaligned pointer.
    if (num_samples == kAccessUnitSamples &&
        source_offset * sizeof(float) % AudioBus::kChannelAlignment == 0) {
      DCHECK_EQ(buffer_fill_offset, 0);
      for (int ch = 0; ch < audio_bus->channels(); ++ch) {
        auto* samples = const_cast<float*>(audio_bus->channel(ch));
        input_bus_->SetChannelData(ch, samples + source_offset);
      }
      return;
    }

    // Copy the samples into the input buffer.
    DCHECK_EQ(input_bus_->channel(0), input_buffer_->channel(0));
    audio_bus->CopyPartialFramesTo(source_offset, num_samples,
                                   buffer_fill_offset, input_buffer_.get());
  }

  bool EncodeFromFilledBuffer(std::string* out) final {
    // Reset the buffer size field to the buffer capacity.
    converter_abl_.mBuffers[0].mDataByteSize = max_access_unit_size_;

    // Encode the current input buffer. This is a synchronous call.
    OSStatus oserr;
    UInt32 io_num_packets = 1;
    AudioStreamPacketDescription packet_description;
    oserr = AudioConverterFillComplexBuffer(
        converter_, &ConverterFillDataCallback, this, &io_num_packets,
        &converter_abl_, &packet_description);
    if (oserr != noErr || io_num_packets == 0) {
      return false;
    }

    // Reserve space in the output buffer to write the packet.
    out->reserve(packet_description.mDataByteSize + kAdtsHeaderSize);

    // Set the current output buffer and emit an ADTS-wrapped AAC access unit.
    // This is a synchronous call. After it returns, reset the output buffer.
    output_buffer_ = out;
    oserr = AudioFileWritePackets(
        file_, false, converter_abl_.mBuffers[0].mDataByteSize,
        &packet_description, num_access_units_, &io_num_packets,
        converter_abl_.mBuffers[0].mData);
    output_buffer_ = nullptr;
    if (oserr != noErr || io_num_packets == 0) {
      return false;
    }
    num_access_units_ += io_num_packets;
    return true;
  }

  // The |AudioConverterFillComplexBuffer| input callback function. Configures
  // the provided |AudioBufferList| to alias |input_bus_|. The implementation
  // can only supply |kAccessUnitSamples| samples as a result of not copying
  // samples or tracking read and write positions. Note that this function is
  // called synchronously by |AudioConverterFillComplexBuffer|.
  static OSStatus ConverterFillDataCallback(
      AudioConverterRef in_converter,
      UInt32* io_num_packets,
      AudioBufferList* io_data,
      AudioStreamPacketDescription** out_packet_desc,
      void* in_encoder) {
    DCHECK(in_encoder);
    auto* encoder = reinterpret_cast<AppleAacImpl*>(in_encoder);
    auto* input_buffer = encoder->input_buffer_.get();
    auto* input_bus = encoder->input_bus_.get();

    DCHECK_EQ(static_cast<int>(*io_num_packets), kAccessUnitSamples);
    DCHECK_EQ(io_data->mNumberBuffers,
              static_cast<unsigned>(input_bus->channels()));
    for (int i_buf = 0, end = io_data->mNumberBuffers; i_buf < end; ++i_buf) {
      io_data->mBuffers[i_buf].mNumberChannels = 1;
      io_data->mBuffers[i_buf].mDataByteSize = sizeof(float) * *io_num_packets;
      io_data->mBuffers[i_buf].mData = input_bus->channel(i_buf);

      // Reset the input bus back to the input buffer. See the comment on
      // |input_bus_| for more on this optimization.
      input_bus->SetChannelData(i_buf, input_buffer->channel(i_buf));
    }
    return noErr;
  }

  // The AudioFile read callback function.
  static OSStatus FileReadCallback(void* in_encoder,
                                   SInt64 in_position,
                                   UInt32 in_size,
                                   void* in_buffer,
                                   UInt32* out_size) {
    // This class only does writing.
    NOTREACHED();
  }

  // The AudioFile write callback function. Appends the data to the encoder's
  // current |output_buffer_|.
  static OSStatus FileWriteCallback(void* in_encoder,
                                    SInt64 in_position,
                                    UInt32 in_size,
                                    const void* in_buffer,
                                    UInt32* out_size) {
    DCHECK(in_encoder);
    DCHECK(in_buffer);
    auto* encoder = reinterpret_cast<const AppleAacImpl*>(in_encoder);
    auto* buffer = reinterpret_cast<const std::string::value_type*>(in_buffer);

    std::string* const output_buffer = encoder->output_buffer_;
    DCHECK(output_buffer);

    output_buffer->append(buffer, in_size);
    *out_size = in_size;
    return noErr;
  }

  // The AudioFile getsize callback function.
  static SInt64 FileGetSizeCallback(void* in_encoder) {
    // This class only does writing.
    NOTREACHED();
  }

  // The AudioFile setsize callback function.
  static OSStatus FileSetSizeCallback(void* in_encoder, SInt64 in_size) {
    return noErr;
  }

  // Buffer that holds one AAC access unit worth of samples. The input callback
  // function provides samples from this buffer via |input_bus_| to the encoder.
  const std::unique_ptr<AudioBus> input_buffer_;

  // Wrapper AudioBus used by the input callback function. Normally it wraps
  // |input_buffer_|. However, as an optimization when the client submits a
  // buffer containing exactly one access unit worth of samples, the bus is
  // redirected to the client buffer temporarily. We know that the base
  // implementation will call us right after to encode the buffer and thus we
  // can eliminate the copy into |input_buffer_|.
  const std::unique_ptr<AudioBus> input_bus_;

  // A buffer that holds one AAC access unit. Initialized in |Initialize| once
  // the maximum access unit size is known.
  const std::unique_ptr<uint8_t[]> access_unit_buffer_;

  // The maximum size of an access unit that the encoder can emit.
  const uint32_t max_access_unit_size_;

  // A temporary pointer to the current output buffer. Only non-null when
  // writing an access unit. Accessed by the AudioFile write callback function.
  raw_ptr<std::string> output_buffer_;

  // The |AudioConverter| is responsible for AAC encoding. This is a Core Audio
  // object, not to be confused with |media::AudioConverter|.
  AudioConverterRef converter_;

  // The |AudioFile| is responsible for ADTS packetization.
  AudioFileID file_;

  // An |AudioBufferList| passed to the converter to store encoded samples.
  AudioBufferList converter_abl_;

  // The number of access units emitted so far by the encoder.
  uint64_t num_access_units_;
};
#endif  // BUILDFLAG(IS_APPLE)

AudioEncoder::AudioEncoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    int num_channels,
    int sampling_rate,
    int bitrate,
    AudioCodec codec,
    FrameEncodedCallback frame_encoded_callback)
    : cast_environment_(cast_environment) {
  // Note: It doesn't matter which thread constructs AudioEncoder, just so long
  // as all calls to InsertAudio() are by the same thread.
  DETACH_FROM_THREAD(insert_thread_checker_);
  switch (codec) {
    case AudioCodec::kOpus:
      impl_ = new OpusImpl(cast_environment, num_channels, sampling_rate,
                           bitrate, std::move(frame_encoded_callback));
      break;
#if BUILDFLAG(IS_APPLE)
    case AudioCodec::kAAC:
      impl_ = new AppleAacImpl(cast_environment, num_channels, sampling_rate,
                               bitrate, std::move(frame_encoded_callback));
      break;
#endif  // BUILDFLAG(IS_MAC)
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported or unspecified codec for audio encoder";
      break;
  }
}

AudioEncoder::~AudioEncoder() = default;

OperationalStatus AudioEncoder::InitializationResult() const {
  DCHECK_CALLED_ON_VALID_THREAD(insert_thread_checker_);
  if (impl_.get()) {
    return impl_->InitializationResult();
  }
  return STATUS_UNSUPPORTED_CODEC;
}

int AudioEncoder::GetSamplesPerFrame() const {
  DCHECK_CALLED_ON_VALID_THREAD(insert_thread_checker_);
  CHECK_EQ(InitializationResult(), STATUS_INITIALIZED);
  return impl_->samples_per_frame();
}

base::TimeDelta AudioEncoder::GetFrameDuration() const {
  DCHECK_CALLED_ON_VALID_THREAD(insert_thread_checker_);
  CHECK_EQ(InitializationResult(), STATUS_INITIALIZED);
  return impl_->frame_duration();
}

int AudioEncoder::GetBitrate() const {
  DCHECK_CALLED_ON_VALID_THREAD(insert_thread_checker_);
  if (InitializationResult() != STATUS_INITIALIZED) {
    return 0;
  }
  return impl_->GetBitrate();
}

void AudioEncoder::InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                               const base::TimeTicks recorded_time) {
  DCHECK_CALLED_ON_VALID_THREAD(insert_thread_checker_);
  DCHECK(audio_bus.get());
  CHECK_EQ(InitializationResult(), STATUS_INITIALIZED);
  cast_environment_->PostTask(
      CastEnvironment::AUDIO, FROM_HERE,
      base::BindOnce(&AudioEncoder::ImplBase::EncodeAudio, impl_,
                     std::move(audio_bus), recorded_time));
}

}  // namespace cast
}  // namespace media
