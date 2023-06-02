/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
Sampler = Utilities.createClass(
    function(seriesCount, expectedSampleCount, processor)
    {
        this._processor = processor;

        this.samples = [];
        for (var i = 0; i < seriesCount; ++i) {
            var array = new Array(expectedSampleCount);
            array.fill(0);
            this.samples[i] = array;
        }
        this.sampleCount = 0;
    }, {

    record: function() {
        // Assume that arguments.length == this.samples.length
        for (var i = 0; i < arguments.length; i++) {
            this.samples[i][this.sampleCount] = arguments[i];
        }
        ++this.sampleCount;
    },

    processSamples: function()
    {
        var results = {};

        // Remove unused capacity
        this.samples = this.samples.map(function(array) {
            return array.slice(0, this.sampleCount);
        }, this);

        this._processor.processSamples(results);

        return results;
    }
});

Controller = Utilities.createClass(
    function(benchmark, options)
    {
        // Initialize timestamps relative to the start of the benchmark
        // In start() the timestamps are offset by the start timestamp
        this._startTimestamp = 0;
        this._endTimestamp = options["test-interval"];
        this._targetFrameRate = options["frame-rate"] || 60;
        // Default data series: timestamp, complexity, estimatedFrameLength
        var sampleSize = options["sample-capacity"] || (this._targetFrameRate * options["test-interval"] / 1000);
        this._sampler = new Sampler(options["series-count"] || 3, sampleSize, this);
        this._marks = {};

        this._frameLengthEstimator = new SimpleKalmanEstimator(options["kalman-process-error"], options["kalman-measurement-error"]);
        this._isFrameLengthEstimatorEnabled = true;

        // Length of subsequent intervals; a value of 0 means use no intervals
        this.intervalSamplingLength = 100;

        this.initialComplexity = 1;
    }, {

    set isFrameLengthEstimatorEnabled(enabled) {
        this._isFrameLengthEstimatorEnabled = enabled;
    },

    start: function(startTimestamp, stage)
    {
        this._startTimestamp = startTimestamp;
        this._endTimestamp += startTimestamp;
        this._previousTimestamp = startTimestamp;
        this._measureAndResetInterval(startTimestamp);
        this.recordFirstSample(startTimestamp, stage);
    },

    recordFirstSample: function(startTimestamp, stage)
    {
        this._sampler.record(startTimestamp, stage.complexity(), -1);
        this.mark(Strings.json.samplingStartTimeOffset, startTimestamp);
    },

    mark: function(comment, timestamp, data) {
        data = data || {};
        data.time = timestamp;
        data.index = this._sampler.sampleCount;
        this._marks[comment] = data;
    },

    containsMark: function(comment) {
        return comment in this._marks;
    },

    filterOutOutliers: function(array)
    {
        if (array.length == 0)
            return [];

        array.sort((a, b) => a - b);
        var q1 = array[Math.min(Math.round(array.length * 1 / 4), array.length - 1)];
        var q3 = array[Math.min(Math.round(array.length * 3 / 4), array.length - 1)];
        var interquartileRange = q3 - q1;
        var minimum = q1 - interquartileRange * 1.5;
        var maximum = q3 + interquartileRange * 1.5;
        return array.filter(x => x >= minimum && x <= maximum);
    },

    _measureAndResetInterval: function(currentTimestamp)
    {
        var sampleCount = this._sampler.sampleCount;
        var averageFrameLength = 0;

        if (this._intervalEndTimestamp) {
            var durations = [];
            for (var i = Math.max(this._intervalStartIndex, 1); i < sampleCount; ++i) {
                durations.push(this._sampler.samples[0][i] - this._sampler.samples[0][i - 1]);
            }
            var filteredDurations = this.filterOutOutliers(durations);
            if (filteredDurations.length > 0)
                averageFrameLength = filteredDurations.reduce((a, b) => a + b, 0) / filteredDurations.length;
        }

        this._intervalStartIndex = sampleCount;
        this._intervalEndTimestamp = currentTimestamp + this.intervalSamplingLength;

        return averageFrameLength;
    },

    update: function(timestamp, stage)
    {
        var lastFrameLength = timestamp - this._previousTimestamp;
        this._previousTimestamp = timestamp;

        var frameLengthEstimate = -1, intervalAverageFrameLength = -1;
        var didFinishInterval = false;
        if (!this.intervalSamplingLength) {
            if (this._isFrameLengthEstimatorEnabled) {
                this._frameLengthEstimator.sample(lastFrameLength);
                frameLengthEstimate = this._frameLengthEstimator.estimate;
            }
        } else {
            this.registerFrameTime(lastFrameLength);
            if (this.intervalHasConcluded(timestamp)) {
                var intervalStartTimestamp = this._sampler.samples[0][this._intervalStartIndex];
                intervalAverageFrameLength = this._measureAndResetInterval(timestamp);
                if (this._isFrameLengthEstimatorEnabled) {
                    this._frameLengthEstimator.sample(intervalAverageFrameLength);
                    frameLengthEstimate = this._frameLengthEstimator.estimate;
                }
                didFinishInterval = true;
                this.didFinishInterval(timestamp, stage, intervalAverageFrameLength);
                this._frameLengthEstimator.reset();
            }
        }

        this._sampler.record(timestamp, stage.complexity(), frameLengthEstimate);
        this.tune(timestamp, stage, lastFrameLength, didFinishInterval, intervalAverageFrameLength);
    },

    registerFrameTime: function(lastFrameLength)
    {
    },

    intervalHasConcluded: function(timestamp)
    {
        return timestamp >= this._intervalEndTimestamp;
    },

    didFinishInterval: function(timestamp, stage, intervalAverageFrameLength)
    {
    },

    tune: function(timestamp, stage, lastFrameLength, didFinishInterval, intervalAverageFrameLength)
    {
    },

    shouldStop: function(timestamp)
    {
        return timestamp > this._endTimestamp;
    },

    results: function()
    {
        return this._sampler.processSamples();
    },

    _processComplexitySamples: function(complexitySamples)
    {
        complexitySamples.sort(function(a, b) {
            return complexitySamples.getFieldInDatum(a, Strings.json.complexity) - complexitySamples.getFieldInDatum(b, Strings.json.complexity);
        });
    },

    _processMarks: function()
    {
        for (var markName in this._marks)
            this._marks[markName].time -= this._startTimestamp;
        return this._marks;
    },
    _processControllerSamples: function()
    {
        var controllerSamples = new SampleData;
        controllerSamples.addField(Strings.json.time, 0);
        controllerSamples.addField(Strings.json.complexity, 1);
        controllerSamples.addField(Strings.json.frameLength, 2);
        controllerSamples.addField(Strings.json.smoothedFrameLength, 3);

        var samples = this._sampler.samples;
        samples[0].forEach(function(timestamp, i) {
            var sample = controllerSamples.createDatum();
            controllerSamples.push(sample);

            // Represent time in milliseconds
            controllerSamples.setFieldInDatum(sample, Strings.json.time, timestamp - this._startTimestamp);
            controllerSamples.setFieldInDatum(sample, Strings.json.complexity, samples[1][i]);

            if (i == 0)
                controllerSamples.setFieldInDatum(sample, Strings.json.frameLength, 1000/this._targetFrameRate);
            else
                controllerSamples.setFieldInDatum(sample, Strings.json.frameLength, timestamp - samples[0][i - 1]);

            if (samples[2][i] != -1)
                controllerSamples.setFieldInDatum(sample, Strings.json.smoothedFrameLength, samples[2][i]);
        }, this);

        return controllerSamples;
    },

    processSamples: function(results)
    {
        results[Strings.json.marks] = this._processMarks();

        var controllerSamples = this._processControllerSamples();
        var complexitySamples = new SampleData(controllerSamples.fieldMap);

        results[Strings.json.samples] = {};
        results[Strings.json.samples][Strings.json.controller] = controllerSamples;
        results[Strings.json.samples][Strings.json.complexity] = complexitySamples;
        controllerSamples.forEach(function (sample) {
            complexitySamples.push(sample);
        });
        this._processComplexitySamples(complexitySamples);
    }
});

