/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
Pseudo =
{
    initialRandomSeed: 49734321,
    randomSeed: 49734321,

    resetRandomSeed: function()
    {
        Pseudo.randomSeed = Pseudo.initialRandomSeed;
    },

    random: function()
    {
        var randomSeed = Pseudo.randomSeed;
        randomSeed = ((randomSeed + 0x7ed55d16) + (randomSeed << 12))  & 0xffffffff;
        randomSeed = ((randomSeed ^ 0xc761c23c) ^ (randomSeed >>> 19)) & 0xffffffff;
        randomSeed = ((randomSeed + 0x165667b1) + (randomSeed << 5))   & 0xffffffff;
        randomSeed = ((randomSeed + 0xd3a2646c) ^ (randomSeed << 9))   & 0xffffffff;
        randomSeed = ((randomSeed + 0xfd7046c5) + (randomSeed << 3))   & 0xffffffff;
        randomSeed = ((randomSeed ^ 0xb55a4f09) ^ (randomSeed >>> 16)) & 0xffffffff;
        Pseudo.randomSeed = randomSeed;
        return (randomSeed & 0xfffffff) / 0x10000000;
    }
};

Statistics =
{
    sampleMean: function(numberOfSamples, sum)
    {
        if (numberOfSamples < 1)
            return 0;
        return sum / numberOfSamples;
    },

    // With sum and sum of squares, we can compute the sample standard deviation in O(1).
    // See https://rniwa.com/2012-11-10/sample-standard-deviation-in-terms-of-sum-and-square-sum-of-samples/
    unbiasedSampleStandardDeviation: function(numberOfSamples, sum, squareSum)
    {
        if (numberOfSamples < 2)
            return 0;
        return Math.sqrt((squareSum - sum * sum / numberOfSamples) / (numberOfSamples - 1));
    },

    geometricMean: function(values)
    {
        if (!values.length)
            return 0;
        var roots = values.map(function(value) { return Math.pow(value, 1 / values.length); })
        return roots.reduce(function(a, b) { return a * b; });
    },

    // Cumulative distribution function
    cdf: function(value, mean, standardDeviation)
    {
        return 0.5 * (1 + Statistics.erf((value - mean) / (Math.sqrt(2 * standardDeviation * standardDeviation))));
    },

    // Approximation of Gauss error function, Abramowitz and Stegun 7.1.26
    erf: function(value)
    {
          var sign = (value >= 0) ? 1 : -1;
          value = Math.abs(value);

          var a1 = 0.254829592;
          var a2 = -0.284496736;
          var a3 = 1.421413741;
          var a4 = -1.453152027;
          var a5 = 1.061405429;
          var p = 0.3275911;

          var t = 1.0 / (1.0 + p * value);
          var y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * Math.exp(-value * value);
          return sign * y;
    },

    largestDeviationPercentage: function(low, mean, high)
    {
        return Math.max(Math.abs(low / mean - 1), (high / mean - 1));
    }
};

Experiment = Utilities.createClass(
    function(includeConcern)
    {
        if (includeConcern)
            this._maxHeap = Heap.createMaxHeap(Experiment.defaults.CONCERN_SIZE);
        this.reset();
    }, {

    reset: function()
    {
        this._sum = 0;
        this._squareSum = 0;
        this._numberOfSamples = 0;
        if (this._maxHeap)
            this._maxHeap.init();
    },

    get sampleCount()
    {
        return this._numberOfSamples;
    },

    sample: function(value)
    {
        this._sum += value;
        this._squareSum += value * value;
        if (this._maxHeap)
            this._maxHeap.push(value);
        ++this._numberOfSamples;
    },

    mean: function()
    {
        return Statistics.sampleMean(this._numberOfSamples, this._sum);
    },

    standardDeviation: function()
    {
        return Statistics.unbiasedSampleStandardDeviation(this._numberOfSamples, this._sum, this._squareSum);
    },

    cdf: function(value)
    {
        return Statistics.cdf(value, this.mean(), this.standardDeviation());
    },

    percentage: function()
    {
        var mean = this.mean();
        return mean ? this.standardDeviation() * 100 / mean : 0;
    },

    concern: function(percentage)
    {
        if (!this._maxHeap)
            return this.mean();

        var size = Math.ceil(this._numberOfSamples * percentage / 100);
        var values = this._maxHeap.values(size);
        return values.length ? values.reduce(function(a, b) { return a + b; }) / values.length : 0;
    },

    score: function(percentage)
    {
        return Statistics.geometricMean([this.mean(), Math.max(this.concern(percentage), 1)]);
    }
});

Experiment.defaults =
{
    CONCERN: 5,
    CONCERN_SIZE: 100,
};

