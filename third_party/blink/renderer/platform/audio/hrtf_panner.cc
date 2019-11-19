/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/hrtf_panner.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// The value of 2 milliseconds is larger than the largest delay which exists in
// any HRTFKernel from the default HRTFDatabase (0.0136 seconds).
// We ASSERT the delay values used in process() with this value.
const double kMaxDelayTimeSeconds = 0.002;

const int kUninitializedAzimuth = -1;

HRTFPanner::HRTFPanner(float sample_rate, HRTFDatabaseLoader* database_loader)
    : Panner(kPanningModelHRTF),
      database_loader_(database_loader),
      sample_rate_(sample_rate),
      crossfade_selection_(kCrossfadeSelection1),
      azimuth_index1_(kUninitializedAzimuth),
      elevation1_(0),
      azimuth_index2_(kUninitializedAzimuth),
      elevation2_(0),
      crossfade_x_(0),
      crossfade_incr_(0),
      convolver_l1_(FftSizeForSampleRate(sample_rate)),
      convolver_r1_(FftSizeForSampleRate(sample_rate)),
      convolver_l2_(FftSizeForSampleRate(sample_rate)),
      convolver_r2_(FftSizeForSampleRate(sample_rate)),
      delay_line_l_(kMaxDelayTimeSeconds, sample_rate),
      delay_line_r_(kMaxDelayTimeSeconds, sample_rate),
      temp_l1_(audio_utilities::kRenderQuantumFrames),
      temp_r1_(audio_utilities::kRenderQuantumFrames),
      temp_l2_(audio_utilities::kRenderQuantumFrames),
      temp_r2_(audio_utilities::kRenderQuantumFrames) {
  DCHECK(database_loader);
}

HRTFPanner::~HRTFPanner() = default;

size_t HRTFPanner::FftSizeForSampleRate(float sample_rate) {
  // The HRTF impulse responses (loaded as audio resources) are 512
  // sample-frames @44.1KHz.  Currently, we truncate the impulse responses to
  // half this size, but an FFT-size of twice impulse response size is needed
  // (for convolution).  So for sample rates around 44.1KHz an FFT size of 512
  // is good.  For different sample rates, the truncated response is resampled.
  // The resampled length is used to compute the FFT size by choosing a power
  // of two that is greater than or equal the resampled length. This power of
  // two is doubled to get the actual FFT size.

  DCHECK(audio_utilities::IsValidAudioBufferSampleRate(sample_rate));

  int truncated_impulse_length = 256;
  double sample_rate_ratio = sample_rate / 44100;
  double resampled_length = truncated_impulse_length * sample_rate_ratio;

  // This is the size used for analysis frames in the HRTF kernel.  The
  // convolvers used by the kernel are twice this size.
  int analysis_fft_size = 1 << static_cast<unsigned>(log2(resampled_length));

  // Don't let the analysis size be smaller than the supported size
  analysis_fft_size = std::max(analysis_fft_size, FFTFrame::MinFFTSize());

  int convolver_fft_size = 2 * analysis_fft_size;

  // Make sure this size of convolver is supported.
  DCHECK_LE(convolver_fft_size, FFTFrame::MaxFFTSize());

  return convolver_fft_size;
}

void HRTFPanner::Reset() {
  convolver_l1_.Reset();
  convolver_r1_.Reset();
  convolver_l2_.Reset();
  convolver_r2_.Reset();
  delay_line_l_.Reset();
  delay_line_r_.Reset();
}

