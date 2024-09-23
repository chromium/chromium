/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/hrtf_elevation.h"

#include <math.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database.h"
#include "third_party/blink/renderer/platform/audio/hrtf_panner.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {
// Spacing, in degrees, between every azimuth loaded from resource.
constexpr unsigned kAzimuthSpacing = 15;

// Number of azimuths loaded from resource.
constexpr unsigned kNumberOfRawAzimuths = 360 / kAzimuthSpacing;

// Interpolates by this factor to get the total number of azimuths from every
// azimuth loaded from resource.
constexpr unsigned kInterpolationFactor = 8;

// Total number of azimuths after interpolation.
constexpr unsigned kNumberOfTotalAzimuths =
    kNumberOfRawAzimuths * kInterpolationFactor;

// Total number of components of an HRTF database.
constexpr size_t kTotalNumberOfResponses = 240;

// Number of frames in an individual impulse response.
constexpr size_t kResponseFrameSize = 256;

// Sample-rate of the spatialization impulse responses as stored in the resource
// file.  The impulse responses may be resampled to a different sample-rate
// (depending on the audio hardware) when they are loaded.
constexpr float kResponseSampleRate = 44100;

// This table maps the index into the elevation table with the corresponding
// angle. See https://bugs.webkit.org/show_bug.cgi?id=98294#c9 for the
// elevation angles and their order in the concatenated response.
constexpr int kElevationIndexTableSize = 10;
constexpr int kElevationIndexTable[kElevationIndexTableSize] = {
    0, 15, 30, 45, 60, 75, 90, 315, 330, 345};

// The range of elevations for the IRCAM impulse responses varies depending on
// azimuth, but the minimum elevation appears to always be -45.
//
// Here's how it goes:
constexpr int kMaxElevations[] = {
    //  Azimuth
    //
    90,  // 0
    45,  // 15
    60,  // 30
    45,  // 45
    75,  // 60
    45,  // 75
    60,  // 90
    45,  // 105
    75,  // 120
    45,  // 135
    60,  // 150
    45,  // 165
    75,  // 180
    45,  // 195
    60,  // 210
    45,  // 225
    75,  // 240
    45,  // 255
    60,  // 270
    45,  // 285
    75,  // 300
    45,  // 315
    60,  // 330
    45   // 345
};

// Lazily load a concatenated HRTF database for given subject and store it in a
// local hash table to ensure quick efficient future retrievals.
scoped_refptr<AudioBus> GetConcatenatedImpulseResponsesForSubject(
    int subject_resource_id) {
  typedef HashMap<int, scoped_refptr<AudioBus>> AudioBusMap;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AudioBusMap, audio_bus_map, ());
  DEFINE_THREAD_SAFE_STATIC_LOCAL(base::Lock, lock, ());

  base::AutoLock locker(lock);
  scoped_refptr<AudioBus> bus;
  AudioBusMap::iterator iterator = audio_bus_map.find(subject_resource_id);
  if (iterator == audio_bus_map.end()) {
    scoped_refptr<AudioBus> concatenated_impulse_responses(
        AudioBus::GetDataResource(subject_resource_id, kResponseSampleRate));
    DCHECK(concatenated_impulse_responses);

    bus = concatenated_impulse_responses;
    audio_bus_map.Set(subject_resource_id, bus);
  } else {
    bus = iterator->value;
  }

  size_t response_length = bus->length();
  size_t expected_length =
      static_cast<size_t>(kTotalNumberOfResponses * kResponseFrameSize);

  // Check number of channels and length. For now these are fixed and known.
  DCHECK_EQ(response_length, expected_length);
  DCHECK_EQ(bus->NumberOfChannels(), 2u);

  return bus;
}

}  // namespace