Regression = Utilities.createClass(
    function(samples, getComplexity, getFrameLength, startIndex, endIndex, options)
    {
        var targetFrameRate = options["frame-rate"] || 60;
        var desiredFrameLength = options.desiredFrameLength || 1000/targetFrameRate;
        var bestProfile;

        if (!options.preferredProfile || options.preferredProfile == Strings.json.profiles.slope) {
            var slope = this._calculateRegression(samples, getComplexity, getFrameLength, startIndex, endIndex, {
                shouldClip: true,
                s1: desiredFrameLength,
                t1: 0
            });
            if (!bestProfile || slope.error < bestProfile.error) {
                bestProfile = slope;
                this.profile = Strings.json.profiles.slope;
            }
        }
        if (!options.preferredProfile || options.preferredProfile == Strings.json.profiles.flat) {
            var flat = this._calculateRegression(samples, getComplexity, getFrameLength, startIndex, endIndex, {
                shouldClip: true,
                s1: desiredFrameLength,
                t1: 0,
                t2: 0
            });

            if (!bestProfile || flat.error < bestProfile.error) {
                bestProfile = flat;
                this.profile = Strings.json.profiles.flat;
            }
        }

        this.startIndex = Math.min(startIndex, endIndex);
        this.endIndex = Math.max(startIndex, endIndex);

        this.complexity = bestProfile.complexity;
        this.s1 = bestProfile.s1;
        this.t1 = bestProfile.t1;
        this.s2 = bestProfile.s2;
        this.t2 = bestProfile.t2;
        this.stdev1 = bestProfile.stdev1;
        this.stdev2 = bestProfile.stdev2;
        this.n1 = bestProfile.n1;
        this.n2 = bestProfile.n2;
        this.error = bestProfile.error;
    }, {

    valueAt: function(complexity)
    {
        if (this.n1 == 1 || complexity > this.complexity)
            return this.s2 + this.t2 * complexity;
        return this.s1 + this.t1 * complexity;
    },

    // A generic two-segment piecewise regression calculator. Based on Kundu/Ubhaya
    //
    // Minimize sum of (y - y')^2
    // where                        y = s1 + t1*x
    //                              y = s2 + t2*x
    //                y' = s1 + t1*x' = s2 + t2*x'   if x_0 <= x' <= x_n
    //
    // Allows for fixing s1, t1, s2, t2
    //
    // x is assumed to be complexity, y is frame length. Can be used for pure complexity-FPS
    // analysis or for ramp controllers since complexity monotonically decreases with time.
    _calculateRegression: function(samples, getComplexity, getFrameLength, startIndex, endIndex, options)
    {
        if (startIndex == endIndex) {
            // Only one sample point; we can't calculate any regression.
            var x = getComplexity(samples, startIndex);
            return {
                complexity: x,
                s1: x,
                t1: 0,
                s2: x,
                t2: 0,
                error1: 0,
                error2: 0
            };
        }

        // x is expected to increase in complexity
        var iterationDirection = endIndex > startIndex ? 1 : -1;
        var lowComplexity = getComplexity(samples, startIndex);
        var highComplexity = getComplexity(samples, endIndex);
        var a1 = 0, b1 = 0, c1 = 0, d1 = 0, h1 = 0, k1 = 0;
        var a2 = 0, b2 = 0, c2 = 0, d2 = 0, h2 = 0, k2 = 0;

        // Iterate from low to high complexity
        for (var i = startIndex; iterationDirection * (endIndex - i) > -1; i += iterationDirection) {
            var x = getComplexity(samples, i);
            var y = getFrameLength(samples, i);
            a2 += 1;
            b2 += x;
            c2 += x * x;
            d2 += y;
            h2 += y * x;
            k2 += y * y;
        }

        var s1_best, t1_best, s2_best, t2_best, n1_best, n2_best, error1_best, error2_best, x_best, x_prime;

        function setBest(s1, t1, error1, s2, t2, error2, splitIndex, x_prime, x)
        {
            s1_best = s1;
            t1_best = t1;
            error1_best = error1;
            s2_best = s2;
            t2_best = t2;
            error2_best = error2;
            // Number of samples included in the first segment, inclusive of splitIndex
            n1_best = iterationDirection * (splitIndex - startIndex) + 1;
            // Number of samples included in the second segment
            n2_best = iterationDirection * (endIndex - splitIndex);
            if (!options.shouldClip || (x_prime >= lowComplexity && x_prime <= highComplexity))
                x_best = x_prime;
            else {
                // Discontinuous piecewise regression
                x_best = x;
            }
        }

        // Iterate from startIndex to endIndex - 1, inclusive
        for (var i = startIndex; iterationDirection * (endIndex - i) > 0; i += iterationDirection) {
            var x = getComplexity(samples, i);
            var y = getFrameLength(samples, i);
            var xx = x * x;
            var yx = y * x;
            var yy = y * y;
            // a1, b1, etc. is sum from startIndex to i, inclusive
            a1 += 1;
            b1 += x;
            c1 += xx;
            d1 += y;
            h1 += yx;
            k1 += yy;
            // a2, b2, etc. is sum from i+1 to endIndex, inclusive
            a2 -= 1;
            b2 -= x;
            c2 -= xx;
            d2 -= y;
            h2 -= yx;
            k2 -= yy;

            var A = c1*d1 - b1*h1;
            var B = a1*h1 - b1*d1;
            var C = a1*c1 - b1*b1;
            var D = c2*d2 - b2*h2;
            var E = a2*h2 - b2*d2;
            var F = a2*c2 - b2*b2;
            var s1 = options.s1 !== undefined ? options.s1 : (A / C);
            var t1 = options.t1 !== undefined ? options.t1 : (B / C);
            var s2 = options.s2 !== undefined ? options.s2 : (D / F);
            var t2 = options.t2 !== undefined ? options.t2 : (E / F);
            // Assumes that the two segments meet
            var x_prime = (s1 - s2) / (t2 - t1);

            var error1 = (k1 + a1*s1*s1 + c1*t1*t1 - 2*d1*s1 - 2*h1*t1 + 2*b1*s1*t1) || Number.MAX_VALUE;
            var error2 = (k2 + a2*s2*s2 + c2*t2*t2 - 2*d2*s2 - 2*h2*t2 + 2*b2*s2*t2) || Number.MAX_VALUE;

            if (i == startIndex) {
                setBest(s1, t1, error1, s2, t2, error2, i, x_prime, x);
                continue;
            }

            if (C == 0 || F == 0)
                continue;

            // Projected point is not between this and the next sample
            if (x_prime > getComplexity(samples, i + iterationDirection) || x_prime < x) {
                // Calculate lambda, which divides the weight of this sample between the two lines

                // These values remove the influence of this sample
                var I = c1 - 2*b1*x + a1*xx;
                var H = C - I;
                var G = A + B*x - C*y;

                var J = D + E*x - F*y;
                var K = c2 - 2*b2*x + a2*xx;

                var lambda = (G*F + G*K - H*J) / (I*J + G*K);
                if (lambda > 0 && lambda < 1) {
                    var lambda1 = 1 - lambda;
                    s1 = options.s1 !== undefined ? options.s1 : ((A - lambda1*(-h1*x + d1*xx + c1*y - b1*yx)) / (C - lambda1*I));
                    t1 = options.t1 !== undefined ? options.t1 : ((B - lambda1*(h1 - d1*x - b1*y + a1*yx)) / (C - lambda1*I));
                    s2 = options.s2 !== undefined ? options.s2 : ((D + lambda1*(-h2*x + d2*xx + c2*y - b2*yx)) / (F + lambda1*K));
                    t2 = options.t2 !== undefined ? options.t2 : ((E + lambda1*(h2 - d2*x - b2*y + a2*yx)) / (F + lambda1*K));
                    x_prime = (s1 - s2) / (t2 - t1);

                    error1 = ((k1 + a1*s1*s1 + c1*t1*t1 - 2*d1*s1 - 2*h1*t1 + 2*b1*s1*t1) - lambda1 * Math.pow(y - (s1 + t1*x), 2)) || Number.MAX_VALUE;
                    error2 = ((k2 + a2*s2*s2 + c2*t2*t2 - 2*d2*s2 - 2*h2*t2 + 2*b2*s2*t2) + lambda1 * Math.pow(y - (s2 + t2*x), 2)) || Number.MAX_VALUE;
                } else if (t1 != t2)
                    continue;
            }

            if (error1 + error2 < error1_best + error2_best)
                setBest(s1, t1, error1, s2, t2, error2, i, x_prime, x);
        }

        return {
            complexity: x_best,
            s1: s1_best,
            t1: t1_best,
            stdev1: Math.sqrt(error1_best / n1_best),
            s2: s2_best,
            t2: t2_best,
            stdev2: Math.sqrt(error2_best / n2_best),
            error: error1_best + error2_best,
            n1: n1_best,
            n2: n2_best
        };
    }
});

Utilities.extendObject(Regression, {
    bootstrap: function(samples, iterationCount, processResample, confidencePercentage)
    {
        var sampleLength = samples.length;
        var resample = new Array(sampleLength);

        var bootstrapEstimator = new Experiment;
        var bootstrapData = new Array(iterationCount);

        Pseudo.resetRandomSeed();
        for (var i = 0; i < iterationCount; ++i) {
            for (var j = 0; j < sampleLength; ++j)
                resample[j] = samples[Math.floor(Pseudo.random() * sampleLength)];

            var resampleResult = processResample(resample);
            bootstrapEstimator.sample(resampleResult);
            bootstrapData[i] = resampleResult;
        }

        bootstrapData.sort(function(a, b) { return a - b; });
        return {
            confidenceLow: bootstrapData[Math.round((iterationCount - 1) * (1 - confidencePercentage) / 2)],
            confidenceHigh: bootstrapData[Math.round((iterationCount - 1) * (1 + confidencePercentage) / 2)],
            median: bootstrapData[Math.round(iterationCount / 2)],
            mean: bootstrapEstimator.mean(),
            data: bootstrapData,
            confidencePercentage: confidencePercentage
        };
    }
});
