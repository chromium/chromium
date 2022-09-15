// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_QUANTIZER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_QUANTIZER_H_

#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "services/device/public/mojom/pressure_state.mojom.h"
#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.h"

namespace content {

// Quantizes compute pressure data for a frame.
//
// See blink::mojom::PressureQuantization for a descripion of the quantization
// scheme. The scheme converts a high-entropy device::mojom::PressureState
// into a low-entropy one, which minimizes the amount of information exposed
// to a Web page that uses the Compute Pressure API.
//
// This class is not thread-safe, so each instance must be used on one sequence.
class CONTENT_EXPORT PressureQuantizer {
 public:
  // Creates a quantizer with the single sub-interval [0, 1].
  //
  // Until Assign() is called, all values will be quantized to the same value.
  PressureQuantizer();
  ~PressureQuantizer();

  PressureQuantizer(const PressureQuantizer&) = delete;
  PressureQuantizer& operator=(const PressureQuantizer&) = delete;

  // True iff `quantization` is a valid pressure quantization scheme.
  static bool IsValid(const blink::mojom::PressureQuantization& quantization);

  // True if this quantizer's scheme is the same as `quantization`.
  //
  // This is a bit more complicated than element-wise vector equality to allow
  // for floating-point precision errors, such as 0.5 != 0.2 + 0.3.
  //
  // `quantization` must be valid.
  bool IsSame(const blink::mojom::PressureQuantization& quantization) const;

  // Overwrites the quantization scheme used by this quantizer.
  void Assign(blink::mojom::PressureQuantizationPtr quantization);

  // Quantizes `sample` using the current quantization scheme.
  device::mojom::PressureState Quantize(
      device::mojom::PressureStatePtr sample) const;

 private:
  // Quantizes a single value in compute pressure data.
  class ValueQuantizer {
   public:
    ValueQuantizer();
    ~ValueQuantizer();

    ValueQuantizer(const ValueQuantizer&) = delete;
    ValueQuantizer& operator=(const ValueQuantizer&) = delete;

    static bool IsValid(const std::vector<double>& thresholds);
    bool IsSame(const std::vector<double>& thresholds) const;
    double Quantize(double value) const;
    void Assign(std::vector<double> thresholds);

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    // The array of thresholds passes the IsValid() check.
    //
    // This means that the array is sorted and does not include 0.0 or 1.0. An
    // empty array is a valid quantizing scheme that transforms every input to
    // 0.5.
    std::vector<double> thresholds_ GUARDED_BY_CONTEXT(sequence_checker_);
  };

  SEQUENCE_CHECKER(sequence_checker_);

  ValueQuantizer cpu_utilization_quantizer_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_QUANTIZER_H_