bool HRTFElevation::CalculateKernelsForAzimuthElevation(
    int azimuth,
    int elevation,
    float sample_rate,
    int subject_resource_id,
    std::unique_ptr<HRTFKernel>& kernel_l,
    std::unique_ptr<HRTFKernel>& kernel_r) {
  // Valid values for azimuth are 0 -> 345 in 15 degree increments.
  // Valid values for elevation are -45 -> +90 in 15 degree increments.

  DCHECK_GE(azimuth, 0);
  DCHECK_LE(azimuth, 345);
  DCHECK_EQ((azimuth / 15) * 15, azimuth);

  DCHECK_GE(elevation, -45);
  DCHECK_LE(elevation, 90);
  DCHECK_EQ((elevation / 15) * 15, elevation);

  const int positive_elevation = elevation < 0 ? elevation + 360 : elevation;

  scoped_refptr<AudioBus> bus(
      GetConcatenatedImpulseResponsesForSubject(subject_resource_id));

  if (!bus) {
    return false;
  }

  // Just sequentially search the table to find the correct index.
  int elevation_index = -1;

  for (int k = 0; k < kElevationIndexTableSize; ++k) {
    if (kElevationIndexTable[k] == positive_elevation) {
      elevation_index = k;
      break;
    }
  }

  DCHECK_GE(elevation_index, 0);
  DCHECK_LT(elevation_index, kElevationIndexTableSize);

  // The concatenated impulse response is a bus containing all
  // the elevations per azimuth, for all azimuths by increasing
  // order. So for a given azimuth and elevation we need to compute
  // the index of the wanted audio frames in the concatenated table.
  unsigned index =
      ((azimuth / kAzimuthSpacing) * HRTFDatabase::NumberOfRawElevations()) +
      elevation_index;
  DCHECK_LE(index, kTotalNumberOfResponses);

  // Extract the individual impulse response from the concatenated
  // responses and potentially sample-rate convert it to the desired
  // (hardware) sample-rate.
  unsigned start_frame = index * kResponseFrameSize;
  unsigned stop_frame = start_frame + kResponseFrameSize;
  scoped_refptr<AudioBus> pre_sample_rate_converted_response(
      AudioBus::CreateBufferFromRange(bus.get(), start_frame, stop_frame));
  scoped_refptr<AudioBus> response(AudioBus::CreateBySampleRateConverting(
      pre_sample_rate_converted_response.get(), false, sample_rate));

  // Note that depending on the fftSize returned by the panner, we may be
  // truncating the impulse response we just loaded in, or we might zero-pad it.
  const unsigned fft_size = HRTFPanner::FftSizeForSampleRate(sample_rate);

  if (2 * response->length() < fft_size) {
    // Need to resize the response buffer length so that it fis the fft size.
    // Create a new response of the right length and copy over the current
    // response.
    scoped_refptr<AudioBus> padded_response(
        AudioBus::Create(response->NumberOfChannels(), fft_size / 2));
    for (unsigned channel = 0; channel < response->NumberOfChannels();
         ++channel) {
      memcpy(padded_response->Channel(channel)->MutableData(),
             response->Channel(channel)->Data(),
             response->length() * sizeof(float));
    }
    response = padded_response;
  }
  DCHECK_GE(2 * response->length(), fft_size);

  AudioChannel* left_ear_impulse_response =
      response->Channel(AudioBus::kChannelLeft);
  AudioChannel* right_ear_impulse_response =
      response->Channel(AudioBus::kChannelRight);

  kernel_l = std::make_unique<HRTFKernel>(left_ear_impulse_response, fft_size,
                                          sample_rate);
  kernel_r = std::make_unique<HRTFKernel>(right_ear_impulse_response, fft_size,
                                          sample_rate);

  return true;
}

std::unique_ptr<HRTFElevation> HRTFElevation::CreateForSubject(
    int subject_resource_id,
    int elevation,
    float sample_rate) {
  DCHECK_GE(elevation, -45);
  DCHECK_LE(elevation, 90);
  DCHECK_EQ((elevation / 15) * 15, elevation);

  std::unique_ptr<HRTFKernelList> kernel_list_l =
      std::make_unique<HRTFKernelList>(kNumberOfTotalAzimuths);
  std::unique_ptr<HRTFKernelList> kernel_list_r =
      std::make_unique<HRTFKernelList>(kNumberOfTotalAzimuths);

  // Load convolution kernels from HRTF files.
  int interpolated_index = 0;
  for (unsigned raw_index = 0; raw_index < kNumberOfRawAzimuths; ++raw_index) {
    // Don't let elevation exceed maximum for this azimuth.
    const int max_elevation = kMaxElevations[raw_index];
    const int actual_elevation = std::min(elevation, max_elevation);

    const bool success = CalculateKernelsForAzimuthElevation(
        raw_index * kAzimuthSpacing, actual_elevation, sample_rate,
        subject_resource_id, kernel_list_l->at(interpolated_index),
        kernel_list_r->at(interpolated_index));
    if (!success) {
      return nullptr;
    }

    interpolated_index += kInterpolationFactor;
  }

  // Now go back and interpolate intermediate azimuth values.
  for (unsigned i = 0; i < kNumberOfTotalAzimuths; i += kInterpolationFactor) {
    int j = (i + kInterpolationFactor) % kNumberOfTotalAzimuths;

    // Create the interpolated convolution kernels and delays.
    for (unsigned jj = 1; jj < kInterpolationFactor; ++jj) {
      float x =
          static_cast<float>(jj) /
          static_cast<float>(kInterpolationFactor);  // interpolate from 0 -> 1

      (*kernel_list_l)[i + jj] = HRTFKernel::CreateInterpolatedKernel(
          kernel_list_l->at(i).get(), kernel_list_l->at(j).get(), x);
      (*kernel_list_r)[i + jj] = HRTFKernel::CreateInterpolatedKernel(
          kernel_list_r->at(i).get(), kernel_list_r->at(j).get(), x);
    }
  }

  std::unique_ptr<HRTFElevation> hrtf_elevation =
      base::WrapUnique(new HRTFElevation(std::move(kernel_list_l),
                                         std::move(kernel_list_r), elevation));
  return hrtf_elevation;
}

