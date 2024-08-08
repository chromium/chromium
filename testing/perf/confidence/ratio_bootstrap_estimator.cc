// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/perf/confidence/ratio_bootstrap_estimator.h"

#define _USE_MATH_DEFINES  // Needed to get M_SQRT1_2 on Windows.
#include <math.h>

#include <algorithm>
#include <memory>
#include <vector>

#ifdef UNSAFE_BUFFERS_BUILD
// Not used with untrusted inputs.
#pragma allow_unsafe_buffers
#endif

using std::lower_bound;
using std::min;
using std::numeric_limits;
using std::sort;
using std::unique_ptr;
using std::vector;

// Inverse normal CDF, e.g. InverseNormalCDF(0.975) ~= 1.96
// (a 95% CI will cover +/- 1.96 standard deviations from the mean).
//
// For some reason, C has erf() in its standard library, but not its inverse,
// so we have to build it ourselves (which is a bit annoying, since we only
// really want it to convert the confidence interval quantiles!). This is
// nontrivial, but fortunately, others have figured it out. This is an
// implementation of “Algorithm AS 241: The Percentage Points of the Normal
// Distribution” (Wichura), and is roughly the same as was used in GNU R
// until recently. We don't need extreme precision, so we've only used the
// version that is accurate to about seven decimal digits (the other one
// is pretty much the same, just with even more constants).
double RatioBootstrapEstimator::InverseNormalCDF(double p) {
  double q = p - 0.5;
  if (fabs(q) < 0.425) {
    const double a0 = 3.3871327179e0;
    const double a1 = 5.0434271938e1;
    const double a2 = 1.5929113202e2;
    const double a3 = 5.9109374720e1;
    const double b1 = 1.7895169469e1;
    const double b2 = 7.8757757664e1;
    const double b3 = 6.7187563600e1;

    double r = 0.180625 - q * q;
    return q * (((a3 * r + a2) * r + a1) * r + a0) /
           (((b3 * r + b2) * r + b1) * r + 1.0);
  } else {
    double r = (q < 0) ? p : 1.0 - p;
    if (r < 0.0) {
      return numeric_limits<double>::quiet_NaN();
    }
    r = sqrt(-log(r));
    double ret;
    if (r < 5.0) {
      const double c0 = 1.4234372777e0;
      const double c1 = 2.7568153900e0;
      const double c2 = 1.3067284816e0;
      const double c3 = 1.7023821103e-1;
      const double d1 = 7.3700164250e-1;
      const double d2 = 1.2021132975e-1;

      r -= 1.6;
      ret = (((c3 * r + c2) * r + c1) * r + c0) / ((d2 * r + d1) * r + 1.0);
    } else {
      const double e0 = 6.6579051150e0;
      const double e1 = 3.0812263860e0;
      const double e2 = 4.2868294337e-1;
      const double e3 = 1.7337203997e-2;
      const double f1 = 2.4197894225e-1;
      const double f2 = 1.2258202635e-2;

      r -= 5.0;
      ret = (((e3 * r + e2) * r + e1) * r + e0) / ((f2 * r + f1) * r + 1.0);
    }
    return (q < 0) ? -ret : ret;
  }
}

namespace {

// Normal (Gaussian) CDF, e.g. NormCRF(1.96) ~= 0.975
// (+/- 1.96 standard deviations would cover a 95% CI).
double NormalCDF(double q) {
  return 0.5 * erfc(-q * M_SQRT1_2);
}

// Compute percentiles of the bootstrap distribution (the inverse of G).
// We estimate G by Ĝ, the bootstrap estimate of G (text above eq. 2.9
// in the paper). Note that unlike bcajack, we interpolate between values
// to get slightly better accuracy.
double ComputeBCa(const double* estimates,
                  size_t num_estimates,
                  double alpha,
                  double z0,
                  double a) {
  double z_alpha = RatioBootstrapEstimator::InverseNormalCDF(alpha);

  // Eq. (2.2); the basic BCa formula.
  double q = NormalCDF(z0 + (z0 + z_alpha) / (1 - a * (z0 + z_alpha)));

  double index = q * (num_estimates - 1);
  int base_index = index;
  if (base_index == static_cast<int>(num_estimates - 1)) {
    // The edge of the CDF; note that R would warn in this case.
    return estimates[base_index];
  }
  double frac = index - base_index;
  return estimates[base_index] +
         frac * (estimates[base_index + 1] - estimates[base_index]);
}

// Calculate Ĝ (the fraction of estimates that are less than search-value).
double FindCDF(const double* estimates,
               size_t num_estimates,
               double search_val) {
  // Find first x where x >= search_val.
  auto it = lower_bound(estimates, estimates + num_estimates, search_val);
  if (it == estimates + num_estimates) {
    // All values are less than search_val.
    // Note that R warns in this case.
    return 1.0;
  }

  unsigned index = std::distance(estimates, it);
  if (index == 0) {
    // All values are >= search_val.
    // Note that R warns in this case.
    return 0.0;
  }

  // TODO(sesse): Consider whether we should interpolate here, like in
  // compute_bca().
  return index / double(num_estimates);
}

}  // namespace