FixedController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        Controller.call(this, benchmark, options);
        this.initialComplexity = options["complexity"];
        this.intervalSamplingLength = 0;
    }
);

AdaptiveController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        // Data series: timestamp, complexity, estimatedIntervalFrameLength
        Controller.call(this, benchmark, options);

        // All tests start at 0, so we expect to see the target fps quickly.
        this._samplingTimestamp = options["test-interval"] / 2;
        this._startedSampling = false;
        this._targetFrameRate = options["frame-rate"];
        this._pid = new PIDController(this._targetFrameRate);

        this._intervalFrameCount = 0;
        this._numberOfFramesToMeasurePerInterval = 4;
    }, {

    start: function(startTimestamp, stage)
    {
        Controller.prototype.start.call(this, startTimestamp, stage);

        this._samplingTimestamp += startTimestamp;
        this._intervalTimestamp = startTimestamp;
    },

    recordFirstSample: function(startTimestamp, stage)
    {
        this._sampler.record(startTimestamp, stage.complexity(), -1);
    },

    update: function(timestamp, stage)
    {
        if (!this._startedSampling && timestamp >= this._samplingTimestamp) {
            this._startedSampling = true;
            this.mark(Strings.json.samplingStartTimeOffset, this._samplingTimestamp);
        }

        // Start the work for the next frame.
        ++this._intervalFrameCount;

        if (this._intervalFrameCount < this._numberOfFramesToMeasurePerInterval) {
            this._sampler.record(timestamp, stage.complexity(), -1);
            return;
        }

        // Adjust the test to reach the desired FPS.
        var intervalLength = timestamp - this._intervalTimestamp;
        this._frameLengthEstimator.sample(intervalLength / this._numberOfFramesToMeasurePerInterval);
        var intervalEstimatedFrameRate = 1000 / this._frameLengthEstimator.estimate;
        var tuneValue = -this._pid.tune(timestamp - this._startTimestamp, intervalLength, intervalEstimatedFrameRate);
        tuneValue = tuneValue > 0 ? Math.floor(tuneValue) : Math.ceil(tuneValue);
        stage.tune(tuneValue);

        this._sampler.record(timestamp, stage.complexity(), this._frameLengthEstimator.estimate);

        // Start the next interval.
        this._intervalFrameCount = 0;
        this._intervalTimestamp = timestamp;
    }
});

