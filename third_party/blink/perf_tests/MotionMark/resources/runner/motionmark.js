/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
 ResultsDashboard = Utilities.createClass(
    function(version, options, testData)
    {
        this._iterationsSamplers = [];
        this._options = options;
        this._results = null;
        this._version = version;
        this._targetFrameRate = options["frame-rate"] || 60;
        this._systemFrameRate = options["system-frame-rate"] || 60;
        if (testData) {
            this._iterationsSamplers = testData;
            this._processData();
        }
    }, {

    push: function(suitesSamplers)
    {
        this._iterationsSamplers.push(suitesSamplers);
    },

    _processData: function()
    {
        this._results = {};
        this._results[Strings.json.results.iterations] = [];

        var iterationsScores = [];
        this._iterationsSamplers.forEach(function(iteration, index) {
            var testsScores = [];
            var testsLowerBoundScores = [];
            var testsUpperBoundScores = [];

            var result = {};
            this._results[Strings.json.results.iterations][index] = result;

            var suitesResult = {};
            result[Strings.json.results.tests] = suitesResult;

            for (var suiteName in iteration) {
                var suiteData = iteration[suiteName];

                var suiteResult = {};
                suitesResult[suiteName] = suiteResult;

                for (var testName in suiteData) {
                    if (!suiteData[testName][Strings.json.result])
                        this.calculateScore(suiteData[testName]);

                    suiteResult[testName] = suiteData[testName][Strings.json.result];
                    delete suiteData[testName][Strings.json.result];

                    testsScores.push(suiteResult[testName][Strings.json.score]);
                    testsLowerBoundScores.push(suiteResult[testName][Strings.json.scoreLowerBound]);
                    testsUpperBoundScores.push(suiteResult[testName][Strings.json.scoreUpperBound]);
                }
            }

            result[Strings.json.score] = Statistics.geometricMean(testsScores);
            result[Strings.json.scoreLowerBound] = Statistics.geometricMean(testsLowerBoundScores);
            result[Strings.json.scoreUpperBound] = Statistics.geometricMean(testsUpperBoundScores);
            iterationsScores.push(result[Strings.json.score]);
        }, this);

        this._results[Strings.json.version] = this._version;
        this._results[Strings.json.fps] = this._targetFrameRate;
        this._results[Strings.json.score] = Statistics.sampleMean(iterationsScores.length, iterationsScores.reduce(function(a, b) { return a + b; }));
        this._results[Strings.json.scoreLowerBound] = this._results[Strings.json.results.iterations][0][Strings.json.scoreLowerBound];
        this._results[Strings.json.scoreUpperBound] = this._results[Strings.json.results.iterations][0][Strings.json.scoreUpperBound];
    },

    calculateScore: function(data)
    {
        var result = {};
        data[Strings.json.result] = result;
        var samples = data[Strings.json.samples];

        function findRegression(series, profile) {
            var minIndex = Math.round(.025 * series.length);
            var maxIndex = Math.round(.975 * (series.length - 1));
            var minComplexity = series.getFieldInDatum(minIndex, Strings.json.complexity);
            var maxComplexity = series.getFieldInDatum(maxIndex, Strings.json.complexity);

            if (Math.abs(maxComplexity - minComplexity) < 20 && maxIndex - minIndex < 20) {
                minIndex = 0;
                maxIndex = series.length - 1;
                minComplexity = series.getFieldInDatum(minIndex, Strings.json.complexity);
                maxComplexity = series.getFieldInDatum(maxIndex, Strings.json.complexity);
            }

            var complexityIndex = series.fieldMap[Strings.json.complexity];
            var frameLengthIndex = series.fieldMap[Strings.json.frameLength];
            var regressionOptions = { desiredFrameLength: 1000/this._targetFrameRate };
            if (profile)
                regressionOptions.preferredProfile = profile;
            return {
                minComplexity: minComplexity,
                maxComplexity: maxComplexity,
                samples: series.slice(minIndex, maxIndex + 1),
                regression: new Regression(
                    series.data,
                    function (data, i) { return data[i][complexityIndex]; },
                    function (data, i) { return data[i][frameLengthIndex]; },
                    minIndex, maxIndex, regressionOptions)
            };
        }

        // Convert these samples into SampleData objects if needed
        [Strings.json.complexity, Strings.json.controller].forEach(function(seriesName) {
            var series = samples[seriesName];
            if (series && !(series instanceof SampleData))
                samples[seriesName] = new SampleData(series.fieldMap, series.data);
        });

        var isRampController = this._options["controller"] == "ramp";
        var predominantProfile = "";
        if (isRampController) {
            var profiles = {};
            data[Strings.json.controller].forEach(function(regression) {
                if (regression[Strings.json.regressions.profile]) {
                    var profile = regression[Strings.json.regressions.profile];
                    profiles[profile] = (profiles[profile] || 0) + 1;
                }
            });

            var maxProfileCount = 0;
            for (var profile in profiles) {
                if (profiles[profile] > maxProfileCount) {
                    predominantProfile = profile;
                    maxProfileCount = profiles[profile];
                }
            }
        }

        var regressionResult = findRegression(samples[Strings.json.complexity], predominantProfile);
        var calculation = regressionResult.regression;
        result[Strings.json.complexity] = {};
        result[Strings.json.complexity][Strings.json.regressions.segment1] = [
            [regressionResult.minComplexity, calculation.s1 + calculation.t1 * regressionResult.minComplexity],
            [calculation.complexity, calculation.s1 + calculation.t1 * calculation.complexity]
        ];
        result[Strings.json.complexity][Strings.json.regressions.segment2] = [
            [calculation.complexity, calculation.s2 + calculation.t2 * calculation.complexity],
            [regressionResult.maxComplexity, calculation.s2 + calculation.t2 * regressionResult.maxComplexity]
        ];
        result[Strings.json.complexity][Strings.json.complexity] = calculation.complexity;
        result[Strings.json.complexity][Strings.json.measurements.stdev] = Math.sqrt(calculation.error / samples[Strings.json.complexity].length);

        result[Strings.json.fps] = data.targetFPS || 60;

        if (isRampController) {
            var timeComplexity = new Experiment;
            data[Strings.json.controller].forEach(function(regression) {
                timeComplexity.sample(regression[Strings.json.complexity]);
            });

            var experimentResult = {};
            result[Strings.json.controller] = experimentResult;
            experimentResult[Strings.json.score] = timeComplexity.mean();
            experimentResult[Strings.json.measurements.average] = timeComplexity.mean();
            experimentResult[Strings.json.measurements.stdev] = timeComplexity.standardDeviation();
            experimentResult[Strings.json.measurements.percent] = timeComplexity.percentage();

            const bootstrapIterations = 2500;
            var bootstrapResult = Regression.bootstrap(regressionResult.samples.data, bootstrapIterations, function(resampleData) {
                var complexityIndex = regressionResult.samples.fieldMap[Strings.json.complexity];
                resampleData.sort(function(a, b) {
                    return a[complexityIndex] - b[complexityIndex];
                });

                var resample = new SampleData(regressionResult.samples.fieldMap, resampleData);
                var bootstrapRegressionResult = findRegression(resample, predominantProfile);
                return bootstrapRegressionResult.regression.complexity;
            }, .8);

            result[Strings.json.complexity][Strings.json.bootstrap] = bootstrapResult;
            result[Strings.json.score] = bootstrapResult.median;
            result[Strings.json.scoreLowerBound] = bootstrapResult.confidenceLow;
            result[Strings.json.scoreUpperBound] = bootstrapResult.confidenceHigh;
        } else {
            var marks = data[Strings.json.marks];
            var samplingStartIndex = 0, samplingEndIndex = -1;
            if (Strings.json.samplingStartTimeOffset in marks)
                samplingStartIndex = marks[Strings.json.samplingStartTimeOffset].index;
            if (Strings.json.samplingEndTimeOffset in marks)
                samplingEndIndex = marks[Strings.json.samplingEndTimeOffset].index;

            var averageComplexity = new Experiment;
            var averageFrameLength = new Experiment;
            var controllerSamples = samples[Strings.json.controller];
            controllerSamples.forEach(function (sample, i) {
                if (i >= samplingStartIndex && (samplingEndIndex == -1 || i < samplingEndIndex)) {
                    averageComplexity.sample(controllerSamples.getFieldInDatum(sample, Strings.json.complexity));
                    var smoothedFrameLength = controllerSamples.getFieldInDatum(sample, Strings.json.smoothedFrameLength);
                    if (smoothedFrameLength && smoothedFrameLength != -1)
                        averageFrameLength.sample(smoothedFrameLength);
                }
            });

            var experimentResult = {};
            result[Strings.json.controller] = experimentResult;
            experimentResult[Strings.json.measurements.average] = averageComplexity.mean();
            experimentResult[Strings.json.measurements.concern] = averageComplexity.concern(Experiment.defaults.CONCERN);
            experimentResult[Strings.json.measurements.stdev] = averageComplexity.standardDeviation();
            experimentResult[Strings.json.measurements.percent] = averageComplexity.percentage();

            experimentResult = {};
            result[Strings.json.frameLength] = experimentResult;
            experimentResult[Strings.json.measurements.average] = 1000 / averageFrameLength.mean();
            experimentResult[Strings.json.measurements.concern] = averageFrameLength.concern(Experiment.defaults.CONCERN);
            experimentResult[Strings.json.measurements.stdev] = averageFrameLength.standardDeviation();
            experimentResult[Strings.json.measurements.percent] = averageFrameLength.percentage();

            result[Strings.json.score] = averageComplexity.score(Experiment.defaults.CONCERN);
            result[Strings.json.scoreLowerBound] = result[Strings.json.score] - averageFrameLength.standardDeviation();
            result[Strings.json.scoreUpperBound] = result[Strings.json.score] + averageFrameLength.standardDeviation();
        }
    },

    get data()
    {
        return this._iterationsSamplers;
    },

    get results()
    {
        if (this._results)
            return this._results[Strings.json.results.iterations];
        this._processData();
        return this._results[Strings.json.results.iterations];
    },

    get options()
    {
        return this._options;
    },

    get version()
    {
        return this._version;
    },

    _getResultsProperty: function(property)
    {
        if (this._results)
            return this._results[property];
        this._processData();
        return this._results[property];
    },

    get score()
    {
        return this._getResultsProperty(Strings.json.score);
    },

    get scoreLowerBound()
    {
        return this._getResultsProperty(Strings.json.scoreLowerBound);
    },

    get scoreUpperBound()
    {
        return this._getResultsProperty(Strings.json.scoreUpperBound);
    }
});

