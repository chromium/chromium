// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <numbers>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/sinc_resampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

static const double kSampleRateRatio = 192000.0 / 44100.0;

// Helper class to ensure ChunkedResample() functions properly.
class MockSource {
 public:
  MOCK_METHOD2(ProvideInput, void(int frames, float* destination));
};

ACTION(ClearBuffer) {
  memset(arg1, 0, arg0 * sizeof(float));
}

ACTION(FillBuffer) {
  // Value chosen arbitrarily such that SincResampler resamples it to something
  // easily representable on all platforms; e.g., using kSampleRateRatio this
  // becomes 1.81219.
  memset(arg1, 64, arg0 * sizeof(float));
}

// Test requesting multiples of ChunkSize() frames results in the proper number
// of callbacks.
TEST(SincResamplerTest, ChunkedResample) {
  MockSource mock_source;

  // Choose a high ratio of input to output samples which will result in quick
  // exhaustion of SincResampler's internal buffers.
  SincResampler resampler(kSampleRateRatio, SincResampler::kDefaultRequestSize,
                          base::BindRepeating(&MockSource::ProvideInput,
                                              base::Unretained(&mock_source)));

  static const int kChunks = 2;
  int max_chunk_size = resampler.ChunkSize() * kChunks;
  auto resampled_destination = base::HeapArray<float>::Uninit(max_chunk_size);

  // Verify requesting ChunkSize() frames causes a single callback.
  EXPECT_CALL(mock_source, ProvideInput(_, _)).Times(1).WillOnce(ClearBuffer());
  resampler.Resample(resampler.ChunkSize(), resampled_destination.data());

  // Verify requesting kChunks * ChunkSize() frames causes kChunks callbacks.
  testing::Mock::VerifyAndClear(&mock_source);
  EXPECT_CALL(mock_source, ProvideInput(_, _))
      .Times(kChunks)
      .WillRepeatedly(ClearBuffer());
  resampler.Resample(max_chunk_size, resampled_destination.data());
}

// Verify priming the resampler avoids changes to ChunkSize() between calls.
TEST(SincResamplerTest, PrimedResample) {
  MockSource mock_source;

  // Choose a high ratio of input to output samples which will result in quick
  // exhaustion of SincResampler's internal buffers.
  SincResampler resampler(kSampleRateRatio, SincResampler::kDefaultRequestSize,
                          base::BindRepeating(&MockSource::ProvideInput,
                                              base::Unretained(&mock_source)));

  // Verify the priming adjusts the chunk size within reasonable limits.
  const int first_chunk_size = resampler.ChunkSize();
  resampler.PrimeWithSilence();
  const int max_chunk_size = resampler.ChunkSize();

  EXPECT_NE(first_chunk_size, max_chunk_size);
  EXPECT_LE(
      max_chunk_size,
      static_cast<int>(first_chunk_size + std::ceil(resampler.KernelSize() /
                                                    (2 * kSampleRateRatio))));

  // Verify Flush() resets to an unprimed state.
  resampler.Flush();
  EXPECT_EQ(first_chunk_size, resampler.ChunkSize());
  resampler.PrimeWithSilence();
  EXPECT_EQ(max_chunk_size, resampler.ChunkSize());

  const int kChunks = 2;
  const int kMaxFrames = max_chunk_size * kChunks;
  auto resampled_destination = base::HeapArray<float>::Uninit(kMaxFrames);

  // Verify requesting ChunkSize() frames causes a single callback.
  EXPECT_CALL(mock_source, ProvideInput(_, _)).Times(1).WillOnce(ClearBuffer());
  resampler.Resample(max_chunk_size, resampled_destination.data());
  EXPECT_EQ(max_chunk_size, resampler.ChunkSize());

  // Verify requesting kChunks * ChunkSize() frames causes kChunks callbacks.
  testing::Mock::VerifyAndClear(&mock_source);
  EXPECT_CALL(mock_source, ProvideInput(_, _))
      .Times(kChunks)
      .WillRepeatedly(ClearBuffer());
  resampler.Resample(kMaxFrames, resampled_destination.data());
  EXPECT_EQ(max_chunk_size, resampler.ChunkSize());
}

