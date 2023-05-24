// This file can be customized to report results as needed.

(function () {
    if (!window.testRunner && location.search != '?webkit' && location.hash != '#webkit')
        return;

    if (window.testRunner)
        testRunner.waitUntilDone();

    var scriptElement = document.createElement('script');
    scriptElement.src = '../resources/runner.js';
    document.head.appendChild(scriptElement);

    var styleElement = document.createElement('style');
    styleElement.textContent = 'pre { padding-top: 600px; }';
    document.head.appendChild(styleElement);

    var createTest;
    var valuesByIteration = new Array;

    window.onload = function () {
        document.body.removeChild(document.querySelector('main'));
        startBenchmark();
    }

    window.benchmarkClient = {
        iterationCount: 5, // Use 4 different instances of DRT/WTR to run 5 iterations.
        willStartFirstIteration: function (iterationCount) {
            createTest = function (name, aggregator, isLastTest, unit = 'ms') {
                return {
                    customIterationCount: iterationCount,
                    doNotIgnoreInitialRun: true,
                    doNotMeasureMemoryUsage: true,
                    continueTesting: !isLastTest,
                    unit: unit,
                    name: name,
                    aggregator: aggregator};
            }
            PerfTestRunner.prepareToMeasureValuesAsync(createTest(null, 'Geometric'));
        },
        didRunSuites: function (measuredValues) {
            PerfTestRunner.measureValueAsync(measuredValues.geomean);
            valuesByIteration.push(measuredValues);
        },
        didFinishLastIteration: function () {
            document.head.removeChild(document.querySelector('style'));

            var measuredValuesByFullName = {};
            function addToMeasuredValue(value, fullName, aggregator) {
                var values = measuredValuesByFullName[fullName] || new Array;
                measuredValuesByFullName[fullName] = values;
                values.push(value);
                values.aggregator = aggregator;
            }

            var scores = [];
            valuesByIteration.forEach(function (measuredValues) {
                scores.push(measuredValues.score);
                for (var suiteName in measuredValues.tests) {
                    var suite = measuredValues.tests[suiteName];
                    for (var testName in suite.tests) {
                        var test = suite.tests[testName];
                        for (var subtestName in test.tests)
                            addToMeasuredValue(test.tests[subtestName], suiteName + '/' + testName + '/' + subtestName);
                        addToMeasuredValue(test.total, suiteName + '/' + testName, 'Total');
                    }
                    addToMeasuredValue(suite.total, suiteName, 'Total');
                }
            });

            PerfTestRunner.reportValues(createTest(null, null, false, 'pt'), scores);

            var fullNames = new Array;
            for (var fullName in measuredValuesByFullName)
                fullNames.push(fullName);

            for (var i = 0; i < fullNames.length; i++) {
                var values = measuredValuesByFullName[fullNames[i]];
                PerfTestRunner.reportValues(createTest(fullNames[i], values.aggregator, i + 1 == fullNames.length), values);
            }
        }
    };
})();