ResultsTable = Utilities.createClass(
    function(element, headers)
    {
        this.element = element;
        this._headers = headers;

        this._flattenedHeaders = [];
        this._headers.forEach(function(header) {
            if (header.disabled)
                return;

            if (header.children)
                this._flattenedHeaders = this._flattenedHeaders.concat(header.children);
            else
                this._flattenedHeaders.push(header);
        }, this);

        this._flattenedHeaders = this._flattenedHeaders.filter(function (header) {
            return !header.disabled;
        });

        this.clear();
    }, {

    clear: function()
    {
        this.element.textContent = "";
    },

    _addHeader: function()
    {
        var thead = Utilities.createElement("thead", {}, this.element);
        var row = Utilities.createElement("tr", {}, thead);

        this._headers.forEach(function (header) {
            if (header.disabled)
                return;

            var th = Utilities.createElement("th", {}, row);
            if (header.title != Strings.text.graph)
                th.innerHTML = header.title;
            if (header.children)
                th.colSpan = header.children.length;
        });
    },

    _addBody: function()
    {
        this.tbody = Utilities.createElement("tbody", {}, this.element);
    },

    _addEmptyRow: function()
    {
        var row = Utilities.createElement("tr", {}, this.tbody);
        this._flattenedHeaders.forEach(function (header) {
            return Utilities.createElement("td", { class: "suites-separator" }, row);
        });
    },

    _addTest: function(testName, testResult, options)
    {
        var row = Utilities.createElement("tr", {}, this.tbody);

        this._flattenedHeaders.forEach(function (header) {
            var td = Utilities.createElement("td", {}, row);
            if (header.text == Strings.text.testName) {
                td.textContent = testName;
            } else if (typeof header.text == "string") {
                var data = testResult[header.text];
                if (typeof data == "number")
                    data = data.toFixed(2);
                td.innerHTML = data;
            } else
                td.innerHTML = header.text(testResult);
        }, this);
    },

    _addIteration: function(iterationResult, iterationData, options)
    {
        var testsResults = iterationResult[Strings.json.results.tests];
        for (var suiteName in testsResults) {
            this._addEmptyRow();
            var suiteResult = testsResults[suiteName];
            var suiteData = iterationData[suiteName];
            for (var testName in suiteResult)
                this._addTest(testName, suiteResult[testName], options, suiteData[testName]);
        }
    },

    showIterations: function(dashboard)
    {
        this.clear();
        this._addHeader();
        this._addBody();

        var iterationsResults = dashboard.results;
        iterationsResults.forEach(function(iterationResult, index) {
            this._addIteration(iterationResult, dashboard.data[index], dashboard.options);
        }, this);
    }
});