// Test flush resets the internal state properly.
TEST(SincResamplerTest, Flush) {
  MockSource mock_source;
  SincResampler resampler(kSampleRateRatio, SincResampler::kDefaultRequestSize,
                          base::BindRepeating(&MockSource::ProvideInput,
                                              base::Unretained(&mock_source)));
  auto resampled_destination =
      base::HeapArray<float>::Uninit(resampler.ChunkSize());

  // Fill the resampler with junk data.
  EXPECT_CALL(mock_source, ProvideInput(_, _)).Times(1).WillOnce(FillBuffer());
  resampler.Resample(resampler.ChunkSize() / 2, resampled_destination.data());
  ASSERT_NE(resampled_destination[0], 0);

  // Flush and request more data, which should all be zeros now.
  resampler.Flush();
  testing::Mock::VerifyAndClear(&mock_source);
  EXPECT_CALL(mock_source, ProvideInput(_, _)).Times(1).WillOnce(ClearBuffer());
  resampler.Resample(resampler.ChunkSize() / 2, resampled_destination.data());
  for (int i = 0; i < resampler.ChunkSize() / 2; ++i) {
    ASSERT_FLOAT_EQ(resampled_destination[i], 0);
  }
}

// This test is designed to be executed manually.
TEST(SincResamplerTest, DISABLED_SetRatioBench) {
  MockSource mock_source;
  SincResampler resampler(kSampleRateRatio, SincResampler::kDefaultRequestSize,
                          base::BindRepeating(&MockSource::ProvideInput,
                                              base::Unretained(&mock_source)));

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 1; i < 10000; ++i) {
    resampler.SetRatio(1.0 / i);
  }
  double total_time_c_ms = (base::TimeTicks::Now() - start).InMillisecondsF();
  printf("SetRatio() took %.2fms.\n", total_time_c_ms);
}

// Ensure various optimized Convolve() methods return the same value.  Only run
// this test if other optimized methods exist, otherwise the default Convolve()
// will be tested by the parameterized SincResampler tests below.
static const double kKernelInterpolationFactor = 0.5;

TEST(SincResamplerTest, Convolve) {
  // Initialize a dummy resampler.
  MockSource mock_source;
  SincResampler resampler(kSampleRateRatio, SincResampler::kDefaultRequestSize,
                          base::BindRepeating(&MockSource::ProvideInput,
                                              base::Unretained(&mock_source)));

  // The optimized Convolve methods are slightly more precise than Convolve_C(),
  // so comparison must be done using an epsilon.
  static const double kEpsilon = 0.00000005;

  // Use a kernel from SincResampler as input and kernel data, this has the
  // benefit of already being properly sized and aligned for Convolve_SSE().
  double result = resampler.Convolve_C(
      resampler.KernelSize(), resampler.kernel_storage_.get(),
      resampler.kernel_storage_.get(), resampler.kernel_storage_.get(),
      kKernelInterpolationFactor);
  double result2 = resampler.convolve_proc_(
      resampler.KernelSize(), resampler.kernel_storage_.get(),
      resampler.kernel_storage_.get(), resampler.kernel_storage_.get(),
      kKernelInterpolationFactor);
  EXPECT_NEAR(result2, result, kEpsilon);

  // Test Convolve() w/ unaligned input pointer.
  result = resampler.Convolve_C(
      resampler.KernelSize(), resampler.kernel_storage_.get() + 1,
      resampler.kernel_storage_.get(), resampler.kernel_storage_.get(),
      kKernelInterpolationFactor);
  result2 = resampler.convolve_proc_(
      resampler.KernelSize(), resampler.kernel_storage_.get() + 1,
      resampler.kernel_storage_.get(), resampler.kernel_storage_.get(),
      kKernelInterpolationFactor);
  EXPECT_NEAR(result2, result, kEpsilon);
}