RampController = Utilities.createSubclass(Controller,
    function(benchmark, options)
    {
        this.targetFPS = options["frame-rate"] || 60;

        // The tier warm-up takes at most 5 seconds
        options["sample-capacity"] = (options["test-interval"] / 1000 + 5) * this.targetFPS;
        Controller.call(this, benchmark, options);

        // Initially start with a tier test to find the bounds
        // The number of objects in a tier test is 10^|_tier|
        this._tier = -.5;
        // The timestamp is first set after the first interval completes
        this._tierStartTimestamp = 0;
        this._minimumComplexity = 1;
        this._maximumComplexity = 1;

        this._testLength = options["test-interval"];

        // After the tier range is determined, figure out the number of ramp iterations
        var minimumRampLength = 3000;
        var totalRampIterations = Math.max(1, Math.floor(this._endTimestamp / minimumRampLength));
        // Give a little extra room to run since the ramps won't be exactly this length
        this._rampLength = Math.floor((this._endTimestamp - totalRampIterations * this.intervalSamplingLength) / totalRampIterations);
        this._rampDidWarmup = false;
        this._rampRegressions = [];

        this._finishedTierSampling = false;
        this._changePointEstimator = new Experiment;
        this._minimumComplexityEstimator = new Experiment;
        // Estimates all frames within an interval
        this._intervalFrameLengthEstimator = new Experiment;

        // Used for regression calculations in the ramps
        this.frameLengthDesired = 1000/this.targetFPS;
        // Add some tolerance; frame lengths shorter than this are considered to be @ the desired frame length
        this.frameLengthDesiredThreshold = 1000/(this.targetFPS - 2);
        // During tier sampling get at least this slow to find the right complexity range
        this.frameLengthTierThreshold = 1000/(this.targetFPS * 0.5);
        // Try to make each ramp get this slow so that we can cross the break point
        this.frameLengthRampLowerThreshold = 1000/(this.targetFPS * 0.75);
        // Do not let the regression calculation at the maximum complexity of a ramp get slower than this threshold
        this.frameLengthRampUpperThreshold = 1000/(this.targetFPS / 3);
    }, {

    // If the engine can handle the tier's complexity at the desired frame rate, test for a short
    // period, then move on to the next tier
    tierFastTestLength: 250,
    // If the engine is under stress, let the test run a little longer to let the measurement settle
    tierSlowTestLength: 750,
    // Tier intervals must have this number of non-outlier frames in order to end.
    numberOfFramesRequiredInInterval: 9,

    rampWarmupLength: 200,

    start: function(startTimestamp, stage)
    {
        Controller.prototype.start.call(this, startTimestamp, stage);
        this._rampStartTimestamp = 0;
        this.intervalSamplingLength = 100;
        this._frameTimeHistory = [];
    },

    registerFrameTime: function(lastFrameLength)
    {
        this._frameTimeHistory.push(lastFrameLength);
    },

    intervalHasConcluded: function(timestamp)
    {
        if (!Controller.prototype.intervalHasConcluded.call(this, timestamp))
            return false;

        return this._finishedTierSampling || this.filterOutOutliers(this._frameTimeHistory).length > this.numberOfFramesRequiredInInterval;
    },

    didFinishInterval: function(timestamp, stage, intervalAverageFrameLength)
    {
        this._frameTimeHistory = [];
        if (!this._finishedTierSampling) {
            if (this._tierStartTimestamp > 0 && timestamp < this._tierStartTimestamp + this.tierFastTestLength)
                return;

            var currentComplexity = stage.complexity();
            var currentFrameLength = this._frameLengthEstimator.estimate;
            if (currentFrameLength < this.frameLengthTierThreshold) {
                var isAnimatingAtTargetFPS = currentFrameLength < this.frameLengthDesiredThreshold;
                var hasFinishedSlowTierTest = timestamp > this._tierStartTimestamp + this.tierSlowTestLength;

                if (!isAnimatingAtTargetFPS && !hasFinishedSlowTierTest)
                    return;

                // We're measuring at the target fps, so quickly move on to the next tier, or
                // we're slower than the target fps, but we've let this tier run long enough to
                // get an estimate
                this._lastTierComplexity = currentComplexity;
                this._lastTierFrameLength = currentFrameLength;

                if (currentComplexity <= 50)
                    this._tier += 1/2;
                else if (currentComplexity <= 10000)
                    this._tier += 1/4;
                else
                    this._tier += 1/8;
                this._endTimestamp = timestamp + this._testLength;
                var nextTierComplexity = Math.max(Math.round(Math.pow(10, this._tier)), currentComplexity + 1);
                stage.tune(nextTierComplexity - currentComplexity);

                // Some tests may be unable to go beyond a certain capacity. If so, don't keep moving up tiers
                if (stage.complexity() - currentComplexity > 0 || nextTierComplexity == 1) {
                    this._tierStartTimestamp = timestamp;
                    this.mark("Complexity: " + nextTierComplexity, timestamp);
                    return;
                }
            } else if (timestamp < this._tierStartTimestamp + this.tierSlowTestLength)
                return;

            this._finishedTierSampling = true;
            this.isFrameLengthEstimatorEnabled = false;
            this.intervalSamplingLength = 120;

            // Extend the test length so that the full test length is made of the ramps
            this._endTimestamp = timestamp + this._testLength;
            this.mark(Strings.json.samplingStartTimeOffset, timestamp);

            this._minimumComplexity = 1;
            this._possibleMinimumComplexity = this._minimumComplexity;
            this._minimumComplexityEstimator.sample(this._minimumComplexity);

            // Sometimes this last tier will drop the frame length well below the threshold.
            // Avoid going down that far since it means fewer measurements are taken in the target fps area.
            // Interpolate a maximum complexity that gets us around the lowest threshold.
            // Avoid doing this calculation if we never get out of the first tier (where this._lastTierComplexity is undefined).
            if (this._lastTierComplexity && this._lastTierComplexity != currentComplexity)
                this._maximumComplexity = Math.floor(Utilities.lerp(Utilities.progressValue(this.frameLengthTierThreshold, this._lastTierFrameLength, currentFrameLength), this._lastTierComplexity, currentComplexity));
            else {
                // If the browser is capable of handling the most complex version of the test, use that
                this._maximumComplexity = currentComplexity;
            }
            this._possibleMaximumComplexity = this._maximumComplexity;

            // If we get ourselves onto a ramp where the maximum complexity does not yield slow enough FPS,
            // We'll use this as a boundary to find a higher maximum complexity for the next ramp
            this._lastTierComplexity = currentComplexity;
            this._lastTierFrameLength = currentFrameLength;

            // First ramp
            stage.tune(this._maximumComplexity - currentComplexity);
            this._rampDidWarmup = false;
            // Start timestamp represents start of ramp iteration and warm up
            this._rampStartTimestamp = timestamp;
            return;
        }

        if ((timestamp - this._rampStartTimestamp) < this.rampWarmupLength)
            return;

        if (this._rampDidWarmup)
            return;

        this._rampDidWarmup = true;
        this._currentRampLength = this._rampStartTimestamp + this._rampLength - timestamp;
        // Start timestamp represents start of ramp down, after warm up
        this._rampStartTimestamp = timestamp;
        this._rampStartIndex = this._sampler.sampleCount;
    },

    tune: function(timestamp, stage, lastFrameLength, didFinishInterval, intervalAverageFrameLength)
    {
        if (!this._rampDidWarmup)
            return;

        this._intervalFrameLengthEstimator.sample(lastFrameLength);
        if (!didFinishInterval)
            return;

        var currentComplexity = stage.complexity();
        var intervalFrameLengthMean = this._intervalFrameLengthEstimator.mean();
        var intervalFrameLengthStandardDeviation = this._intervalFrameLengthEstimator.standardDeviation();

        if (intervalFrameLengthMean < this.frameLengthDesiredThreshold && this._intervalFrameLengthEstimator.cdf(this.frameLengthDesiredThreshold) > .9) {
            this._possibleMinimumComplexity = Math.max(this._possibleMinimumComplexity, currentComplexity);
        } else if (intervalFrameLengthStandardDeviation > 2) {
            // In the case where we might have found a previous interval where the target fps was reached. We hit a significant blip,
            // so we should resample this area in the next ramp.
            this._possibleMinimumComplexity = 1;
        }
        if (intervalFrameLengthMean - intervalFrameLengthStandardDeviation > this.frameLengthRampLowerThreshold)
            this._possibleMaximumComplexity = Math.min(this._possibleMaximumComplexity, currentComplexity);
        this._intervalFrameLengthEstimator.reset();

        var progress = (timestamp - this._rampStartTimestamp) / this._currentRampLength;

        if (progress < 1) {
            // Reframe progress percentage so that the last interval of the ramp can sample at minimum complexity
            progress = (timestamp - this._rampStartTimestamp) / (this._currentRampLength - this.intervalSamplingLength);
            stage.tune(Math.max(this._minimumComplexity, Math.floor(Utilities.lerp(progress, this._maximumComplexity, this._minimumComplexity))) - currentComplexity);
            return;
        }

        var regression = new Regression(this._sampler.samples, this._getComplexity, this._getFrameLength,
            this._sampler.sampleCount - 1, this._rampStartIndex, { desiredFrameLength: this.frameLengthDesired });
        this._rampRegressions.push(regression);

        var frameLengthAtMaxComplexity = regression.valueAt(this._maximumComplexity);
        if (frameLengthAtMaxComplexity < this.frameLengthRampLowerThreshold)
            this._possibleMaximumComplexity = Math.floor(Utilities.lerp(Utilities.progressValue(this.frameLengthRampLowerThreshold, frameLengthAtMaxComplexity, this._lastTierFrameLength), this._maximumComplexity, this._lastTierComplexity));
        // If the regression doesn't fit the first segment at all, keep the minimum bound at 1
        if ((timestamp - this._sampler.samples[0][this._sampler.sampleCount - regression.n1]) / this._currentRampLength < .25)
            this._possibleMinimumComplexity = 1;

        this._minimumComplexityEstimator.sample(this._possibleMinimumComplexity);
        this._minimumComplexity = Math.round(this._minimumComplexityEstimator.mean());

        if (frameLengthAtMaxComplexity < this.frameLengthRampUpperThreshold) {
            this._changePointEstimator.sample(regression.complexity);
            // Ideally we'll target the change point in the middle of the ramp. If the range of the ramp is too small, there isn't enough
            // range along the complexity (x) axis for a good regression calculation to be made, so force at least a range of 5
            // particles. Make it possible to increase the maximum complexity in case unexpected noise caps the regression too low.
            this._maximumComplexity = Math.round(this._minimumComplexity +
                Math.max(5,
                    this._possibleMaximumComplexity - this._minimumComplexity,
                    (this._changePointEstimator.mean() - this._minimumComplexity) * 2));
        } else {
            // The slowest samples weighed the regression too heavily
            this._maximumComplexity = Math.max(Math.round(.8 * this._maximumComplexity), this._minimumComplexity + 5);
        }

        // Next ramp
        stage.tune(this._maximumComplexity - stage.complexity());
        this._rampDidWarmup = false;
        // Start timestamp represents start of ramp iteration and warm up
        this._rampStartTimestamp = timestamp;
        this._possibleMinimumComplexity = 1;
        this._possibleMaximumComplexity = this._maximumComplexity;
    },

    _getComplexity: function(samples, i) {
        return samples[1][i];
    },

    _getFrameLength: function(samples, i) {
        return samples[0][i] - samples[0][i - 1];
    },

    processSamples: function(results)
    {
        results[Strings.json.marks] = this._processMarks();
        // Have samplingTimeOffset represent time 0
        var startTimestamp = this._marks[Strings.json.samplingStartTimeOffset].time;
        for (var markName in results[Strings.json.marks]) {
            results[Strings.json.marks][markName].time -= startTimestamp;
        }

        results[Strings.json.samples] = {};

        var controllerSamples = this._processControllerSamples();
        results[Strings.json.samples][Strings.json.controller] = controllerSamples;
        controllerSamples.forEach(function(timeSample) {
            controllerSamples.setFieldInDatum(timeSample, Strings.json.time, controllerSamples.getFieldInDatum(timeSample, Strings.json.time) - startTimestamp);
        });

        // Aggregate all of the ramps into one big complexity-frameLength dataset
        var complexitySamples = new SampleData(controllerSamples.fieldMap);
        results[Strings.json.samples][Strings.json.complexity] = complexitySamples;

        results[Strings.json.controller] = [];
        this._rampRegressions.forEach(function(ramp) {
            var startIndex = ramp.startIndex, endIndex = ramp.endIndex;
            var startTime = controllerSamples.getFieldInDatum(startIndex, Strings.json.time);
            var endTime = controllerSamples.getFieldInDatum(endIndex, Strings.json.time);
            var startComplexity = controllerSamples.getFieldInDatum(startIndex, Strings.json.complexity);
            var endComplexity = controllerSamples.getFieldInDatum(endIndex, Strings.json.complexity);

            var regression = {};
            results[Strings.json.controller].push(regression);

            var percentage = (ramp.complexity - startComplexity) / (endComplexity - startComplexity);
            var inflectionTime = startTime + percentage * (endTime - startTime);

            regression[Strings.json.regressions.segment1] = [
                [startTime, ramp.s2 + ramp.t2 * startComplexity],
                [inflectionTime, ramp.s2 + ramp.t2 * ramp.complexity]
            ];
            regression[Strings.json.regressions.segment2] = [
                [inflectionTime, ramp.s1 + ramp.t1 * ramp.complexity],
                [endTime, ramp.s1 + ramp.t1 * endComplexity]
            ];
            regression[Strings.json.complexity] = ramp.complexity;
            regression[Strings.json.regressions.startIndex] = startIndex;
            regression[Strings.json.regressions.endIndex] = endIndex;
            regression[Strings.json.regressions.profile] = ramp.profile;

            for (var j = startIndex; j <= endIndex; ++j)
                complexitySamples.push(controllerSamples.at(j));
        });

        this._processComplexitySamples(complexitySamples);
    }
});