window.benchmarkRunnerClient = {
    iterationCount: 1,
    options: null,
    results: null,

    initialize: function(suites, options)
    {
        this.options = options;
    },

    willStartFirstIteration: function()
    {
        this.results = new ResultsDashboard(Strings.version, this.options);
    },

    didRunSuites: function(suitesSamplers)
    {
        this.results.push(suitesSamplers);
    },

    didRunTest: function(testData)
    {
        this.results.calculateScore(testData);
    },

    didFinishLastIteration: function()
    {
        benchmarkController.showResults();
    }
};

window.sectionsManager =
{
    showSection: function(sectionIdentifier, pushState)
    {
        var sections = document.querySelectorAll("main > section");
        for (var i = 0; i < sections.length; ++i) {
            document.body.classList.remove("showing-" + sections[i].id);
        }
        document.body.classList.add("showing-" + sectionIdentifier);

        var currentSectionElement = document.querySelector("section.selected");
        console.assert(currentSectionElement);

        var newSectionElement = document.getElementById(sectionIdentifier);
        console.assert(newSectionElement);

        currentSectionElement.classList.remove("selected");
        newSectionElement.classList.add("selected");

        if (pushState)
            history.pushState({section: sectionIdentifier}, document.title);
    },

    setSectionVersion: function(sectionIdentifier, version)
    {
        document.querySelector("#" + sectionIdentifier + " .version").textContent = version;
    },

    setSectionScore: function(sectionIdentifier, score, confidence, fps)
    {
        if (fps && score)
            document.querySelector("#" + sectionIdentifier + " .score").textContent = `${score} @ ${fps}fps`;
        if (confidence)
            document.querySelector("#" + sectionIdentifier + " .confidence").textContent = confidence;
    },

    populateTable: function(tableIdentifier, headers, dashboard)
    {
        var table = new ResultsTable(document.getElementById(tableIdentifier), headers);
        table.showIterations(dashboard);
    }
};