// Fake audio source for testing the resampler.  Generates a sinusoidal linear
// chirp (http://en.wikipedia.org/wiki/Chirp) which can be tuned to stress the
// resampler for the specific sample rate conversion being used.
class SinusoidalLinearChirpSource {
 public:
  SinusoidalLinearChirpSource(int sample_rate,
                              int samples,
                              double max_frequency)
      : sample_rate_(sample_rate),
        total_samples_(samples),
        max_frequency_(max_frequency),
        current_index_(0) {
    // Chirp rate.
    double duration = static_cast<double>(total_samples_) / sample_rate_;
    k_ = (max_frequency_ - kMinFrequency) / duration;
  }

  SinusoidalLinearChirpSource(const SinusoidalLinearChirpSource&) = delete;
  SinusoidalLinearChirpSource& operator=(const SinusoidalLinearChirpSource&) =
      delete;

  virtual ~SinusoidalLinearChirpSource() = default;

  void ProvideInput(int frames, float* destination) {
    for (int i = 0; i < frames; ++i, ++current_index_) {
      // Filter out frequencies higher than Nyquist.
      if (Frequency(current_index_) > 0.5 * sample_rate_) {
        destination[i] = 0;
      } else {
        // Calculate time in seconds.
        double t = static_cast<double>(current_index_) / sample_rate_;

        // Sinusoidal linear chirp.
        destination[i] =
            sin(2 * std::numbers::pi * (kMinFrequency * t + (k_ / 2) * t * t));
      }
    }
  }

  double Frequency(int position) {
    return kMinFrequency +
           position * (max_frequency_ - kMinFrequency) / total_samples_;
  }

 private:
  static constexpr int kMinFrequency = 5;

  double sample_rate_;
  int total_samples_;
  double max_frequency_;
  double k_;
  int current_index_;
};

typedef std::tuple<int, int, double, double, double> SincResamplerTestData;
class SincResamplerTest : public testing::TestWithParam<SincResamplerTestData> {
 public:
  SincResamplerTest()
      : input_rate_(std::get<0>(GetParam())),
        output_rate_(std::get<1>(GetParam())),
        rms_error_(std::get<2>(GetParam())),
        low_freq_error_(std::get<3>(GetParam())),
        high_freq_error_(std::get<4>(GetParam())) {}

  virtual ~SincResamplerTest() = default;

 protected:
  int input_rate_;
  int output_rate_;
  double rms_error_;
  double low_freq_error_;
  double high_freq_error_;
};

