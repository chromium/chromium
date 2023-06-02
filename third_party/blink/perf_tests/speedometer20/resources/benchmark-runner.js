// FIXME: Use the real promise if available.
// FIXME: Make sure this interface is compatible with the real Promise.
function SimplePromise() {
    this._chainedPromise = null;
    this._callback = null;
}

SimplePromise.prototype.then = function (callback) {
    if (this._callback)
        throw "SimplePromise doesn't support multiple calls to then";
    this._callback = callback;
    this._chainedPromise = new SimplePromise;

    if (this._resolved)
        this.resolve(this._resolvedValue);

    return this._chainedPromise;
}

SimplePromise.prototype.resolve = function (value) {
    if (!this._callback) {
        this._resolved = true;
        this._resolvedValue = value;
        return;
    }

    var result = this._callback(value);
    if (result instanceof SimplePromise) {
        var chainedPromise = this._chainedPromise;
        result.then(function (result) { chainedPromise.resolve(result); });
    } else
        this._chainedPromise.resolve(result);
}

function BenchmarkTestStep(testName, testFunction) {
    this.name = testName;
    this.run = testFunction;
}

function BenchmarkRunner(suites, client) {
    this._suites = suites;
    this._prepareReturnValue = null;
    this._client = client;
}

BenchmarkRunner.prototype.waitForElement = function (selector) {
    var promise = new SimplePromise;
    var contentDocument = this._frame.contentDocument;

    function resolveIfReady() {
        var element = contentDocument.querySelector(selector);
        if (element)
            return promise.resolve(element);
        setTimeout(resolveIfReady, 50);
    }

    resolveIfReady();
    return promise;
}

BenchmarkRunner.prototype._removeFrame = function () {
    if (this._frame) {
        this._frame.parentNode.removeChild(this._frame);
        this._frame = null;
    }
}

BenchmarkRunner.prototype._appendFrame = function (src) {
    var frame = document.createElement('iframe');
    frame.style.width = '800px';
    frame.style.height = '600px';
    frame.style.border = '0px none';
    frame.style.position = 'absolute';
    frame.setAttribute('scrolling', 'no');

    var marginLeft = parseInt(getComputedStyle(document.body).marginLeft);
    var marginTop = parseInt(getComputedStyle(document.body).marginTop);
    if (window.innerWidth > 800 + marginLeft && window.innerHeight > 600 + marginTop) {
        frame.style.left = marginLeft + 'px';
        frame.style.top = marginTop + 'px';
    } else {
        frame.style.left = '0px';
        frame.style.top = '0px';
    }

    if (this._client && this._client.willAddTestFrame)
        this._client.willAddTestFrame(frame);

    document.body.insertBefore(frame, document.body.firstChild);
    this._frame = frame;
    return frame;
}

BenchmarkRunner.prototype._waitAndWarmUp = function () {
    var startTime = Date.now();

    function Fibonacci(n) {
        if (Date.now() - startTime > 100)
            return;
        if (n <= 0)
            return 0;
        else if (n == 1)
            return 1;
        return Fibonacci(n - 2) + Fibonacci(n - 1);
    }

    var promise = new SimplePromise;
    setTimeout(function () {
        Fibonacci(100);
        promise.resolve();
    }, 200);
    return promise;
}

BenchmarkRunner.prototype._writeMark = function(name) {
    if (window.performance && window.performance.mark)
        window.performance.mark(name);
}

// This function ought be as simple as possible. Don't even use SimplePromise.
BenchmarkRunner.prototype._runTest = function(suite, test, prepareReturnValue, callback)
{
    var self = this;
    var now = window.performance && window.performance.now ? function () { return window.performance.now(); } : Date.now;

    var contentWindow = self._frame.contentWindow;
    var contentDocument = self._frame.contentDocument;

    self._writeMark(suite.name + '.' + test.name + '-start');
    var startTime = now();
    test.run(prepareReturnValue, contentWindow, contentDocument);
    var endTime = now();
    self._writeMark(suite.name + '.' + test.name + '-sync-end');

    var syncTime = endTime - startTime;

    var startTime = now();
    setTimeout(function () {
        // Some browsers don't immediately update the layout for paint.
        // Force the layout here to ensure we're measuring the layout time.
        var height = self._frame.contentDocument.body.getBoundingClientRect().height;
        var endTime = now();
        self._frame.contentWindow._unusedHeightValue = height; // Prevent dead code elimination.
        self._writeMark(suite.name + '.' + test.name + '-async-end');
        callback(syncTime, endTime - startTime, height);
    }, 0);
}

