// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test tracing recording input events detail\n`);
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
  tracingManager.start(tracingClient, 'disabled-by-default-devtools.timeline.inputs', '').then(() => {
    TestRunner.addResult('Tracing started');
    sendMouseEventToPage('mousePressed', 100, 100).then(function() {
      sendMouseEventToPage('mouseMoved', 150, 50)
    }).then(function(){
      sendMouseEventToPage('mouseReleased', 150, 50)
    }).then(
      // Wait for roughly 2 frame for event to dispatch.
      setTimeout(tracingManager.stop.bind(tracingManager), 32)
    );
  });


  // Simulate a mouse click on point.
  function sendMouseEventToPage(type, x, y) {
    return TestRunner.InputAgent.invoke_dispatchMouseEvent({
        type: type,
        x: x,
        y: y,
        button: "left",
      });
  }

  function runEventsSanityCheck(tracingModel) {
    var events = [];
    var inputEventCount = 0;

    tracingModel.sortedProcesses().forEach(function(process) {
      process.sortedThreads().forEach(function(thread) {
        events = events.concat(thread.events());
      });
    });

    for (var i = 0; i < events.length; ++i) {
      var event = events[i];
      if (event.name === 'EventDispatch') {
        const mouseEventTypes = new Set(["mousedown", "mouseup", "mousemove"]);
        if (mouseEventTypes.has(event.args.data.type)) {
          ++inputEventCount;
          TestRunner.addResult(event.args.data.type + " at " + event.args.data.x + "," + event.args.data.y);
        }
      }
    }

    TestRunner.assertGreaterOrEqual(events.length, 100, 'Too few trace events recorded');
    TestRunner.assertGreaterOrEqual(inputEventCount, 3, 'Too few input events recorded');
    TestRunner.addResult('Event sanity test done');
    TestRunner.completeTest();
  }
})();