// Find the ratio estimate over all values except for the one at skip_index,
// i.e., leave-one-out. (If skip_index == -1 or similar, simply compute over
// all values.) This is used in the jackknife estimate for the acceleration.
double RatioBootstrapEstimator::EstimateRatioExcept(
    const vector<RatioBootstrapEstimator::Sample>& x,
    int skip_index) {
  double before = 0.0, after = 0.0;
  for (unsigned i = 0; i < x.size(); ++i) {
    if (static_cast<int>(i) == skip_index) {
      continue;
    }
    before += x[i].before;
    after += x[i].after;
  }
  return before / after;
}

// Similar, for the geometric mean across all the data sets.
double RatioBootstrapEstimator::EstimateGeometricMeanExcept(
    const vector<vector<RatioBootstrapEstimator::Sample>>& x,
    int skip_index) {
  double geometric_mean = 1.0;
  for (const auto& samples : x) {
    geometric_mean *= EstimateRatioExcept(samples, skip_index);
  }
  return pow(geometric_mean, 1.0 / x.size());
}

vector<RatioBootstrapEstimator::Estimate>
RatioBootstrapEstimator::ComputeRatioEstimates(
    const vector<vector<RatioBootstrapEstimator::Sample>>& data,
    unsigned num_resamples,
    double confidence_level,
    bool compute_geometric_mean) {
  if (data.empty() || num_resamples < 10 || confidence_level <= 0.0 ||
      confidence_level >= 1.0) {
    return {};
  }

  unsigned num_observations = numeric_limits<unsigned>::max();
  for (const vector<Sample>& samples : data) {
    num_observations = min<unsigned>(num_observations, samples.size());
  }

  // Allocate some memory for temporaries that we need.
  unsigned num_dimensions = data.size();
  unique_ptr<double[]> before(new double[num_dimensions]);
  unique_ptr<double[]> after(new double[num_dimensions]);
  unique_ptr<double[]> all_estimates(
      new double[(num_dimensions + compute_geometric_mean) * num_resamples]);

  // Do our bootstrap resampling. Note that we can sample independently
  // from the numerator and denumerator (which the R packages cannot do);
  // this makes sense, because they are not pairs. This allows us to sometimes
  // get slightly narrower confidence intervals.
  //
  // When computing the geometric mean, we could perhaps consider doing
  // similar independent sampling across the various data sets, but we
  // currently don't do so.
  for (unsigned i = 0; i < num_resamples; ++i) {
    for (unsigned d = 0; d < num_dimensions; ++d) {
      before[d] = 0.0;
      after[d] = 0.0;
    }
    for (unsigned j = 0; j < num_observations; ++j) {
      unsigned r1 = gen_();
      unsigned r2 = gen_();

      // NOTE: The bias from the modulo here should be insignificant.
      for (unsigned d = 0; d < num_dimensions; ++d) {
        unsigned index1 = r1 % data[d].size();
        unsigned index2 = r2 % data[d].size();
        before[d] += data[d][index1].before;
        after[d] += data[d][index2].after;
      }
    }
    double geometric_mean = 1.0;
    for (unsigned d = 0; d < num_dimensions; ++d) {
      double ratio = before[d] / after[d];
      all_estimates[d * num_resamples + i] = ratio;
      geometric_mean *= ratio;
    }
    if (compute_geometric_mean) {
      all_estimates[num_dimensions * num_resamples + i] =
          pow(geometric_mean, 1.0 / num_dimensions);
    }
  }

  // Make our point estimates.
  vector<Estimate> result;
  for (unsigned d = 0; d < num_dimensions + compute_geometric_mean; ++d) {
    bool is_geometric_mean = (d == num_dimensions);

    double* estimates = &all_estimates[d * num_resamples];

    // FindCDF() and others expect sorted data.
    sort(estimates, estimates + num_resamples);

    // Make our point estimate.
    double point_estimate = is_geometric_mean
                                ? EstimateGeometricMeanExcept(data, -1)
                                : EstimateRatioExcept(data[d], -1);

    // Compute bias correction, Eq. (2.9).
    double z0 =
        InverseNormalCDF(FindCDF(estimates, num_resamples, point_estimate));

    // Compute acceleration. This is Eq. (3.11), except that there seems
    // to be a typo; the sign seems to be flipped compared to bcajack,
    // so we correct that (see line 148 of bcajack.R). Note that bcajack
    // uses the average-of-averages instead of the point estimate,
    // which doesn't seem to match the paper, but the difference is
    // completely insigificant in practice.
    double sum_d_squared = 0.0;
    double sum_d_cubed = 0.0;
    if (is_geometric_mean) {
      // NOTE: If there are differing numbers of samples in the different
      // data series, this will be ever so slightly off, but the effect
      // should hopefully be small.
      for (unsigned i = 0; i < num_observations; ++i) {
        double dd = point_estimate - EstimateGeometricMeanExcept(data, i);
        sum_d_squared += dd * dd;
        sum_d_cubed += dd * dd * dd;
      }
    } else {
      for (unsigned i = 0; i < data[d].size(); ++i) {
        double dd = point_estimate - EstimateRatioExcept(data[d], i);
        sum_d_squared += dd * dd;
        sum_d_cubed += dd * dd * dd;
      }
    }
    double a = sum_d_cubed /
               (6.0 * sqrt(sum_d_squared * sum_d_squared * sum_d_squared));

    double alpha = 0.5 * (1 - confidence_level);

    Estimate est;
    est.point_estimate = point_estimate;
    est.lower = ComputeBCa(estimates, num_resamples, alpha, z0, a);
    est.upper = ComputeBCa(estimates, num_resamples, 1.0 - alpha, z0, a);
    result.push_back(est);
  }
  return result;
}
