if (!window.PerfTestRunner)
    console.log("measure-gc.js requires PerformanceTests/resources/runner.js to be loaded.");
if (!window.internals)
    console.log("measure-gc.js requires window.internals.");
if (!window.GCController && !window.gc)
    console.log("measure-gc.js requires GCController or exposed gc().");

(function (PerfTestRunner) {
    var warmUpCount = 3;
    var completedIterations = 0;

    PerfTestRunner.measureBlinkGCTime = function(test) {
        if (!test.unit)
            test.unit = 'ms';
        if (!test.warmUpCount)
            test.warmUpCount = warmUpCount;
        if (!test.run)
            test.run = function() {};

        completedIterations = 0;
        PerfTestRunner.startMeasureValuesAsync(test);

        // Force a V8 GC before running Blink GC test to avoid measuring marking from stale V8 wrappers.
        if (window.GCController)
            GCController.collectAll();
        else if (window.gc) {
            for (var i = 0; i < 7; i++)
                gc();
        }
        setTimeout(runTest, 0);
    }

    var NumberOfGCRunsPerForceBlinkGC = 5;

    function runTest() {
        if (completedIterations > warmUpCount)
            console.time("BlinkGCTimeMeasurement");

        // scheduleBlinkGC will schedule 5 Blink GCs at the end of event loop.
        // setTimeout function runs on next event loop, so assuming that event loop is not busy,
        // we can estimate GC time by measuring the delay of setTimeout function.
        internals.scheduleBlinkGC();
        var start = PerfTestRunner.now();
        setTimeout(function() {
            var end = PerfTestRunner.now();
            PerfTestRunner.measureValueAsync((end - start) / NumberOfGCRunsPerForceBlinkGC);
            setTimeout(runTest, 0);

            if (completedIterations > warmUpCount)
                console.timeEnd("BlinkGCTimeMeasurement");
            completedIterations++;
        }, 0);
    }
})(window.PerfTestRunner);
