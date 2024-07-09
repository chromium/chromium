// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/audio/iir_filter.h"

#include <algorithm>
#include <complex>

#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/fdlibm/ieee754.h"

namespace blink {

// The length of the memory buffers for the IIR filter.  This MUST be a power of
// two and must be greater than the possible length of the filter coefficients.
const int kBufferLength = 32;
static_assert(kBufferLength >= IIRFilter::kMaxOrder + 1,
              "Internal IIR buffer length must be greater than maximum IIR "
              "Filter order.");

IIRFilter::IIRFilter(const AudioDoubleArray* feedforward,
                     const AudioDoubleArray* feedback)
    : buffer_index_(0), feedback_(feedback), feedforward_(feedforward) {
  // These are guaranteed to be zero-initialized.
  x_buffer_.Allocate(kBufferLength);
  y_buffer_.Allocate(kBufferLength);
}

IIRFilter::~IIRFilter() = default;

void IIRFilter::Reset() {
  x_buffer_.Zero();
  y_buffer_.Zero();
  buffer_index_ = 0;
}

static std::complex<double> EvaluatePolynomial(const double* coef,
                                               std::complex<double> z,
                                               int order) {
  // Use Horner's method to evaluate the polynomial P(z) = sum(coef[k]*z^k, k,
  // 0, order);
  std::complex<double> result = 0;

  for (int k = order; k >= 0; --k) {
    result = result * z + std::complex<double>(coef[k]);
  }

  return result;
}

void IIRFilter::Process(const float* source_p,
                        float* dest_p,
                        uint32_t frames_to_process) {
  // Compute
  //
  //   y[n] = sum(b[k] * x[n - k], k = 0, M) - sum(a[k] * y[n - k], k = 1, N)
  //
  // where b[k] are the feedforward coefficients and a[k] are the feedback
  // coefficients of the filter.

  // This is a Direct Form I implementation of an IIR Filter.  Should we
  // consider doing a different implementation such as Transposed Direct Form
  // II?
  const double* feedback = feedback_->Data();
  const double* feedforward = feedforward_->Data();

  DCHECK(feedback);
  DCHECK(feedforward);

  // Sanity check to see if the feedback coefficients have been scaled
  // appropriately. It must be EXACTLY 1!
  DCHECK_EQ(feedback[0], 1);

  int feedback_length = feedback_->size();
  int feedforward_length = feedforward_->size();
  int min_length = std::min(feedback_length, feedforward_length);

  double* x_buffer = x_buffer_.Data();
  double* y_buffer = y_buffer_.Data();

  for (size_t n = 0; n < frames_to_process; ++n) {
    // To help minimize roundoff, we compute using double's, even though the
    // filter coefficients only have single precision values.
    double yn = feedforward[0] * source_p[n];

    // Run both the feedforward and feedback terms together, when possible.
    for (int k = 1; k < min_length; ++k) {
      int m = (buffer_index_ - k) & (kBufferLength - 1);
      yn += feedforward[k] * x_buffer[m];
      yn -= feedback[k] * y_buffer[m];
    }

    // Handle any remaining feedforward or feedback terms.
    for (int k = min_length; k < feedforward_length; ++k) {
      yn +=
          feedforward[k] * x_buffer[(buffer_index_ - k) & (kBufferLength - 1)];
    }

    for (int k = min_length; k < feedback_length; ++k) {
      yn -= feedback[k] * y_buffer[(buffer_index_ - k) & (kBufferLength - 1)];
    }

    // Save the current input and output values in the memory buffers for the
    // next output.
    x_buffer_[buffer_index_] = source_p[n];
    y_buffer_[buffer_index_] = yn;

    buffer_index_ = (buffer_index_ + 1) & (kBufferLength - 1);

    dest_p[n] = yn;
  }
}

void IIRFilter::GetFrequencyResponse(int n_frequencies,
                                     const float* frequency,
                                     float* mag_response,
                                     float* phase_response) {
  // Evaluate the z-transform of the filter at the given normalized frequencies
  // from 0 to 1. (One corresponds to the Nyquist frequency.)
  //
  // The z-tranform of the filter is
  //
  // H(z) = sum(b[k]*z^(-k), k, 0, M) / sum(a[k]*z^(-k), k, 0, N);
  //
  // The desired frequency response is H(exp(j*omega)), where omega is in [0,
  // 1).
  //
  // Let P(x) = sum(c[k]*x^k, k, 0, P) be a polynomial of order P.  Then each of
  // the sums in H(z) is equivalent to evaluating a polynomial at the point
  // 1/z.

  for (int k = 0; k < n_frequencies; ++k) {
    if (frequency[k] < 0 || frequency[k] > 1) {
      // Out-of-bounds frequencies should return NaN.
      mag_response[k] = std::nanf("");
      phase_response[k] = std::nanf("");
    } else {
      // zRecip = 1/z = exp(-j*frequency)
      double omega = -kPiDouble * frequency[k];
      std::complex<double> z_recip =
          std::complex<double>(fdlibm::cos(omega), fdlibm::sin(omega));

      std::complex<double> numerator = EvaluatePolynomial(
          feedforward_->Data(), z_recip, feedforward_->size() - 1);
      std::complex<double> denominator =
          EvaluatePolynomial(feedback_->Data(), z_recip, feedback_->size() - 1);
      std::complex<double> response = numerator / denominator;
      mag_response[k] = static_cast<float>(abs(response));
      phase_response[k] =
          static_cast<float>(fdlibm::atan2(imag(response), real(response)));
    }
  }
}

double IIRFilter::TailTime(double sample_rate,
                           bool is_filter_stable,
                           unsigned render_quantum_frames) {
  // The maximum tail time.  This is somewhat arbitrary, but we're assuming that
  // no one is going to expect the IIRFilter to produce an output after this
  // much time after the inputs have stopped.
  const double kMaxTailTime = 10;

  // If the maximum amplitude of the impulse response is less than this, we
  // assume that we've reached the tail of the response.  Currently, this means
  // that the impulse is less than 1 bit of a 16-bit PCM value.
  const float kMaxTailAmplitude = 1 / 32768.0;

  // If filter is not stable, just return max tail.  Since the filter is not
  // stable, the impulse response won't converge to zero, so we don't need to
  // find the impulse response to find the actual tail time.
  if (!is_filter_stable) {
    return kMaxTailTime;
  }

  // How to compute the tail time?  We're going to filter an impulse
  // for |kMaxTailTime| seconds, in blocks of |render_quantum_frames| at
  // a time.  The maximum magnitude of this block is saved.  After all
  // of the samples have been computed, find the last block with a
  // maximum magnitude greater than |kMaxTaileAmplitude|.  That block
  // index + 1 will be the tail time.  We don't need to be
  // super-accurate in computing the tail time since we process on
  // blocks, block accuracy is good enough, and the value just needs
  // to be larger than the "real" tail time, so we don't prematurely
  // zero out the output of the node.

  // Number of render quanta needed to reach the max tail time.
  int number_of_blocks =
      std::ceil(sample_rate * kMaxTailTime / render_quantum_frames);

  // Input and output buffers for filtering.
  AudioFloatArray input(render_quantum_frames);
  AudioFloatArray output(render_quantum_frames);

  // Array to hold the max magnitudes
  AudioFloatArray magnitudes(number_of_blocks);

  // Create the impulse input signal.
  input[0] = 1;

  // Process the first block and get the max magnitude of the output.
  Process(input.Data(), output.Data(), render_quantum_frames);
  vector_math::Vmaxmgv(output.Data(), 1, &magnitudes[0], render_quantum_frames);

  // Process the rest of the signal, getting the max magnitude of the
  // output for each block.
  input[0] = 0;

  for (int k = 1; k < number_of_blocks; ++k) {
    Process(input.Data(), output.Data(), render_quantum_frames);
    vector_math::Vmaxmgv(output.Data(), 1, &magnitudes[k],
                         render_quantum_frames);
  }

  // Done computing the impulse response; reset the state so the actual node
  // starts in the expected initial state.
  Reset();

  // Find the last block with amplitude greater than the threshold.
  int index = number_of_blocks - 1;
  for (int k = index; k >= 0; --k) {
    if (magnitudes[k] > kMaxTailAmplitude) {
      index = k;
      break;
    }
  }

  // The magnitude first become lower than the threshold at the next block.
  // Compute the corresponding time value value; that's the tail time.
  return (index + 1) * render_quantum_frames / sample_rate;
}

}  // namespace blink