int HRTFPanner::CalculateDesiredAzimuthIndexAndBlend(double azimuth,
                                                     double& azimuth_blend) {
  // Convert the azimuth angle from the range -180 -> +180 into the range 0 ->
  // 360.  The azimuth index may then be calculated from this positive value.
  if (azimuth < 0)
    azimuth += 360.0;

  int number_of_azimuths = HRTFDatabase::NumberOfAzimuths();
  const double angle_between_azimuths = 360.0 / number_of_azimuths;

  // Calculate the azimuth index and the blend (0 -> 1) for interpolation.
  double desired_azimuth_index_float = azimuth / angle_between_azimuths;
  int desired_azimuth_index = static_cast<int>(desired_azimuth_index_float);
  azimuth_blend =
      desired_azimuth_index_float - static_cast<double>(desired_azimuth_index);

  // We don't immediately start using this azimuth index, but instead approach
  // this index from the last index we rendered at.  This minimizes the clicks
  // and graininess for moving sources which occur otherwise.
  desired_azimuth_index =
      clampTo(desired_azimuth_index, 0, number_of_azimuths - 1);
  return desired_azimuth_index;
}

void HRTFPanner::Pan(double desired_azimuth,
                     double elevation,
                     const AudioBus* input_bus,
                     AudioBus* output_bus,
                     uint32_t frames_to_process,
                     AudioBus::ChannelInterpretation channel_interpretation) {
  unsigned num_input_channels = input_bus ? input_bus->NumberOfChannels() : 0;

  DCHECK(input_bus);
  DCHECK_GE(num_input_channels, 1u);
  DCHECK_LE(num_input_channels, 2u);

  DCHECK(output_bus);
  DCHECK_EQ(output_bus->NumberOfChannels(), 2u);
  DCHECK_LE(frames_to_process, output_bus->length());

  HRTFDatabase* database = database_loader_->Database();
  if (!database) {
    output_bus->CopyFrom(*input_bus, channel_interpretation);
    return;
  }

  // IRCAM HRTF azimuths values from the loaded database is reversed from the
  // panner's notion of azimuth.
  double azimuth = -desired_azimuth;

  DCHECK_GE(azimuth, -180.0);
  DCHECK_LE(azimuth, 180.0);

  // Normally, we'll just be dealing with mono sources.
  // If we have a stereo input, implement stereo panning with left source
  // processed by left HRTF, and right source by right HRTF.
  const AudioChannel* input_channel_l =
      input_bus->ChannelByType(AudioBus::kChannelLeft);
  const AudioChannel* input_channel_r =
      num_input_channels > 1 ? input_bus->ChannelByType(AudioBus::kChannelRight)
                             : nullptr;

  // Get source and destination pointers.
  const float* source_l = input_channel_l->Data();
  const float* source_r =
      num_input_channels > 1 ? input_channel_r->Data() : source_l;
  float* destination_l =
      output_bus->ChannelByType(AudioBus::kChannelLeft)->MutableData();
  float* destination_r =
      output_bus->ChannelByType(AudioBus::kChannelRight)->MutableData();

  double azimuth_blend;
  int desired_azimuth_index =
      CalculateDesiredAzimuthIndexAndBlend(azimuth, azimuth_blend);

  // Initially snap azimuth and elevation values to first values encountered.
  if (azimuth_index1_ == kUninitializedAzimuth) {
    azimuth_index1_ = desired_azimuth_index;
    elevation1_ = elevation;
  }
  if (azimuth_index2_ == kUninitializedAzimuth) {
    azimuth_index2_ = desired_azimuth_index;
    elevation2_ = elevation;
  }

  // Cross-fade / transition over a period of around 45 milliseconds.
  // This is an empirical value tuned to be a reasonable trade-off between
  // smoothness and speed.
  const double fade_frames = SampleRate() <= 48000 ? 2048 : 4096;

  // Check for azimuth and elevation changes, initiating a cross-fade if needed.
  if (!crossfade_x_ && crossfade_selection_ == kCrossfadeSelection1) {
    if (desired_azimuth_index != azimuth_index1_ || elevation != elevation1_) {
      // Cross-fade from 1 -> 2
      crossfade_incr_ = 1 / fade_frames;
      azimuth_index2_ = desired_azimuth_index;
      elevation2_ = elevation;
    }
  }
  if (crossfade_x_ == 1 && crossfade_selection_ == kCrossfadeSelection2) {
    if (desired_azimuth_index != azimuth_index2_ || elevation != elevation2_) {
      // Cross-fade from 2 -> 1
      crossfade_incr_ = -1 / fade_frames;
      azimuth_index1_ = desired_azimuth_index;
      elevation1_ = elevation;
    }
  }

  // This algorithm currently requires that we process in power-of-two size
  // chunks at least audio_utilities::kRenderQuantumFrames.
  DCHECK_EQ(1UL << static_cast<int>(log2(frames_to_process)),
            frames_to_process);
  DCHECK_GE(frames_to_process, audio_utilities::kRenderQuantumFrames);

  const unsigned kFramesPerSegment = audio_utilities::kRenderQuantumFrames;
  const unsigned number_of_segments = frames_to_process / kFramesPerSegment;

  for (unsigned segment = 0; segment < number_of_segments; ++segment) {
    // Get the HRTFKernels and interpolated delays.
    HRTFKernel* kernel_l1;
    HRTFKernel* kernel_r1;
    HRTFKernel* kernel_l2;
    HRTFKernel* kernel_r2;
    double frame_delay_l1;
    double frame_delay_r1;
    double frame_delay_l2;
    double frame_delay_r2;
    database->GetKernelsFromAzimuthElevation(azimuth_blend, azimuth_index1_,
                                             elevation1_, kernel_l1, kernel_r1,
                                             frame_delay_l1, frame_delay_r1);
    database->GetKernelsFromAzimuthElevation(azimuth_blend, azimuth_index2_,
                                             elevation2_, kernel_l2, kernel_r2,
                                             frame_delay_l2, frame_delay_r2);

    DCHECK(kernel_l1);
    DCHECK(kernel_r1);
    DCHECK(kernel_l2);
    DCHECK(kernel_r2);
    DCHECK_LT(frame_delay_l1 / SampleRate(), kMaxDelayTimeSeconds);
    DCHECK_LT(frame_delay_r1 / SampleRate(), kMaxDelayTimeSeconds);
    DCHECK_LT(frame_delay_l2 / SampleRate(), kMaxDelayTimeSeconds);
    DCHECK_LT(frame_delay_r2 / SampleRate(), kMaxDelayTimeSeconds);

    // Crossfade inter-aural delays based on transitions.
    double frame_delay_l =
        (1 - crossfade_x_) * frame_delay_l1 + crossfade_x_ * frame_delay_l2;
    double frame_delay_r =
        (1 - crossfade_x_) * frame_delay_r1 + crossfade_x_ * frame_delay_r2;

    // Calculate the source and destination pointers for the current segment.
    unsigned offset = segment * kFramesPerSegment;
    const float* segment_source_l = source_l + offset;
    const float* segment_source_r = source_r + offset;
    float* segment_destination_l = destination_l + offset;
    float* segment_destination_r = destination_r + offset;

    // First run through delay lines for inter-aural time difference.
    delay_line_l_.SetDelayFrames(frame_delay_l);
    delay_line_r_.SetDelayFrames(frame_delay_r);
    delay_line_l_.Process(segment_source_l, segment_destination_l,
                          kFramesPerSegment);
    delay_line_r_.Process(segment_source_r, segment_destination_r,
                          kFramesPerSegment);

    bool needs_crossfading = crossfade_incr_;

    // Have the convolvers render directly to the final destination if we're not
    // cross-fading.
    float* convolution_destination_l1 =
        needs_crossfading ? temp_l1_.Data() : segment_destination_l;
    float* convolution_destination_r1 =
        needs_crossfading ? temp_r1_.Data() : segment_destination_r;
    float* convolution_destination_l2 =
        needs_crossfading ? temp_l2_.Data() : segment_destination_l;
    float* convolution_destination_r2 =
        needs_crossfading ? temp_r2_.Data() : segment_destination_r;

    // Now do the convolutions.
    // Note that we avoid doing convolutions on both sets of convolvers if we're
    // not currently cross-fading.

    if (crossfade_selection_ == kCrossfadeSelection1 || needs_crossfading) {
      convolver_l1_.Process(kernel_l1->FftFrame(), segment_destination_l,
                            convolution_destination_l1, kFramesPerSegment);
      convolver_r1_.Process(kernel_r1->FftFrame(), segment_destination_r,
                            convolution_destination_r1, kFramesPerSegment);
    }

    if (crossfade_selection_ == kCrossfadeSelection2 || needs_crossfading) {
      convolver_l2_.Process(kernel_l2->FftFrame(), segment_destination_l,
                            convolution_destination_l2, kFramesPerSegment);
      convolver_r2_.Process(kernel_r2->FftFrame(), segment_destination_r,
                            convolution_destination_r2, kFramesPerSegment);
    }

    if (needs_crossfading) {
      // Apply linear cross-fade.
      float x = crossfade_x_;
      float incr = crossfade_incr_;
      for (unsigned i = 0; i < kFramesPerSegment; ++i) {
        segment_destination_l[i] = (1 - x) * convolution_destination_l1[i] +
                                   x * convolution_destination_l2[i];
        segment_destination_r[i] = (1 - x) * convolution_destination_r1[i] +
                                   x * convolution_destination_r2[i];
        x += incr;
      }
      // Update cross-fade value from local.
      crossfade_x_ = x;

      if (crossfade_incr_ > 0 && fabs(crossfade_x_ - 1) < crossfade_incr_) {
        // We've fully made the crossfade transition from 1 -> 2.
        crossfade_selection_ = kCrossfadeSelection2;
        crossfade_x_ = 1;
        crossfade_incr_ = 0;
      } else if (crossfade_incr_ < 0 && fabs(crossfade_x_) < -crossfade_incr_) {
        // We've fully made the crossfade transition from 2 -> 1.
        crossfade_selection_ = kCrossfadeSelection1;
        crossfade_x_ = 0;
        crossfade_incr_ = 0;
      }
    }
  }
}

