BenchmarkRunnerState = Utilities.createClass(
    function(suites)
    {
        this._suites = suites;
        this._suiteIndex = -1;
        this._testIndex = 0;
        this.next();
    }, {

    currentSuite: function()
    {
        return this._suites[this._suiteIndex];
    },

    currentTest: function()
    {
        var suite = this.currentSuite();
        return suite ? suite.tests[this._testIndex] : null;
    },

    isFirstTest: function()
    {
        return !this._testIndex;
    },

    next: function()
    {
        this._testIndex++;

        var suite = this._suites[this._suiteIndex];
        if (suite && this._testIndex < suite.tests.length)
            return;

        this._testIndex = 0;
        do {
            this._suiteIndex++;
        } while (this._suiteIndex < this._suites.length && this._suites[this._suiteIndex].disabled);
    },

    prepareCurrentTest: function(runner, frame)
    {
        var test = this.currentTest();
        var promise = new SimplePromise;

        frame.onload = function() {
            promise.resolve();
        };

        frame.src = "tests/" + test.url;
        return promise;
    }
});

BenchmarkRunner = Utilities.createClass(
    function(suites, frameContainer, client)
    {
        this._suites = suites;
        this._client = client;
        this._frameContainer = frameContainer;
    }, {

    _appendFrame: function()
    {
        var frame = document.createElement("iframe");
        frame.setAttribute("scrolling", "no");

        this._frameContainer.insertBefore(frame, this._frameContainer.firstChild);
        this._frame = frame;
        return frame;
    },

    _removeFrame: function()
    {
        if (this._frame) {
            this._frame.parentNode.removeChild(this._frame);
            this._frame = null;
        }
    },

    _runBenchmarkAndRecordResults: function(state)
    {
        var promise = new SimplePromise;
        var suite = state.currentSuite();
        var test = state.currentTest();

        if (this._client && this._client.willRunTest)
            this._client.willRunTest(suite, test);

        var contentWindow = this._frame.contentWindow;
        var self = this;

        var options = { complexity: test.complexity };
        Utilities.extendObject(options, this._client.options);
        Utilities.extendObject(options, contentWindow.Utilities.parseParameters());

        var benchmark = new contentWindow.benchmarkClass(options);
        document.body.style.backgroundColor = benchmark.backgroundColor();
        benchmark.run().then(function(testData) {
            var suiteResults = self._suitesResults[suite.name] || {};
            suiteResults[test.name] = testData;
            self._suitesResults[suite.name] = suiteResults;

            if (self._client && self._client.didRunTest)
                self._client.didRunTest(testData);

            state.next();
            if (state.currentSuite() != suite)
                self._removeFrame();
            promise.resolve(state);
        });

        return promise;
    },

    step: function(state)
    {
        if (!state) {
            state = new BenchmarkRunnerState(this._suites);
            this._suitesResults = {};
        }

        var suite = state.currentSuite();
        if (!suite) {
            this._finalize();
            var promise = new SimplePromise;
            promise.resolve();
            return promise;
        }

        if (state.isFirstTest()) {
            this._appendFrame();
        }

        return state.prepareCurrentTest(this, this._frame).then(function(prepareReturnValue) {
            return this._runBenchmarkAndRecordResults(state);
        }.bind(this));
    },

    runAllSteps: function(startingState)
    {
        var nextCallee = this.runAllSteps.bind(this);
        this.step(startingState).then(function(nextState) {
            if (nextState)
                nextCallee(nextState);
        });
    },

    runMultipleIterations: function()
    {
        var self = this;
        var currentIteration = 0;

        this._runNextIteration = function() {
            currentIteration++;
            if (currentIteration < self._client.iterationCount)
                self.runAllSteps();
            else if (this._client && this._client.didFinishLastIteration) {
                document.body.style.backgroundColor = "";
                self._client.didFinishLastIteration();
            }
        }

        if (this._client && this._client.willStartFirstIteration)
            this._client.willStartFirstIteration();

        this.runAllSteps();
    },

    _finalize: function()
    {
        this._removeFrame();

        if (this._client && this._client.didRunSuites)
            this._client.didRunSuites(this._suitesResults);

        if (this._runNextIteration)
            this._runNextIteration();
    }
});