std::unique_ptr<HRTFElevation> HRTFElevation::CreateByInterpolatingSlices(
    HRTFElevation* hrtf_elevation1,
    HRTFElevation* hrtf_elevation2,
    float x) {
  DCHECK(hrtf_elevation1);
  DCHECK(hrtf_elevation2);

  DCHECK_GE(x, 0.0);
  DCHECK_LT(x, 1.0);

  std::unique_ptr<HRTFKernelList> kernel_list_l =
      std::make_unique<HRTFKernelList>(kNumberOfTotalAzimuths);
  std::unique_ptr<HRTFKernelList> kernel_list_r =
      std::make_unique<HRTFKernelList>(kNumberOfTotalAzimuths);

  HRTFKernelList* kernel_list_l1 = hrtf_elevation1->KernelListL();
  HRTFKernelList* kernel_list_r1 = hrtf_elevation1->KernelListR();
  HRTFKernelList* kernel_list_l2 = hrtf_elevation2->KernelListL();
  HRTFKernelList* kernel_list_r2 = hrtf_elevation2->KernelListR();

  // Interpolate kernels of corresponding azimuths of the two elevations.
  for (unsigned i = 0; i < kNumberOfTotalAzimuths; ++i) {
    (*kernel_list_l)[i] = HRTFKernel::CreateInterpolatedKernel(
        kernel_list_l1->at(i).get(), kernel_list_l2->at(i).get(), x);
    (*kernel_list_r)[i] = HRTFKernel::CreateInterpolatedKernel(
        kernel_list_r1->at(i).get(), kernel_list_r2->at(i).get(), x);
  }

  // Interpolate elevation angle.
  const double angle = (1.0 - x) * hrtf_elevation1->elevation_angle_ +
                       x * hrtf_elevation2->elevation_angle_;

  std::unique_ptr<HRTFElevation> hrtf_elevation = base::WrapUnique(
      new HRTFElevation(std::move(kernel_list_l), std::move(kernel_list_r),
                        static_cast<int>(angle)));
  return hrtf_elevation;
}

unsigned HRTFElevation::NumberOfAzimuths() {
  return kNumberOfTotalAzimuths;
}

void HRTFElevation::GetKernelsFromAzimuth(double azimuth_blend,
                                          unsigned azimuth_index,
                                          HRTFKernel*& kernel_l,
                                          HRTFKernel*& kernel_r,
                                          double& frame_delay_l,
                                          double& frame_delay_r) {
  DCHECK_GE(azimuth_blend, 0.0);
  DCHECK_LT(azimuth_blend, 1.0);

  const unsigned num_kernels = kernel_list_l_->size();

  DCHECK_LT(azimuth_index, num_kernels);

  // Return the left and right kernels.
  kernel_l = kernel_list_l_->at(azimuth_index).get();
  kernel_r = kernel_list_r_->at(azimuth_index).get();

  frame_delay_l = kernel_list_l_->at(azimuth_index)->FrameDelay();
  frame_delay_r = kernel_list_r_->at(azimuth_index)->FrameDelay();

  const int azimuth_index2 = (azimuth_index + 1) % num_kernels;
  const double frame_delay2l = kernel_list_l_->at(azimuth_index2)->FrameDelay();
  const double frame_delay2r = kernel_list_r_->at(azimuth_index2)->FrameDelay();

  // Linearly interpolate delays.
  frame_delay_l =
      (1.0 - azimuth_blend) * frame_delay_l + azimuth_blend * frame_delay2l;
  frame_delay_r =
      (1.0 - azimuth_blend) * frame_delay_r + azimuth_blend * frame_delay2r;
}

}  // namespace blink