Stage = Utilities.createClass(
    function()
    {
    }, {

    initialize: function(benchmark)
    {
        this._benchmark = benchmark;
        this._element = document.getElementById("stage");
        this._element.setAttribute("width", document.body.offsetWidth);
        this._element.setAttribute("height", document.body.offsetHeight);
        this._size = Point.elementClientSize(this._element).subtract(Insets.elementPadding(this._element).size);
    },

    get element()
    {
        return this._element;
    },

    get size()
    {
        return this._size;
    },

    complexity: function()
    {
        return 0;
    },

    tune: function()
    {
        throw "Not implemented";
    },

    animate: function()
    {
        throw "Not implemented";
    },

    clear: function()
    {
        return this.tune(-this.tune(0));
    }
});

Utilities.extendObject(Stage, {
    random: function(min, max)
    {
        return (Pseudo.random() * (max - min)) + min;
    },

    randomBool: function()
    {
        return !!Math.round(Pseudo.random());
    },

    randomSign: function()
    {
        return Pseudo.random() >= .5 ? 1 : -1;
    },

    randomInt: function(min, max)
    {
        return Math.floor(this.random(min, max + 1));
    },

    randomPosition: function(maxPosition)
    {
        return new Point(this.randomInt(0, maxPosition.x), this.randomInt(0, maxPosition.y));
    },

    randomSquareSize: function(min, max)
    {
        var side = this.random(min, max);
        return new Point(side, side);
    },

    randomVelocity: function(maxVelocity)
    {
        return this.random(maxVelocity / 8, maxVelocity);
    },

    randomAngle: function()
    {
        return this.random(0, Math.PI * 2);
    },

    randomColor: function()
    {
        var min = 32;
        var max = 256 - 32;
        return "#"
            + this.randomInt(min, max).toString(16)
            + this.randomInt(min, max).toString(16)
            + this.randomInt(min, max).toString(16);
    },

    randomStyleMixBlendMode: function()
    {
        var mixBlendModeList = [
          'normal',
          'multiply',
          'screen',
          'overlay',
          'darken',
          'lighten',
          'color-dodge',
          'color-burn',
          'hard-light',
          'soft-light',
          'difference',
          'exclusion',
          'hue',
          'saturation',
          'color',
          'luminosity'
        ];

        return mixBlendModeList[this.randomInt(0, mixBlendModeList.length)];
    },

    randomStyleFilter: function()
    {
        var filterList = [
            'grayscale(50%)',
            'sepia(50%)',
            'saturate(50%)',
            'hue-rotate(180)',
            'invert(50%)',
            'opacity(50%)',
            'brightness(50%)',
            'contrast(50%)',
            'blur(10px)',
            'drop-shadow(10px 10px 10px gray)'
        ];

        return filterList[this.randomInt(0, filterList.length)];
    },

    randomElementInArray: function(array)
    {
        return array[Stage.randomInt(0, array.length - 1)];
    },

    rotatingColor: function(cycleLengthMs, saturation, lightness)
    {
        return "hsl("
            + Stage.dateFractionalValue(cycleLengthMs) * 360 + ", "
            + ((saturation || .8) * 100).toFixed(0) + "%, "
            + ((lightness || .35) * 100).toFixed(0) + "%)";
    },

    // Returns a fractional value that wraps around within [0,1]
    dateFractionalValue: function(cycleLengthMs)
    {
        return (Date.now() / (cycleLengthMs || 2000)) % 1;
    },

    // Returns an increasing value slowed down by factor
    dateCounterValue: function(factor)
    {
        return Date.now() / factor;
    },

    randomRotater: function()
    {
        return new Rotater(this.random(1000, 10000));
    }
});