window.benchmarkController = {
    benchmarkDefaultParameters: {
        "test-interval": 30,
        "display": "minimal",
        "tiles": "big",
        "controller": "ramp",
        "kalman-process-error": 1,
        "kalman-measurement-error": 4,
        "time-measurement": "performance",
        "warmup-length": 2000,
        "warmup-frame-count": 30,
        "first-frame-minimum-length": 0
    },

    initialize: function()
    {
        document.title = Strings.text.title.replace("%s", Strings.version);
        document.querySelectorAll(".version").forEach(function(e) {
            e.textContent = Strings.version;
        });
        benchmarkController.addOrientationListenerIfNecessary();
    },

    determineCanvasSize: function() {
        var match = window.matchMedia("(max-device-width: 760px)");
        if (match.matches) {
            document.body.classList.add("small");
            return;
        }

        match = window.matchMedia("(max-device-width: 1600px)");
        if (match.matches) {
            document.body.classList.add("medium");
            return;
        }

        match = window.matchMedia("(max-width: 1600px)");
        if (match.matches) {
            document.body.classList.add("medium");
            return;
        }

        document.body.classList.add("large");
    },

    addOrientationListenerIfNecessary: function() {
        if (!("orientation" in window))
            return;

        this.orientationQuery = window.matchMedia("(orientation: landscape)");
        this._orientationChanged(this.orientationQuery);
        this.orientationQuery.addListener(this._orientationChanged);
    },

    _orientationChanged: function(match)
    {
        benchmarkController.isInLandscapeOrientation = match.matches;
        if (match.matches)
            document.querySelector(".start-benchmark p").classList.add("hidden");
        else
            document.querySelector(".start-benchmark p").classList.remove("hidden");
        benchmarkController.updateStartButtonState();
    },

    updateStartButtonState: function()
    {
        document.getElementById("run-benchmark").disabled = !this.isInLandscapeOrientation;
    },

    _startBenchmark: function(suites, options, frameContainerID)
    {
        benchmarkController.determineCanvasSize();

        var configuration = document.body.className.match(/small|medium|large/);
        if (configuration)
            options[Strings.json.configuration] = configuration[0];

        benchmarkRunnerClient.initialize(suites, options);
        var frameContainer = document.getElementById(frameContainerID);
        var runner = new BenchmarkRunner(suites, frameContainer, benchmarkRunnerClient);
        runner.runMultipleIterations();

        sectionsManager.showSection("test-container");
    },

    startBenchmark: function()
    {
        var options = this.benchmarkDefaultParameters;
        this._startBenchmark(Suites, options, "test-container");
    },

    showResults: function()
    {
        if (!this.addedKeyEvent) {
            document.addEventListener("keypress", this.handleKeyPress, false);
            this.addedKeyEvent = true;
        }

        var dashboard = benchmarkRunnerClient.results;
        var score = dashboard.score;
        var confidence = "±" + (Statistics.largestDeviationPercentage(dashboard.scoreLowerBound, score, dashboard.scoreUpperBound) * 100).toFixed(2) + "%";
        var fps = dashboard._systemFrameRate;
        sectionsManager.setSectionVersion("results", dashboard.version);
        sectionsManager.setSectionScore("results", score.toFixed(2), confidence, fps);
        sectionsManager.populateTable("results-header", Headers.testName, dashboard);
        sectionsManager.populateTable("results-score", Headers.score, dashboard);
        sectionsManager.populateTable("results-data", Headers.details, dashboard);
        sectionsManager.showSection("results", true);
    },

    handleKeyPress: function(event)
    {
        switch (event.charCode)
        {
        case 27:  // esc
            benchmarkController.hideDebugInfo();
            break;
        case 106: // j
            benchmarkController.showDebugInfo();
            break;
        case 115: // s
            benchmarkController.selectResults(event.target);
            break;
        }
    },

    hideDebugInfo: function()
    {
        var overlay = document.getElementById("overlay");
        if (!overlay)
            return;
        document.body.removeChild(overlay);
    },

    showDebugInfo: function()
    {
        if (document.getElementById("overlay"))
            return;

        var overlay = Utilities.createElement("div", {
            id: "overlay"
        }, document.body);
        var container = Utilities.createElement("div", {}, overlay);

        var header = Utilities.createElement("h3", {}, container);
        header.textContent = "Debug Output";

        var data = Utilities.createElement("div", {}, container);
        data.textContent = "Please wait...";
        setTimeout(function() {
            var output = {
                version: benchmarkRunnerClient.results.version,
                options: benchmarkRunnerClient.results.options,
                data: benchmarkRunnerClient.results.data
            };
            data.textContent = JSON.stringify(output, function(key, value) {
                if (typeof value === 'number')
                    return Utilities.toFixedNumber(value, 3);
                return value;
            }, 1);
        }, 0);
        data.onclick = function() {
            var selection = window.getSelection();
            selection.removeAllRanges();
            var range = document.createRange();
            range.selectNode(data);
            selection.addRange(range);
        };

        var button = Utilities.createElement("button", {}, container);
        button.textContent = "Done";
        button.onclick = function() {
            benchmarkController.hideDebugInfo();
        };
    },

    selectResults: function(target)
    {
        target.selectRange = ((target.selectRange || 0) + 1) % 3;

        var selection = window.getSelection();
        selection.removeAllRanges();
        var range = document.createRange();
        switch (target.selectRange) {
            case 0: {
                range.selectNode(document.getElementById("results-score"));
                break;
            }
            case 1: {
                range.setStart(document.querySelector("#results .score"), 0);
                range.setEndAfter(document.querySelector("#results-score"), 0);
                break;
            }
            case 2: {
                range.selectNodeContents(document.querySelector("#results .score"));
                break;
            }
        }
        selection.addRange(range);
    }
};

window.addEventListener("load", function() { benchmarkController.initialize(); });