function BenchmarkState(suites) {
    this._suites = suites;
    this._suiteIndex = -1;
    this._testIndex = 0;
    this.next();
}

BenchmarkState.prototype.currentSuite = function() {
    return this._suites[this._suiteIndex];
}

BenchmarkState.prototype.currentTest = function () {
    var suite = this.currentSuite();
    return suite ? suite.tests[this._testIndex] : null;
}

BenchmarkState.prototype.next = function () {
    this._testIndex++;

    var suite = this._suites[this._suiteIndex];
    if (suite && this._testIndex < suite.tests.length)
        return this;

    this._testIndex = 0;
    do {
        this._suiteIndex++;
    } while (this._suiteIndex < this._suites.length && this._suites[this._suiteIndex].disabled);

    return this;
}

BenchmarkState.prototype.isFirstTest = function () {
    return !this._testIndex;
}

BenchmarkState.prototype.prepareCurrentSuite = function (runner, frame) {
    var suite = this.currentSuite();
    var promise = new SimplePromise;
    frame.onload = function () {
        suite.prepare(runner, frame.contentWindow, frame.contentDocument).then(function (result) { promise.resolve(result); });
    }
    frame.src = 'resources/' + suite.url;
    return promise;
}

BenchmarkRunner.prototype.step = function (state) {
    if (!state) {
        state = new BenchmarkState(this._suites);
        this._measuredValues = {tests: {}, total: 0, mean: NaN, geomean: NaN, score: NaN};
    }

    var suite = state.currentSuite();
    if (!suite) {
        this._finalize();
        var promise = new SimplePromise;
        promise.resolve();
        return promise;
    }

    if (state.isFirstTest()) {
        this._removeFrame();
        var self = this;
        return state.prepareCurrentSuite(this, this._appendFrame()).then(function (prepareReturnValue) {
            self._prepareReturnValue = prepareReturnValue;
            return self._runTestAndRecordResults(state);
        });
    }

    return this._runTestAndRecordResults(state);
}

BenchmarkRunner.prototype.runAllSteps = function (startingState) {
    var nextCallee = this.runAllSteps.bind(this);
    this.step(startingState).then(function (nextState) {
        if (nextState)
            nextCallee(nextState);
    });
}

BenchmarkRunner.prototype.runMultipleIterations = function (iterationCount) {
    var self = this;
    var currentIteration = 0;

    this._runNextIteration = function () {
        currentIteration++;
        if (currentIteration < iterationCount)
            self.runAllSteps();
        else if (this._client && this._client.didFinishLastIteration)
            this._client.didFinishLastIteration();
    }

    if (this._client && this._client.willStartFirstIteration)
        this._client.willStartFirstIteration(iterationCount);

    self.runAllSteps();
}

BenchmarkRunner.prototype._runTestAndRecordResults = function (state) {
    var promise = new SimplePromise;
    var suite = state.currentSuite();
    var test = state.currentTest();

    if (this._client && this._client.willRunTest)
        this._client.willRunTest(suite, test);

    var self = this;
    setTimeout(function () {
        self._runTest(suite, test, self._prepareReturnValue, function (syncTime, asyncTime) {
            var suiteResults = self._measuredValues.tests[suite.name] || {tests:{}, total: 0};
            var total = syncTime + asyncTime;
            self._measuredValues.tests[suite.name] = suiteResults;
            suiteResults.tests[test.name] = {tests: {'Sync': syncTime, 'Async': asyncTime}, total: total};
            suiteResults.total += total;

            if (self._client && self._client.didRunTest)
                self._client.didRunTest(suite, test);

            state.next();
            promise.resolve(state);
        });
    }, 0);
    return promise;
}

BenchmarkRunner.prototype._finalize = function () {
    this._removeFrame();

    if (this._client && this._client.didRunSuites) {
        var product = 1;
        var values = [];
        for (var suiteName in this._measuredValues.tests) {
            var suiteTotal = this._measuredValues.tests[suiteName].total;
            product *= suiteTotal;
            values.push(suiteTotal);
        }

        values.sort(function (a, b) { return a - b }); // Avoid the loss of significance for the sum.
        var total = values.reduce(function (a, b) { return a + b });
        var geomean = Math.pow(product, 1 / values.length);

        var correctionFactor = 3; // This factor makes the test score look reasonably fit within 0 to 140.
        this._measuredValues.total = total;
        this._measuredValues.mean = total / values.length;
        this._measuredValues.geomean = geomean;
        this._measuredValues.score = 60 * 1000 / geomean / correctionFactor;
        this._client.didRunSuites(this._measuredValues);
    }

    if (this._runNextIteration)
        this._runNextIteration();
}