Rotater = Utilities.createClass(
    function(rotateInterval)
    {
        this._timeDelta = 0;
        this._rotateInterval = rotateInterval;
        this._isSampling = false;
    }, {

    get interval()
    {
        return this._rotateInterval;
    },

    next: function(timeDelta)
    {
        this._timeDelta = (this._timeDelta + timeDelta) % this._rotateInterval;
    },

    degree: function()
    {
        return (360 * this._timeDelta) / this._rotateInterval;
    },

    rotateZ: function()
    {
        return "rotateZ(" + Math.floor(this.degree()) + "deg)";
    },

    rotate: function(center)
    {
        return "rotate(" + Math.floor(this.degree()) + ", " + center.x + "," + center.y + ")";
    }
});

Benchmark = Utilities.createClass(
    function(stage, options)
    {
        this._animateLoop = this._animateLoop.bind(this);
        this._warmupLength = options["warmup-length"];
        this._frameCount = 0;
        this._warmupFrameCount = options["warmup-frame-count"];
        this._firstFrameMinimumLength = options["first-frame-minimum-length"];

        this._stage = stage;
        this._stage.initialize(this, options);

        switch (options["time-measurement"])
        {
        case "performance":
            if (window.performance && window.performance.now)
                this._getTimestamp = performance.now.bind(performance);
            else
                this._getTimestamp = null;
            break;
        case "raf":
            this._getTimestamp = null;
            break;
        case "date":
            this._getTimestamp = Date.now;
            break;
        }

        options["test-interval"] *= 1000;
        switch (options["controller"])
        {
        case "fixed":
            this._controller = new FixedController(this, options);
            break;
        case "adaptive":
            this._controller = new AdaptiveController(this, options);
            break;
        case "ramp":
            this._controller = new RampController(this, options);
            break;
        }
    }, {

    get stage()
    {
        return this._stage;
    },

    get timestamp()
    {
        return this._currentTimestamp - this._benchmarkStartTimestamp;
    },

    backgroundColor: function()
    {
        var stage = window.getComputedStyle(document.getElementById("stage"));
        return stage["background-color"];
    },

    run: function()
    {
        return this.waitUntilReady().then(function() {
            this._finishPromise = new SimplePromise;
            this._previousTimestamp = undefined;
            this._didWarmUp = false;
            this._stage.tune(this._controller.initialComplexity - this._stage.complexity());
            this._animateLoop();
            return this._finishPromise;
        }.bind(this));
    },

    // Subclasses should override this if they have setup to do prior to commencing.
    waitUntilReady: function()
    {
        var promise = new SimplePromise;
        promise.resolve();
        return promise;
    },

    _animateLoop: function(timestamp)
    {
        timestamp = (this._getTimestamp && this._getTimestamp()) || timestamp;
        this._currentTimestamp = timestamp;

        if (this._controller.shouldStop(timestamp)) {
            this._finishPromise.resolve(this._controller.results());
            return;
        }

        if (!this._didWarmUp) {
            if (!this._previousTimestamp) {
                this._previousTimestamp = timestamp;
                this._benchmarkStartTimestamp = timestamp;
            } else if (timestamp - this._previousTimestamp >= this._warmupLength && this._frameCount >= this._warmupFrameCount) {
                this._didWarmUp = true;
                this._benchmarkStartTimestamp = timestamp;
                this._controller.start(timestamp, this._stage);
                this._previousTimestamp = timestamp;

                while (this._getTimestamp && this._getTimestamp() - timestamp < this._firstFrameMinimumLength) {
                }
            }

            this._stage.animate(0);
            ++this._frameCount;
            requestAnimationFrame(this._animateLoop);
            return;
        }

        this._controller.update(timestamp, this._stage);
        this._stage.animate(timestamp - this._previousTimestamp);
        this._previousTimestamp = timestamp;
        requestAnimationFrame(this._animateLoop);
    }
});
