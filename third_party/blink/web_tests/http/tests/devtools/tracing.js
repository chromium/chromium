// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test tracing\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.loadHTML(`
    <style>
    div#test {
        display: none;
        background-color: blue;
        width: 100px;
        height: 100px;
    }
    </style>
    <div id="test">
    </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function doWork()
      {
          var element = document.getElementById("test");
          element.style.display = "block";
          var unused = element.clientWidth;
      }
  `);

  var TracingManagerClient = function(callback) {
      this._completionCallback = callback;
      this._tracingModel = new SDK.TracingModel(new Bindings.TempFileBackingStorage("tracing"));
  }

  TracingManagerClient.prototype = {
      traceEventsCollected: function(events)
      {
          this._tracingModel.addEvents(events);
      },

      tracingComplete: function()
      {
          TestRunner.addResult("Tracing complete");
          this._tracingModel.tracingComplete();
          this._completionCallback(this._tracingModel);
      },

      tracingBufferUsage: function(usage) { },
      eventsRetrievalProgress: function(progress) { }
  };

  var tracingClient = new TracingManagerClient(runEventsSanityCheck);
  var tracingManager = TestRunner.tracingManager;
  tracingManager.start(tracingClient, '', '').then(() => {
    TestRunner.addResult('Tracing started');
    TestRunner.evaluateInPage('doWork()', tracingManager.stop.bind(tracingManager));
  });

  function runEventsSanityCheck(tracingModel) {
    var events = [];
    var phaseComplete = 0;
    var knownEvents = {};
    var processes = 0;
    var threads = 0;

    tracingModel.sortedProcesses().forEach(function(process) {
      processes++;
      process.sortedThreads().forEach(function(thread) {
        threads++;
        events = events.concat(thread.events());
      });
    });

    knownEvents['MessageLoop::PostTask'] = 0;
    knownEvents['v8.callFunction'] = 0;
    knownEvents['UpdateLayoutTree'] = 0;
    knownEvents['LocalFrameView::layout'] = 0;

    for (var i = 0; i < events.length; ++i) {
      var event = events[i];
      // X is the letter used to mark an event's phase as being Complete.
      if (event.phase === "X")
        ++phaseComplete;
      if (event.name in knownEvents)
        ++knownEvents[event.name];
    }
    TestRunner.assertGreaterOrEqual(events.length, 100, 'Too few trace events recorded');
    TestRunner.assertGreaterOrEqual(knownEvents['UpdateLayoutTree'], 1, 'Too few UpdateLayoutTree');
    TestRunner.assertGreaterOrEqual(knownEvents['LocalFrameView::layout'], 1, 'Too few LocalFrameView::layout');
    TestRunner.assertGreaterOrEqual(phaseComplete, 50, 'Too few begin events');
    TestRunner.assertGreaterOrEqual(processes, 2, 'Too few processes');
    TestRunner.assertGreaterOrEqual(threads, 4, 'Too few threads');
    TestRunner.addResult('Event sanity test done');
    TestRunner.completeTest();
  }
})();