void HRTFPanner::PanWithSampleAccurateValues(
    double* desired_azimuth,
    double* elevation,
    const AudioBus* input_bus,
    AudioBus* output_bus,
    uint32_t frames_to_process,
    AudioBus::ChannelInterpretation channel_interpretation) {
  // Sample-accurate (a-rate) HRTF panner is not implemented, just k-rate.  Just
  // grab the current azimuth/elevation and use that.
  //
  // We are assuming that the inherent smoothing in the HRTF processing is good
  // enough, and we don't want to increase the complexity of the HRTF panner by
  // 15-20 times.  (We need to compute one output sample for each possibly
  // different impulse response.  That N^2.  Previously, we used an FFT to do
  // them all at once for a complexity of N/log2(N).  Hence, N/log2(N) times
  // more complex.)
  Pan(desired_azimuth[0], elevation[0], input_bus, output_bus,
      frames_to_process, channel_interpretation);
}

bool HRTFPanner::RequiresTailProcessing() const {
  // Always return true since the tail and latency are never zero.
  return true;
}

double HRTFPanner::TailTime() const {
  // Because HRTFPanner is implemented with a DelayKernel and a FFTConvolver,
  // the tailTime of the HRTFPanner is the sum of the tailTime of the
  // DelayKernel and the tailTime of the FFTConvolver, which is
  // MaxDelayTimeSeconds and fftSize() / 2, respectively.
  return kMaxDelayTimeSeconds +
         (FftSize() / 2) / static_cast<double>(SampleRate());
}

double HRTFPanner::LatencyTime() const {
  // The latency of a FFTConvolver is also fftSize() / 2, and is in addition to
  // its tailTime of the same value.
  return (FftSize() / 2) / static_cast<double>(SampleRate());
}

}  // namespace blink