// Tests resampling using a given input and output sample rate.
TEST_P(SincResamplerTest, Resample) {
  // Make comparisons using one second of data.
  static const double kTestDurationSecs = 1;
  int input_samples = kTestDurationSecs * input_rate_;
  int output_samples = kTestDurationSecs * output_rate_;

  // Nyquist frequency for the input sampling rate.
  double input_nyquist_freq = 0.5 * input_rate_;

  // Source for data to be resampled.
  SinusoidalLinearChirpSource resampler_source(input_rate_, input_samples,
                                               input_nyquist_freq);

  const double io_ratio = input_rate_ / static_cast<double>(output_rate_);
  SincResampler resampler(
      io_ratio, SincResampler::kDefaultRequestSize,
      base::BindRepeating(&SinusoidalLinearChirpSource::ProvideInput,
                          base::Unretained(&resampler_source)));

  const int kernel_storage_size = resampler.kernel_storage_size_for_testing();
  const int kernel_storage_size_in_bytes = kernel_storage_size * sizeof(float);

  // Force an update to the sample rate ratio to ensure dynamic sample rate
  // changes are working correctly.
  auto kernel = base::HeapArray<float>::Uninit(kernel_storage_size);
  memcpy(kernel.data(), resampler.get_kernel_for_testing(),
         kernel_storage_size_in_bytes);
  resampler.SetRatio(std::numbers::pi);
  ASSERT_NE(0, memcmp(kernel.data(), resampler.get_kernel_for_testing(),
                      kernel_storage_size_in_bytes));
  resampler.SetRatio(io_ratio);
  ASSERT_EQ(0, memcmp(kernel.data(), resampler.get_kernel_for_testing(),
                      kernel_storage_size_in_bytes));

  // TODO(dalecurtis): If we switch to AVX/SSE optimization, we'll need to
  // allocate these on 32-byte boundaries and ensure they're sized % 32 bytes.
  auto resampled_destination = base::HeapArray<float>::Uninit(output_samples);
  auto pure_destination = base::HeapArray<float>::Uninit(output_samples);

  // Generate resampled signal.
  resampler.Resample(output_samples, resampled_destination.data());

  // Generate pure signal.
  SinusoidalLinearChirpSource pure_source(output_rate_, output_samples,
                                          input_nyquist_freq);
  pure_source.ProvideInput(output_samples, pure_destination.data());

  // Range of the Nyquist frequency (0.5 * min(input rate, output_rate)) which
  // we refer to as low and high.
  static const double kLowFrequencyNyquistRange = 0.7;
  static const double kHighFrequencyNyquistRange = 0.9;

  // Calculate Root-Mean-Square-Error and maximum error for the resampling.
  double sum_of_squares = 0;
  double low_freq_max_error = 0;
  double high_freq_max_error = 0;
  int minimum_rate = std::min(input_rate_, output_rate_);
  double low_frequency_range = kLowFrequencyNyquistRange * 0.5 * minimum_rate;
  double high_frequency_range = kHighFrequencyNyquistRange * 0.5 * minimum_rate;
  for (int i = 0; i < output_samples; ++i) {
    double error = fabs(resampled_destination[i] - pure_destination[i]);

    if (pure_source.Frequency(i) < low_frequency_range) {
      if (error > low_freq_max_error) {
        low_freq_max_error = error;
      }
    } else if (pure_source.Frequency(i) < high_frequency_range) {
      if (error > high_freq_max_error) {
        high_freq_max_error = error;
      }
    }
    // TODO(dalecurtis): Sanity check frequencies > kHighFrequencyNyquistRange.

    sum_of_squares += error * error;
  }

  double rms_error = sqrt(sum_of_squares / output_samples);

// Convert each error to dbFS.
#define DBFS(x) 20 * log10(x)
  rms_error = DBFS(rms_error);
  low_freq_max_error = DBFS(low_freq_max_error);
  high_freq_max_error = DBFS(high_freq_max_error);

  EXPECT_LE(rms_error, rms_error_);
  EXPECT_LE(low_freq_max_error, low_freq_error_);
  EXPECT_LE(high_freq_max_error, high_freq_error_);
}

// Tests resampling using a given input and output sample rate, and a small
// kernel size.
TEST_P(SincResamplerTest, Resample_SmallKernel) {
  // Make comparisons using one second of data.
  static const double kTestDurationSecs = 1;
  int input_samples = kTestDurationSecs * input_rate_;
  int output_samples = kTestDurationSecs * output_rate_;

  // Nyquist frequency for the input sampling rate.
  double input_nyquist_freq = 0.5 * input_rate_;

  // Source for data to be resampled.
  SinusoidalLinearChirpSource resampler_source(input_rate_, input_samples,
                                               input_nyquist_freq);

  constexpr int kSmallKernelLimit = SincResampler::kMaxKernelSize * 3 / 2;

  const double io_ratio = input_rate_ / static_cast<double>(output_rate_);
  SincResampler resampler(
      io_ratio, kSmallKernelLimit,
      base::BindRepeating(&SinusoidalLinearChirpSource::ProvideInput,
                          base::Unretained(&resampler_source)));

  EXPECT_EQ(resampler.KernelSize(), SincResampler::kMinKernelSize);

  const int kernel_storage_size = resampler.kernel_storage_size_for_testing();
  const int kernel_storage_size_in_bytes = kernel_storage_size * sizeof(float);

  // Force an update to the sample rate ratio to ensure dynamic sample rate
  // changes are working correctly.
  auto kernel = base::HeapArray<float>::Uninit(kernel_storage_size);
  memcpy(kernel.data(), resampler.get_kernel_for_testing(),
         kernel_storage_size_in_bytes);
  resampler.SetRatio(std::numbers::pi);
  ASSERT_NE(0, memcmp(kernel.data(), resampler.get_kernel_for_testing(),
                      kernel_storage_size_in_bytes));
  resampler.SetRatio(io_ratio);
  ASSERT_EQ(0, memcmp(kernel.data(), resampler.get_kernel_for_testing(),
                      kernel_storage_size_in_bytes));

  // TODO(dalecurtis): If we switch to AVX/SSE optimization, we'll need to
  // allocate these on 32-byte boundaries and ensure they're sized % 32 bytes.
  auto resampled_destination = base::HeapArray<float>::Uninit(output_samples);

  // Generate resampled signal.
  resampler.Resample(output_samples, resampled_destination.data());

  // Do not check for the maximum error range for the small kernel size,
  // as there is already quite a bit of test data. This test is only meant to
  // exercise code paths, not ensure quality.
}

