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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_HRTF_DATABASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_HRTF_DATABASE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/audio/hrtf_elevation.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class HRTFKernel;

class PLATFORM_EXPORT HRTFDatabase {
  USING_FAST_MALLOC(HRTFDatabase);

 public:
  explicit HRTFDatabase(float sample_rate);

  // getKernelsFromAzimuthElevation() returns a left and right ear kernel, and
  // an interpolated left and right frame delay for the given azimuth and
  // elevation.
  // azimuthBlend must be in the range 0 -> 1.
  // Valid values for azimuthIndex are
  //     0 -> HRTFElevation::NumberOfTotalAzimuths - 1
  //     (corresponding to angles of 0 -> 360).
  // Valid values for elevationAngle are MinElevation -> MaxElevation.
  void GetKernelsFromAzimuthElevation(double azimuth_blend,
                                      unsigned azimuth_index,
                                      double elevation_angle,
                                      HRTFKernel*& kernel_l,
                                      HRTFKernel*& kernel_r,
                                      double& frame_delay_l,
                                      double& frame_delay_r);

  // Returns the number of different azimuth angles.
  static unsigned NumberOfAzimuths() {
    return HRTFElevation::kNumberOfTotalAzimuths;
  }

  float SampleRate() const { return sample_rate_; }

  // Number of elevations loaded from resource.
  static const unsigned kNumberOfRawElevations;

 private:
  // Minimum and maximum elevation angles (inclusive) for a HRTFDatabase.
  static const int kMinElevation;
  static const int kMaxElevation;
  static const unsigned kRawElevationAngleSpacing;

  // Interpolates by this factor to get the total number of elevations from
  // every elevation loaded from resource.
  static const unsigned kInterpolationFactor;

  // Total number of elevations after interpolation.
  static const unsigned kNumberOfTotalElevations;

  // Returns the index for the correct HRTFElevation given the elevation angle.
  static unsigned IndexFromElevationAngle(double);

  Vector<std::unique_ptr<HRTFElevation>> elevations_;
  float sample_rate_;

  DISALLOW_COPY_AND_ASSIGN(HRTFDatabase);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_AUDIO_HRTF_DATABASE_H_
