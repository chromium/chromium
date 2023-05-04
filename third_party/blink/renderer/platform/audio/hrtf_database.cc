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

#include "third_party/blink/renderer/platform/audio/hrtf_database.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// Minimum and maximum elevation angles (inclusive) for a HRTFDatabase.
constexpr int kMinElevation = -45;
constexpr int kMaxElevation = 90;
constexpr unsigned kRawElevationAngleSpacing = 15;

// = 10, -45 -> +90 (each 15 degrees)
constexpr unsigned kNumberOfRawElevations =
    1 + ((kMaxElevation - kMinElevation) / kRawElevationAngleSpacing);

// Interpolates by this factor to get the total number of elevations from
// every elevation loaded from resource.
constexpr unsigned kInterpolationFactor = 1;

// Total number of elevations after interpolation.
constexpr unsigned kNumberOfTotalElevations =
    kNumberOfRawElevations * kInterpolationFactor;

// Returns the index for the correct HRTFElevation given the elevation angle.
unsigned IndexFromElevationAngle(double elevation_angle) {
  // Clamp to allowed range.
  elevation_angle =
      ClampTo<double, double>(elevation_angle, kMinElevation, kMaxElevation);

  unsigned elevation_index = static_cast<int>(
      kInterpolationFactor * (elevation_angle - kMinElevation) /
      kRawElevationAngleSpacing);
  return elevation_index;
}

}  // namespace

HRTFDatabase::HRTFDatabase(float sample_rate)
    : elevations_(kNumberOfTotalElevations) {
  unsigned elevation_index = 0;
  for (int elevation = kMinElevation; elevation <= kMaxElevation;
       elevation += kRawElevationAngleSpacing) {
    std::unique_ptr<HRTFElevation> hrtf_elevation =
        HRTFElevation::CreateForSubject(IDR_AUDIO_SPATIALIZATION_COMPOSITE,
                                        elevation, sample_rate);
    DCHECK(hrtf_elevation.get());

    elevations_[elevation_index] = std::move(hrtf_elevation);
    elevation_index += kInterpolationFactor;
  }

  // Now, go back and interpolate elevations.
  if (kInterpolationFactor > 1) {
    for (unsigned i = 0; i < kNumberOfTotalElevations;
         i += kInterpolationFactor) {
      unsigned j = (i + kInterpolationFactor);
      if (j >= kNumberOfTotalElevations) {
        j = i;  // for last elevation interpolate with itself
      }

      // Create the interpolated convolution kernels and delays.
      for (unsigned jj = 1; jj < kInterpolationFactor; ++jj) {
        float x =
            static_cast<float>(jj) / static_cast<float>(kInterpolationFactor);
        elevations_[i + jj] = HRTFElevation::CreateByInterpolatingSlices(
            elevations_[i].get(), elevations_[j].get(), x);
        DCHECK(elevations_[i + jj].get());
      }
    }
  }
}

unsigned HRTFDatabase::NumberOfAzimuths() {
  return HRTFElevation::NumberOfAzimuths();
}

unsigned HRTFDatabase::NumberOfRawElevations() {
  return kNumberOfRawElevations;
}

void HRTFDatabase::GetKernelsFromAzimuthElevation(double azimuth_blend,
                                                  unsigned azimuth_index,
                                                  double elevation_angle,
                                                  HRTFKernel*& kernel_l,
                                                  HRTFKernel*& kernel_r,
                                                  double& frame_delay_l,
                                                  double& frame_delay_r) const {
  unsigned elevation_index = IndexFromElevationAngle(elevation_angle);
  SECURITY_DCHECK(elevation_index < elevations_.size());
  SECURITY_DCHECK(elevations_.size() > 0);

  if (elevation_index > elevations_.size() - 1) {
    elevation_index = elevations_.size() - 1;
  }

  HRTFElevation* hrtf_elevation = elevations_[elevation_index].get();
  DCHECK(hrtf_elevation);

  hrtf_elevation->GetKernelsFromAzimuth(azimuth_blend, azimuth_index, kernel_l,
                                        kernel_r, frame_delay_l, frame_delay_r);
}

}  // namespace blink