// Thresholds chosen arbitrarily based on what each resampling reported during
// testing.  All thresholds are in dbFS, http://en.wikipedia.org/wiki/DBFS.

// Almost all conversions have an RMS error of around -15 dbFS and have a high
// frequency error around -12 dbFS.
static const double kRMSMaxError = -14.94;
static const double kHighFreqMaxError = -12.09;

INSTANTIATE_TEST_SUITE_P(
    SincResamplerTest,
    SincResamplerTest,
    testing::Values(
        // To 16kHz
        std::make_tuple(8000, 16000, kRMSMaxError, -69.26, kHighFreqMaxError),
        std::make_tuple(11025, 16000, kRMSMaxError, -63.97, kHighFreqMaxError),
        // The low freq error of -85.28 dbFS does not work on
        // android-12-x64-rel, android-nougat-x86-rel and fuchsia-arm64-rel.
        std::make_tuple(16000, 16000, kRMSMaxError, -85.26, kHighFreqMaxError),
        std::make_tuple(22050, 16000, -16.77, -67.98, -10.35),
        std::make_tuple(32000, 16000, -19.17, -75.00, -8.82),
        std::make_tuple(44100, 16000, -20.26, -62.40, -7.89),
        std::make_tuple(48000, 16000, -21.05, -53.22, -7.93),
        std::make_tuple(96000, 16000, -23.20, -19.97, -6.98),
        std::make_tuple(192000, 16000, -24.28, -11.57, -6.60),

        // To 32kHz
        std::make_tuple(8000, 32000, kRMSMaxError, -69.26, kHighFreqMaxError),
        std::make_tuple(11025, 32000, kRMSMaxError, -63.97, kHighFreqMaxError),
        std::make_tuple(16000, 32000, kRMSMaxError, -75.28, kHighFreqMaxError),
        std::make_tuple(22050, 32000, kRMSMaxError, -63.82, kHighFreqMaxError),
        // The low freq error of -85.25 dbFS does not work on
        // android-12-x64-rel, android-nougat-x86-rel and fuchsia-arm64-rel.
        std::make_tuple(32000, 32000, kRMSMaxError, -85.24, kHighFreqMaxError),
        std::make_tuple(44100, 32000, -16.78, -67.79, -10.20),
        // The low freq error of -79.11 dbFS does not work on
        // android-12-x64-rel, android-nougat-x86-rel and fuchsia-arm64-rel.
        std::make_tuple(48000, 32000, -17.44, -79.10, -9.73),
        std::make_tuple(96000, 32000, -20.73, -52.60, -7.87),
        std::make_tuple(192000, 32000, -23.67, -20.00, -6.91),

        // To 44.1kHz
        std::make_tuple(8000, 44100, kRMSMaxError, -63.85, kHighFreqMaxError),
        std::make_tuple(11025, 44100, kRMSMaxError, -72.04, kHighFreqMaxError),
        // The low freq error of -63.78 dbFS does not work on
        // android-12-x64-rel, android-nougat-x86-rel and fuchsia-arm64-rel.
        std::make_tuple(16000, 44100, kRMSMaxError, -63.77, kHighFreqMaxError),
        std::make_tuple(22050, 44100, kRMSMaxError, -78.06, kHighFreqMaxError),
        std::make_tuple(32000, 44100, kRMSMaxError, -64.06, kHighFreqMaxError),
        // The low freq error of -85.24 dbFS does not work on
        // android-12-x64-rel, android-nougat-x86-rel and fuchsia-arm64-rel.
        std::make_tuple(44100, 44100, kRMSMaxError, -85.22, kHighFreqMaxError),
        std::make_tuple(48000, 44100, -15.31, -65.58, -11.50),
        std::make_tuple(96000, 44100, -19.14, -73.16, -8.50),
        std::make_tuple(192000, 44100, -22.24, -28.92, -7.20),

        // To 48kHz
        std::make_tuple(8000, 48000, kRMSMaxError, -64.79, kHighFreqMaxError),
        std::make_tuple(11025, 48000, kRMSMaxError, -63.84, kHighFreqMaxError),
        std::make_tuple(16000, 48000, kRMSMaxError, -64.93, kHighFreqMaxError),
        std::make_tuple(22050, 48000, kRMSMaxError, -63.72, kHighFreqMaxError),
        std::make_tuple(32000, 48000, kRMSMaxError, -64.96, kHighFreqMaxError),
        std::make_tuple(44100, 48000, kRMSMaxError, -64.13, kHighFreqMaxError),
        // The low freq error of -85.25 dbFS does not work on
        // android-12-x64-rel, android-nougat-x86-rel and fuchsia-arm64-rel.
        std::make_tuple(48000, 48000, kRMSMaxError, -85.24, kHighFreqMaxError),
        std::make_tuple(96000, 48000, -19.05, -75.32, -8.73),
        std::make_tuple(192000, 48000, -22.10, -32.36, -7.28),

        // To 96kHz
        std::make_tuple(8000, 96000, kRMSMaxError, -64.64, kHighFreqMaxError),
        std::make_tuple(11025, 96000, kRMSMaxError, -63.84, kHighFreqMaxError),
        std::make_tuple(16000, 96000, kRMSMaxError, -64.75, kHighFreqMaxError),
        std::make_tuple(22050, 96000, kRMSMaxError, -63.72, kHighFreqMaxError),
        std::make_tuple(32000, 96000, kRMSMaxError, -64.92, kHighFreqMaxError),
        std::make_tuple(44100, 96000, kRMSMaxError, -64.04, kHighFreqMaxError),
        std::make_tuple(48000, 96000, kRMSMaxError, -84.82, kHighFreqMaxError),
        std::make_tuple(96000, 96000, kRMSMaxError, -85.24, kHighFreqMaxError),
        std::make_tuple(192000, 96000, -19.01, -75.30, -8.71),

        // To 192kHz
        std::make_tuple(8000, 192000, kRMSMaxError, -64.63, kHighFreqMaxError),
        std::make_tuple(11025, 192000, kRMSMaxError, -63.84, kHighFreqMaxError),
        std::make_tuple(16000, 192000, kRMSMaxError, -64.61, kHighFreqMaxError),
        std::make_tuple(22050, 192000, kRMSMaxError, -63.72, kHighFreqMaxError),
        std::make_tuple(32000, 192000, kRMSMaxError, -64.74, kHighFreqMaxError),
        std::make_tuple(44100, 192000, kRMSMaxError, -63.85, kHighFreqMaxError),
        std::make_tuple(48000, 192000, kRMSMaxError, -84.82, kHighFreqMaxError),
        std::make_tuple(96000, 192000, kRMSMaxError, -85.24, kHighFreqMaxError),
        std::make_tuple(192000,
                        192000,
                        kRMSMaxError,
                        -85.24,
                        kHighFreqMaxError)));

