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
            createTest = function (name, aggregator, isLastTest) {
                return {
                    customIterationCount: iterationCount,
                    doNotIgnoreInitialRun: true,
                    doNotMeasureMemoryUsage: true,
                    continueTesting: !isLastTest,
                    unit: 'ms',
                    name: name,
                    aggregator: aggregator};
            }
            PerfTestRunner.prepareToMeasureValuesAsync(createTest(null, 'Total'));
        },
        didRunSuites: function (measuredValues) {
            PerfTestRunner.measureValueAsync(measuredValues.total);
            valuesByIteration.push(measuredValues.tests);
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

            valuesByIteration.forEach(function (measuredValues) {
                for (var suiteName in measuredValues) {
                    var suite = measuredValues[suiteName];
                    for (var testName in suite.tests) {
                        var test = suite.tests[testName];
                        for (var subtestName in test.tests)
                            addToMeasuredValue(test.tests[subtestName], suiteName + '/' + testName + '/' + subtestName);
                        addToMeasuredValue(test.total, suiteName + '/' + testName, 'Total');
                    }
                    addToMeasuredValue(suite.total, suiteName, 'Total');
                }
            });

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