// Verify the resampler properly reports the max number of input frames it would
// request.
TEST(SincResamplerTest, GetMaxInputFramesRequestedTest) {
  SincResampler resampler(kSampleRateRatio, SincResampler::kDefaultRequestSize,
                          SincResampler::ReadCB());

  EXPECT_EQ(SincResampler::kDefaultRequestSize,
            resampler.GetMaxInputFramesRequested(resampler.ChunkSize()));

  // Request sizes smaller than ChunkSize should still trigger 1 read.
  EXPECT_EQ(SincResampler::kDefaultRequestSize,
            resampler.GetMaxInputFramesRequested(resampler.ChunkSize() - 10));

  // Request sizes bigger than ChunkSize can trigger multiple reads.
  EXPECT_EQ(2 * SincResampler::kDefaultRequestSize,
            resampler.GetMaxInputFramesRequested(resampler.ChunkSize() + 10));

  // The number of input frames requested should grow proportionally to the
  // output frames requested.
  EXPECT_EQ(
      5 * SincResampler::kDefaultRequestSize,
      resampler.GetMaxInputFramesRequested(4 * resampler.ChunkSize() + 10));

  const int kCustomRequestSize = SincResampler::kDefaultRequestSize + 128;
  SincResampler custom_size_resampler(kSampleRateRatio, kCustomRequestSize,
                                      SincResampler::ReadCB());

  // The input frames requested should be a multiple of the request size.
  EXPECT_EQ(2 * kCustomRequestSize,
            custom_size_resampler.GetMaxInputFramesRequested(
                custom_size_resampler.ChunkSize() + 10));

  // Verify we get results with both downsampling and upsampling ratios.
  SincResampler inverse_ratio_resampler(1.0 / kSampleRateRatio,
                                        SincResampler::kDefaultRequestSize,
                                        SincResampler::ReadCB());

  EXPECT_EQ(2 * SincResampler::kDefaultRequestSize,
            inverse_ratio_resampler.GetMaxInputFramesRequested(
                inverse_ratio_resampler.ChunkSize() + 10));
}

class SincResamplerKernelSizeTest : public testing::Test {
 public:
  SincResamplerKernelSizeTest() = default;
  ~SincResamplerKernelSizeTest() override = default;
};

TEST_F(SincResamplerKernelSizeTest, KernelSizes) {
  constexpr float kTestIoRatio = 2.0;

  // Default case.
  {
    EXPECT_EQ(SincResampler::KernelSizeFromRequestFrames(
                  SincResampler::kDefaultRequestSize),
              SincResampler::kMaxKernelSize);

    SincResampler default_request_resampler(
        kTestIoRatio, SincResampler::kDefaultRequestSize, base::DoNothing());

    EXPECT_EQ(default_request_resampler.KernelSize(),
              SincResampler::kMaxKernelSize);
  }

  constexpr int kSmallKernelLimit = SincResampler::kMaxKernelSize * 3 / 2;

  // Smallest request size allowed for SincResampler::kMaxKernelSize.
  {
    EXPECT_EQ(SincResampler::KernelSizeFromRequestFrames(kSmallKernelLimit + 1),
              SincResampler::kMaxKernelSize);

    SincResampler limit_request_resampler(kTestIoRatio, kSmallKernelLimit + 1,
                                          base::DoNothing());

    EXPECT_EQ(limit_request_resampler.KernelSize(),
              SincResampler::kMaxKernelSize);
  }

  // Smaller request, forcing a smaller kernel.
  {
    EXPECT_EQ(SincResampler::KernelSizeFromRequestFrames(kSmallKernelLimit),
              SincResampler::kMinKernelSize);

    SincResampler small_request_resampler(kTestIoRatio, kSmallKernelLimit,
                                          base::DoNothing());

    EXPECT_EQ(small_request_resampler.KernelSize(),
              SincResampler::kMinKernelSize);
  }

  // Smallest valid request size.
  {
    constexpr int kSmallestRequestFrames =
        SincResampler::kMinKernelSize * 3 / 2 + 1;
    EXPECT_EQ(
        SincResampler::KernelSizeFromRequestFrames(kSmallestRequestFrames),
        SincResampler::kMinKernelSize);

    SincResampler smallest_request_resampler(
        kTestIoRatio, kSmallestRequestFrames, base::DoNothing());

    EXPECT_EQ(smallest_request_resampler.KernelSize(),
              SincResampler::kMinKernelSize);
  }
}

}  // namespace media
