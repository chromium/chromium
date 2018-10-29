/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* eslint-disable indent */

/**
 * @fileoverview This file contains small testing framework along with the
 * test suite for the frontend. These tests are a part of the continues build
 * and are executed by the devtools_sanity_unittest.cc as a part of the
 * Interactive UI Test suite.
 * FIXME: change field naming style to use trailing underscore.
 */

(function createTestSuite(window) {

  /**
   * @unrestricted
   */
  const TestSuite = class {
    /**
     * Test suite for interactive UI tests.
     * @param {Object} domAutomationController DomAutomationController instance.
     */
    constructor(domAutomationController) {
      this.domAutomationController_ = domAutomationController;
      this.controlTaken_ = false;
      this.timerId_ = -1;
      this._asyncInvocationId = 0;
    }

    /**
     * Key event with given key identifier.
     */
    static createKeyEvent(key) {
      return new KeyboardEvent('keydown', {bubbles: true, cancelable: true, key: key});
    }
  };

  /**
   * Reports test failure.
   * @param {string} message Failure description.
   */
  TestSuite.prototype.fail = function(message) {
    if (this.controlTaken_)
      this.reportFailure_(message);
    else
      throw message;
  };

  /**
   * Equals assertion tests that expected === actual.
   * @param {!Object|boolean} expected Expected object.
   * @param {!Object|boolean} actual Actual object.
   * @param {string} opt_message User message to print if the test fails.
   */
  TestSuite.prototype.assertEquals = function(expected, actual, opt_message) {
    if (expected !== actual) {
      let message = 'Expected: \'' + expected + '\', but was \'' + actual + '\'';
      if (opt_message)
        message = opt_message + '(' + message + ')';
      this.fail(message);
    }
  };

  /**
   * True assertion tests that value == true.
   * @param {!Object} value Actual object.
   * @param {string} opt_message User message to print if the test fails.
   */
  TestSuite.prototype.assertTrue = function(value, opt_message) {
    this.assertEquals(true, !!value, opt_message);
  };

  /**
   * Takes control over execution.
   */
  TestSuite.prototype.takeControl = function() {
    this.controlTaken_ = true;
    // Set up guard timer.
    const self = this;
    this.timerId_ = setTimeout(function() {
      self.reportFailure_('Timeout exceeded: 20 sec');
    }, 20000);
  };

  /**
   * Releases control over execution.
   */
  TestSuite.prototype.releaseControl = function() {
    if (this.timerId_ !== -1) {
      clearTimeout(this.timerId_);
      this.timerId_ = -1;
    }
    this.controlTaken_ = false;
    this.reportOk_();
  };

  /**
   * Async tests use this one to report that they are completed.
   */
  TestSuite.prototype.reportOk_ = function() {
    this.domAutomationController_.send('[OK]');
  };

  /**
   * Async tests use this one to report failures.
   */
  TestSuite.prototype.reportFailure_ = function(error) {
    if (this.timerId_ !== -1) {
      clearTimeout(this.timerId_);
      this.timerId_ = -1;
    }
    this.domAutomationController_.send('[FAILED] ' + error);
  };

  /**
   * Run specified test on a fresh instance of the test suite.
   * @param {Array<string>} args method name followed by its parameters.
   */
  TestSuite.prototype.dispatchOnTestSuite = function(args) {
    const methodName = args.shift();
    try {
      this[methodName].apply(this, args);
      if (!this.controlTaken_)
        this.reportOk_();
    } catch (e) {
      this.reportFailure_(e);
    }
  };

  /**
   * Wrap an async method with TestSuite.{takeControl(), releaseControl()}
   * and invoke TestSuite.reportOk_ upon completion.
   * @param {Array<string>} args method name followed by its parameters.
   */
  TestSuite.prototype.waitForAsync = function(var_args) {
    const args = Array.prototype.slice.call(arguments);
    this.takeControl();
    args.push(this.releaseControl.bind(this));
    this.dispatchOnTestSuite(args);
  };

  /**
   * Overrides the method with specified name until it's called first time.
   * @param {!Object} receiver An object whose method to override.
   * @param {string} methodName Name of the method to override.
   * @param {!Function} override A function that should be called right after the
   *     overridden method returns.
   * @param {?boolean} opt_sticky Whether restore original method after first run
   *     or not.
   */
  TestSuite.prototype.addSniffer = function(receiver, methodName, override, opt_sticky) {
    const orig = receiver[methodName];
    if (typeof orig !== 'function')
      this.fail('Cannot find method to override: ' + methodName);
    const test = this;
    receiver[methodName] = function(var_args) {
      let result;
      try {
        result = orig.apply(this, arguments);
      } finally {
        if (!opt_sticky)
          receiver[methodName] = orig;
      }
      // In case of exception the override won't be called.
      try {
        override.apply(this, arguments);
      } catch (e) {
        test.fail('Exception in overriden method \'' + methodName + '\': ' + e);
      }
      return result;
    };
  };

  /**
   * Waits for current throttler invocations, if any.
   * @param {!Common.Throttler} throttler
   * @param {function()} callback
   */
  TestSuite.prototype.waitForThrottler = function(throttler, callback) {
    const test = this;
    let scheduleShouldFail = true;
    test.addSniffer(throttler, 'schedule', onSchedule);

    function hasSomethingScheduled() {
      return throttler._isRunningProcess || throttler._process;
    }

    function checkState() {
      if (!hasSomethingScheduled()) {
        scheduleShouldFail = false;
        callback();
        return;
      }

      test.addSniffer(throttler, '_processCompletedForTests', checkState);
    }

    function onSchedule() {
      if (scheduleShouldFail)
        test.fail('Unexpected Throttler.schedule');
    }

    checkState();
  };

  /**
   * @param {string} panelName Name of the panel to show.
   */
  TestSuite.prototype.showPanel = function(panelName) {
    return UI.inspectorView.showPanel(panelName);
  };

  // UI Tests

  /**
   * Tests that scripts tab can be open and populated with inspected scripts.
   */
  TestSuite.prototype.testShowScriptsTab = function() {
    const test = this;
    this.showPanel('sources').then(function() {
      // There should be at least main page script.
      this._waitUntilScriptsAreParsed(['debugger_test_page.html'], function() {
        test.releaseControl();
      });
    }.bind(this));
    // Wait until all scripts are added to the debugger.
    this.takeControl();
  };

  /**
   * Tests that scripts tab is populated with inspected scripts even if it
   * hadn't been shown by the moment inspected paged refreshed.
   * @see http://crbug.com/26312
   */
  TestSuite.prototype.testScriptsTabIsPopulatedOnInspectedPageRefresh = function() {
    const test = this;
    const debuggerModel = SDK.targetManager.mainTarget().model(SDK.DebuggerModel);
    debuggerModel.addEventListener(SDK.DebuggerModel.Events.GlobalObjectCleared, waitUntilScriptIsParsed);

    this.showPanel('elements').then(function() {
      // Reload inspected page. It will reset the debugger agent.
      test.evaluateInConsole_('window.location.reload(true);', function(resultText) {});
    });

    function waitUntilScriptIsParsed() {
      debuggerModel.removeEventListener(SDK.DebuggerModel.Events.GlobalObjectCleared, waitUntilScriptIsParsed);
      test.showPanel('sources').then(function() {
        test._waitUntilScriptsAreParsed(['debugger_test_page.html'], function() {
          test.releaseControl();
        });
      });
    }

    // Wait until all scripts are added to the debugger.
    this.takeControl();
  };

  /**
   * Tests that scripts list contains content scripts.
   */
  TestSuite.prototype.testContentScriptIsPresent = function() {
    const test = this;
    this.showPanel('sources').then(function() {
      test._waitUntilScriptsAreParsed(['page_with_content_script.html', 'simple_content_script.js'], function() {
        test.releaseControl();
      });
    });

    // Wait until all scripts are added to the debugger.
    this.takeControl();
  };

  /**
   * Tests that scripts are not duplicaed on Scripts tab switch.
   */
  TestSuite.prototype.testNoScriptDuplicatesOnPanelSwitch = function() {
    const test = this;

    function switchToElementsTab() {
      test.showPanel('elements').then(function() {
        setTimeout(switchToScriptsTab, 0);
      });
    }

    function switchToScriptsTab() {
      test.showPanel('sources').then(function() {
        setTimeout(checkScriptsPanel, 0);
      });
    }

    function checkScriptsPanel() {
      test.assertTrue(test._scriptsAreParsed(['debugger_test_page.html']), 'Some scripts are missing.');
      checkNoDuplicates();
      test.releaseControl();
    }

    function checkNoDuplicates() {
      const uiSourceCodes = test.nonAnonymousUISourceCodes_();
      for (let i = 0; i < uiSourceCodes.length; i++) {
        for (let j = i + 1; j < uiSourceCodes.length; j++) {
          test.assertTrue(
              uiSourceCodes[i].url() !== uiSourceCodes[j].url(),
              'Found script duplicates: ' + test.uiSourceCodesToString_(uiSourceCodes));
        }
      }
    }

    this.showPanel('sources').then(function() {
      test._waitUntilScriptsAreParsed(['debugger_test_page.html'], function() {
        checkNoDuplicates();
        setTimeout(switchToElementsTab, 0);
      });
    });

    // Wait until all scripts are added to the debugger.
    this.takeControl();
  };

  // Tests that debugger works correctly if pause event occurs when DevTools
  // frontend is being loaded.
  TestSuite.prototype.testPauseWhenLoadingDevTools = function() {
    const debuggerModel = SDK.targetManager.mainTarget().model(SDK.DebuggerModel);
    if (debuggerModel.debuggerPausedDetails)
      return;

    this.showPanel('sources').then(function() {
      // Script execution can already be paused.

      this._waitForScriptPause(this.releaseControl.bind(this));
    }.bind(this));

    this.takeControl();
  };

  // Tests that pressing "Pause" will pause script execution if the script
  // is already running.
  TestSuite.prototype.testPauseWhenScriptIsRunning = function() {
    this.showPanel('sources').then(function() {
      this.evaluateInConsole_('setTimeout("handleClick()", 0)', didEvaluateInConsole.bind(this));
    }.bind(this));

    function didEvaluateInConsole(resultText) {
      this.assertTrue(!isNaN(resultText), 'Failed to get timer id: ' + resultText);
      // Wait for some time to make sure that inspected page is running the
      // infinite loop.
      setTimeout(testScriptPause.bind(this), 300);
    }

    function testScriptPause() {
      // The script should be in infinite loop. Click "Pause" button to
      // pause it and wait for the result.
      UI.panels.sources._togglePause();

      this._waitForScriptPause(this.releaseControl.bind(this));
    }

    this.takeControl();
  };

  /**
   * Tests network size.
   */
  TestSuite.prototype.testNetworkSize = function() {
    const test = this;

    function finishRequest(request, finishTime) {
      test.assertEquals(25, request.resourceSize, 'Incorrect total data length');
      test.releaseControl();
    }

    this.addSniffer(SDK.NetworkDispatcher.prototype, '_finishNetworkRequest', finishRequest);

    // Reload inspected page to sniff network events
    test.evaluateInConsole_('window.location.reload(true);', function(resultText) {});

    this.takeControl();
  };

  /**
   * Tests network sync size.
   */
  TestSuite.prototype.testNetworkSyncSize = function() {
    const test = this;

    function finishRequest(request, finishTime) {
      test.assertEquals(25, request.resourceSize, 'Incorrect total data length');
      test.releaseControl();
    }

    this.addSniffer(SDK.NetworkDispatcher.prototype, '_finishNetworkRequest', finishRequest);

    // Send synchronous XHR to sniff network events
    test.evaluateInConsole_(
        'let xhr = new XMLHttpRequest(); xhr.open("GET", "chunked", false); xhr.send(null);', function() {});

    this.takeControl();
  };

  /**
   * Tests network raw headers text.
   */
  TestSuite.prototype.testNetworkRawHeadersText = function() {
    const test = this;

    function finishRequest(request, finishTime) {
      if (!request.responseHeadersText)
        test.fail('Failure: resource does not have response headers text');
      const index = request.responseHeadersText.indexOf('Date:');
      test.assertEquals(
          112, request.responseHeadersText.substring(index).length, 'Incorrect response headers text length');
      test.releaseControl();
    }

    this.addSniffer(SDK.NetworkDispatcher.prototype, '_finishNetworkRequest', finishRequest);

    // Reload inspected page to sniff network events
    test.evaluateInConsole_('window.location.reload(true);', function(resultText) {});

    this.takeControl();
  };

  /**
   * Tests network timing.
   */
  TestSuite.prototype.testNetworkTiming = function() {
    const test = this;

    function finishRequest(request, finishTime) {
      // Setting relaxed expectations to reduce flakiness.
      // Server sends headers after 100ms, then sends data during another 100ms.
      // We expect these times to be measured at least as 70ms.
      test.assertTrue(
          request.timing.receiveHeadersEnd - request.timing.connectStart >= 70,
          'Time between receiveHeadersEnd and connectStart should be >=70ms, but was ' +
              'receiveHeadersEnd=' + request.timing.receiveHeadersEnd + ', connectStart=' +
              request.timing.connectStart + '.');
      test.assertTrue(
          request.responseReceivedTime - request.startTime >= 0.07,
          'Time between responseReceivedTime and startTime should be >=0.07s, but was ' +
              'responseReceivedTime=' + request.responseReceivedTime + ', startTime=' + request.startTime + '.');
      test.assertTrue(
          request.endTime - request.startTime >= 0.14,
          'Time between endTime and startTime should be >=0.14s, but was ' +
              'endtime=' + request.endTime + ', startTime=' + request.startTime + '.');

      test.releaseControl();
    }

    this.addSniffer(SDK.NetworkDispatcher.prototype, '_finishNetworkRequest', finishRequest);

    // Reload inspected page to sniff network events
    test.evaluateInConsole_('window.location.reload(true);', function(resultText) {});

    this.takeControl();
  };

  TestSuite.prototype.testPushTimes = function(url) {
    const test = this;
    let pendingRequestCount = 2;

    function finishRequest(request, finishTime) {
      test.assertTrue(
          typeof request.timing.pushStart === 'number' && request.timing.pushStart > 0,
          `pushStart is invalid: ${request.timing.pushStart}`);
      test.assertTrue(typeof request.timing.pushEnd === 'number', `pushEnd is invalid: ${request.timing.pushEnd}`);
      test.assertTrue(request.timing.pushStart < request.startTime, 'pushStart should be before startTime');
      if (request.url().endsWith('?pushUseNullEndTime')) {
        test.assertTrue(request.timing.pushEnd === 0, `pushEnd should be 0 but is ${request.timing.pushEnd}`);
      } else {
        test.assertTrue(
            request.timing.pushStart < request.timing.pushEnd,
            `pushStart should be before pushEnd (${request.timing.pushStart} >= ${request.timing.pushEnd})`);
        // The below assertion is just due to the way we generate times in the moch URLRequestJob and is not generally an invariant.
        test.assertTrue(request.timing.pushEnd < request.endTime, 'pushEnd should be before endTime');
        test.assertTrue(request.startTime < request.timing.pushEnd, 'pushEnd should be after startTime');
      }
      if (!--pendingRequestCount)
        test.releaseControl();
    }

    this.addSniffer(SDK.NetworkDispatcher.prototype, '_finishNetworkRequest', finishRequest, true);

    test.evaluateInConsole_('addImage(\'' + url + '\')', function(resultText) {});
    test.evaluateInConsole_('addImage(\'' + url + '?pushUseNullEndTime\')', function(resultText) {});
    this.takeControl();
  };

  TestSuite.prototype.testConsoleOnNavigateBack = function() {

    function filteredMessages() {
      return SDK.consoleModel.messages().filter(a => a.source !== SDK.ConsoleMessage.MessageSource.Violation);
    }

    if (filteredMessages().length === 1)
      firstConsoleMessageReceived.call(this, null);
    else
      SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, firstConsoleMessageReceived, this);


    function firstConsoleMessageReceived(event) {
      if (event && event.data.source === SDK.ConsoleMessage.MessageSource.Violation)
        return;
      SDK.consoleModel.removeEventListener(SDK.ConsoleModel.Events.MessageAdded, firstConsoleMessageReceived, this);
      this.evaluateInConsole_('clickLink();', didClickLink.bind(this));
    }

    function didClickLink() {
      // Check that there are no new messages(command is not a message).
      this.assertEquals(3, filteredMessages().length);
      this.evaluateInConsole_('history.back();', didNavigateBack.bind(this));
    }

    function didNavigateBack() {
      // Make sure navigation completed and possible console messages were pushed.
      this.evaluateInConsole_('void 0;', didCompleteNavigation.bind(this));
    }

    function didCompleteNavigation() {
      this.assertEquals(7, filteredMessages().length);
      this.releaseControl();
    }

    this.takeControl();
  };

  TestSuite.prototype.testSharedWorker = function() {
    function didEvaluateInConsole(resultText) {
      this.assertEquals('2011', resultText);
      this.releaseControl();
    }
    this.evaluateInConsole_('globalVar', didEvaluateInConsole.bind(this));
    this.takeControl();
  };

  TestSuite.prototype.testPauseInSharedWorkerInitialization1 = function() {
    // Make sure the worker is loaded.
    this.takeControl();
    this._waitForTargets(2, callback.bind(this));

    function callback() {
      Protocol.InspectorBackend.deprecatedRunAfterPendingDispatches(this.releaseControl.bind(this));
    }
  };

  TestSuite.prototype.testPauseInSharedWorkerInitialization2 = function() {
    this.takeControl();
    this._waitForTargets(2, callback.bind(this));

    function callback() {
      const debuggerModel = SDK.targetManager.models(SDK.DebuggerModel)[0];
      if (debuggerModel.isPaused()) {
        SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, onConsoleMessage, this);
        debuggerModel.resume();
        return;
      }
      this._waitForScriptPause(callback.bind(this));
    }

    function onConsoleMessage(event) {
      const message = event.data.messageText;
      if (message !== 'connected')
        this.fail('Unexpected message: ' + message);
      this.releaseControl();
    }
  };

  TestSuite.prototype.enableTouchEmulation = function() {
    const deviceModeModel = new Emulation.DeviceModeModel(function() {});
    deviceModeModel._target = SDK.targetManager.mainTarget();
    deviceModeModel._applyTouch(true, true);
  };

  TestSuite.prototype.waitForDebuggerPaused = function() {
    const debuggerModel = SDK.targetManager.mainTarget().model(SDK.DebuggerModel);
    if (debuggerModel.debuggerPausedDetails)
      return;

    this.takeControl();
    this._waitForScriptPause(this.releaseControl.bind(this));
  };

  TestSuite.prototype.switchToPanel = function(panelName) {
    this.showPanel(panelName).then(this.releaseControl.bind(this));
    this.takeControl();
  };

  // Regression test for crbug.com/370035.
  TestSuite.prototype.testDeviceMetricsOverrides = function() {
    function dumpPageMetrics() {
      return JSON.stringify(
          {width: window.innerWidth, height: window.innerHeight, deviceScaleFactor: window.devicePixelRatio});
    }

    const test = this;

    async function testOverrides(params, metrics, callback) {
      await SDK.targetManager.mainTarget().emulationAgent().invoke_setDeviceMetricsOverride(params);
      test.evaluateInConsole_('(' + dumpPageMetrics.toString() + ')()', checkMetrics);

      function checkMetrics(consoleResult) {
        test.assertEquals(
            '"' + JSON.stringify(metrics) + '"', consoleResult, 'Wrong metrics for params: ' + JSON.stringify(params));
        callback();
      }
    }

    function step1() {
      testOverrides(
          {width: 1200, height: 1000, deviceScaleFactor: 1, mobile: false, fitWindow: true},
          {width: 1200, height: 1000, deviceScaleFactor: 1}, step2);
    }

    function step2() {
      testOverrides(
          {width: 1200, height: 1000, deviceScaleFactor: 1, mobile: false, fitWindow: false},
          {width: 1200, height: 1000, deviceScaleFactor: 1}, step3);
    }

    function step3() {
      testOverrides(
          {width: 1200, height: 1000, deviceScaleFactor: 3, mobile: false, fitWindow: true},
          {width: 1200, height: 1000, deviceScaleFactor: 3}, step4);
    }

    function step4() {
      testOverrides(
          {width: 1200, height: 1000, deviceScaleFactor: 3, mobile: false, fitWindow: false},
          {width: 1200, height: 1000, deviceScaleFactor: 3}, finish);
    }

    function finish() {
      test.releaseControl();
    }

    test.takeControl();
    step1();
  };

  TestSuite.prototype.testDispatchKeyEventShowsAutoFill = function() {
    const test = this;
    let receivedReady = false;

    function signalToShowAutofill() {
      SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
          {type: 'rawKeyDown', key: 'Down', windowsVirtualKeyCode: 40, nativeVirtualKeyCode: 40});
      SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
          {type: 'keyUp', key: 'Down', windowsVirtualKeyCode: 40, nativeVirtualKeyCode: 40});
    }

    function selectTopAutoFill() {
      SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
          {type: 'rawKeyDown', key: 'Down', windowsVirtualKeyCode: 40, nativeVirtualKeyCode: 40});
      SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
          {type: 'keyUp', key: 'Down', windowsVirtualKeyCode: 40, nativeVirtualKeyCode: 40});
      SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
          {type: 'rawKeyDown', key: 'Enter', windowsVirtualKeyCode: 13, nativeVirtualKeyCode: 13});
      SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
          {type: 'keyUp', key: 'Enter', windowsVirtualKeyCode: 13, nativeVirtualKeyCode: 13});

      test.evaluateInConsole_('document.getElementById("name").value', onResultOfInput);
    }

    function onResultOfInput(value) {
      // Console adds "" around the response.
      test.assertEquals('"Abbf"', value);
      test.releaseControl();
    }

    function onConsoleMessage(event) {
      const message = event.data.messageText;
      if (message === 'ready' && !receivedReady) {
        receivedReady = true;
        signalToShowAutofill();
      }
      // This log comes from the browser unittest code.
      if (message === 'didShowSuggestions')
        selectTopAutoFill();
    }

    this.takeControl();

    // It is possible for the ready console messagage to be already received but not handled
    // or received later. This ensures we can catch both cases.
    SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, onConsoleMessage, this);

    const messages = SDK.consoleModel.messages();
    if (messages.length) {
      const text = messages[0].messageText;
      this.assertEquals('ready', text);
      signalToShowAutofill();
    }
  };

  TestSuite.prototype.testDispatchKeyEventDoesNotCrash = function() {
    SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
        {type: 'rawKeyDown', windowsVirtualKeyCode: 0x23, key: 'End'});
    SDK.targetManager.mainTarget().inputAgent().invoke_dispatchKeyEvent(
        {type: 'keyUp', windowsVirtualKeyCode: 0x23, key: 'End'});
  };

  // Simple sanity check to make sure network throttling is wired up
  // See crbug.com/747724
  TestSuite.prototype.testOfflineNetworkConditions = async function() {
    const test = this;
    SDK.multitargetNetworkManager.setNetworkConditions(SDK.NetworkManager.OfflineConditions);

    function finishRequest(request) {
      test.assertEquals(
          'net::ERR_INTERNET_DISCONNECTED', request.localizedFailDescription, 'Request should have failed');
      test.releaseControl();
    }

    this.addSniffer(SDK.NetworkDispatcher.prototype, '_finishNetworkRequest', finishRequest);

    test.takeControl();
    test.evaluateInConsole_('window.location.reload(true);', function(resultText) {});
  };

  TestSuite.prototype.testEmulateNetworkConditions = function() {
    const test = this;

    function testPreset(preset, messages, next) {
      function onConsoleMessage(event) {
        const index = messages.indexOf(event.data.messageText);
        if (index === -1) {
          test.fail('Unexpected message: ' + event.data.messageText);
          return;
        }

        messages.splice(index, 1);
        if (!messages.length) {
          SDK.consoleModel.removeEventListener(SDK.ConsoleModel.Events.MessageAdded, onConsoleMessage, this);
          next();
        }
      }

      SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, onConsoleMessage, this);
      SDK.multitargetNetworkManager.setNetworkConditions(preset);
    }

    test.takeControl();
    step1();

    function step1() {
      testPreset(
          MobileThrottling.networkPresets[2],
          [
            'offline event: online = false', 'connection change event: type = none; downlinkMax = 0; effectiveType = 4g'
          ],
          step2);
    }

    function step2() {
      testPreset(
          MobileThrottling.networkPresets[1],
          [
            'online event: online = true',
            'connection change event: type = cellular; downlinkMax = 0.390625; effectiveType = 2g'
          ],
          step3);
    }

    function step3() {
      testPreset(
          MobileThrottling.networkPresets[0],
          ['connection change event: type = cellular; downlinkMax = 1.4400000000000002; effectiveType = 3g'],
          test.releaseControl.bind(test));
    }
  };

  TestSuite.prototype.testScreenshotRecording = function() {
    const test = this;

    function performActionsInPage(callback) {
      let count = 0;
      const div = document.createElement('div');
      div.setAttribute('style', 'left: 0px; top: 0px; width: 100px; height: 100px; position: absolute;');
      document.body.appendChild(div);
      requestAnimationFrame(frame);
      function frame() {
        const color = [0, 0, 0];
        color[count % 3] = 255;
        div.style.backgroundColor = 'rgb(' + color.join(',') + ')';
        if (++count > 10)
          requestAnimationFrame(callback);
        else
          requestAnimationFrame(frame);
      }
    }

    const captureFilmStripSetting = Common.settings.createSetting('timelineCaptureFilmStrip', false);
    captureFilmStripSetting.set(true);
    test.evaluateInConsole_(performActionsInPage.toString(), function() {});
    test.invokeAsyncWithTimeline_('performActionsInPage', onTimelineDone);

    function onTimelineDone() {
      captureFilmStripSetting.set(false);
      const filmStripModel = UI.panels.timeline._performanceModel.filmStripModel();
      const frames = filmStripModel.frames();
      test.assertTrue(frames.length > 4 && typeof frames.length === 'number');
      loadFrameImages(frames);
    }

    function loadFrameImages(frames) {
      const readyImages = [];
      for (const frame of frames)
        frame.imageDataPromise().then(onGotImageData);

      function onGotImageData(data) {
        const image = new Image();
        test.assertTrue(!!data, 'No image data for frame');
        image.addEventListener('load', onLoad);
        image.src = 'data:image/jpg;base64,' + data;
      }

      function onLoad(event) {
        readyImages.push(event.target);
        if (readyImages.length === frames.length)
          validateImagesAndCompleteTest(readyImages);
      }
    }

    function validateImagesAndCompleteTest(images) {
      let redCount = 0;
      let greenCount = 0;
      let blueCount = 0;

      const canvas = document.createElement('canvas');
      const ctx = canvas.getContext('2d');
      for (const image of images) {
        test.assertTrue(image.naturalWidth > 10);
        test.assertTrue(image.naturalHeight > 10);
        canvas.width = image.naturalWidth;
        canvas.height = image.naturalHeight;
        ctx.drawImage(image, 0, 0);
        const data = ctx.getImageData(0, 0, 1, 1);
        const color = Array.prototype.join.call(data.data, ',');
        if (data.data[0] > 200)
          redCount++;
        else if (data.data[1] > 200)
          greenCount++;
        else if (data.data[2] > 200)
          blueCount++;
        else
          test.fail('Unexpected color: ' + color);
      }
      test.assertTrue(redCount && greenCount && blueCount, 'Color sanity check failed');
      test.releaseControl();
    }

    test.takeControl();
  };

  TestSuite.prototype.testSettings = function() {
    const test = this;

    createSettings();
    test.takeControl();
    setTimeout(reset, 0);

    function createSettings() {
      const localSetting = Common.settings.createLocalSetting('local', undefined);
      localSetting.set({s: 'local', n: 1});
      const globalSetting = Common.settings.createSetting('global', undefined);
      globalSetting.set({s: 'global', n: 2});
    }

    function reset() {
      Runtime.experiments.clearForTest();
      InspectorFrontendHost.getPreferences(gotPreferences);
    }

    function gotPreferences(prefs) {
      Main.Main._instanceForTest._createSettings(prefs);

      const localSetting = Common.settings.createLocalSetting('local', undefined);
      test.assertEquals('object', typeof localSetting.get());
      test.assertEquals('local', localSetting.get().s);
      test.assertEquals(1, localSetting.get().n);
      const globalSetting = Common.settings.createSetting('global', undefined);
      test.assertEquals('object', typeof globalSetting.get());
      test.assertEquals('global', globalSetting.get().s);
      test.assertEquals(2, globalSetting.get().n);
      test.releaseControl();
    }
  };

  TestSuite.prototype.testWindowInitializedOnNavigateBack = function() {
    const test = this;
    test.takeControl();
    const messages = SDK.consoleModel.messages();
    if (messages.length === 1)
      checkMessages();
    else
      SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, checkMessages.bind(this), this);

    function checkMessages() {
      const messages = SDK.consoleModel.messages();
      test.assertEquals(1, messages.length);
      test.assertTrue(messages[0].messageText.indexOf('Uncaught') === -1);
      test.releaseControl();
    }
  };

  TestSuite.prototype.testConsoleContextNames = function() {
    const test = this;
    test.takeControl();
    this.showPanel('console').then(() => this._waitForExecutionContexts(2, onExecutionContexts.bind(this)));

    function onExecutionContexts() {
      const consoleView = Console.ConsoleView.instance();
      const selector = consoleView._consoleContextSelector;
      const values = [];
      for (const item of selector._items)
        values.push(selector.titleFor(item));
      test.assertEquals('top', values[0]);
      test.assertEquals('Simple content script', values[1]);
      test.releaseControl();
    }
  };

  TestSuite.prototype.testRawHeadersWithHSTS = function(url) {
    const test = this;
    test.takeControl();
    SDK.targetManager.addModelListener(
        SDK.NetworkManager, SDK.NetworkManager.Events.ResponseReceived, onResponseReceived);

    this.evaluateInConsole_(`
      let img = document.createElement('img');
      img.src = "${url}";
      document.body.appendChild(img);
    `, () => {});

    let count = 0;
    function onResponseReceived(event) {
      const networkRequest = event.data;
      if (!networkRequest.url().startsWith('http'))
        return;
      switch (++count) {
        case 1:  // Original redirect
          test.assertEquals(301, networkRequest.statusCode);
          test.assertEquals('Moved Permanently', networkRequest.statusText);
          test.assertTrue(url.endsWith(networkRequest.responseHeaderValue('Location')));
          break;

        case 2:  // HSTS internal redirect
          test.assertTrue(networkRequest.url().startsWith('http://'));
          test.assertEquals(undefined, networkRequest.requestHeadersText());
          test.assertEquals(307, networkRequest.statusCode);
          test.assertEquals('Internal Redirect', networkRequest.statusText);
          test.assertEquals('HSTS', networkRequest.responseHeaderValue('Non-Authoritative-Reason'));
          test.assertTrue(networkRequest.responseHeaderValue('Location').startsWith('https://'));
          break;

        case 3:  // Final response
          test.assertTrue(networkRequest.url().startsWith('https://'));
          test.assertTrue(networkRequest.requestHeaderValue('Referer').startsWith('http://127.0.0.1'));
          test.assertEquals(200, networkRequest.statusCode);
          test.assertEquals('OK', networkRequest.statusText);
          test.assertEquals('132', networkRequest.responseHeaderValue('Content-Length'));
          test.releaseControl();
      }
    }
  };

  TestSuite.prototype.testDOMWarnings = function() {
    const messages = SDK.consoleModel.messages();
    this.assertEquals(1, messages.length);
    const expectedPrefix = '[DOM] Found 2 elements with non-unique id #dup:';
    this.assertTrue(messages[0].messageText.startsWith(expectedPrefix));
  };

  TestSuite.prototype.waitForTestResultsInConsole = function() {
    const messages = SDK.consoleModel.messages();
    for (let i = 0; i < messages.length; ++i) {
      const text = messages[i].messageText;
      if (text === 'PASS')
        return;
      else if (/^FAIL/.test(text))
        this.fail(text);  // This will throw.
    }
    // Neither PASS nor FAIL, so wait for more messages.
    function onConsoleMessage(event) {
      const text = event.data.messageText;
      if (text === 'PASS')
        this.releaseControl();
      else if (/^FAIL/.test(text))
        this.fail(text);
    }

    SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, onConsoleMessage, this);
    this.takeControl();
  };

  TestSuite.prototype._overrideMethod = function(receiver, methodName, override) {
    const original = receiver[methodName];
    if (typeof original !== 'function') {
      this.fail(`TestSuite._overrideMethod: $[methodName] is not a function`);
      return;
    }
    receiver[methodName] = function() {
      let value;
      try {
        value = original.apply(receiver, arguments);
      } finally {
        receiver[methodName] = original;
      }
      override.apply(original, arguments);
      return value;
    };
  };

  TestSuite.prototype.startTimeline = function(callback) {
    const test = this;
    this.showPanel('timeline').then(function() {
      const timeline = UI.panels.timeline;
      test._overrideMethod(timeline, '_recordingStarted', callback);
      timeline._toggleRecording();
    });
  };

  TestSuite.prototype.stopTimeline = function(callback) {
    const timeline = UI.panels.timeline;
    this._overrideMethod(timeline, 'loadingComplete', callback);
    timeline._toggleRecording();
  };

  TestSuite.prototype.invokePageFunctionAsync = function(functionName, opt_args, callback_is_always_last) {
    const callback = arguments[arguments.length - 1];
    const doneMessage = `DONE: ${functionName}.${++this._asyncInvocationId}`;
    const argsString = arguments.length < 3 ?
        '' :
        Array.prototype.slice.call(arguments, 1, -1).map(arg => JSON.stringify(arg)).join(',') + ',';
    this.evaluateInConsole_(
        `${functionName}(${argsString} function() { console.log('${doneMessage}'); });`, function() {});
    SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, onConsoleMessage);

    function onConsoleMessage(event) {
      const text = event.data.messageText;
      if (text === doneMessage) {
        SDK.consoleModel.removeEventListener(SDK.ConsoleModel.Events.MessageAdded, onConsoleMessage);
        callback();
      }
    }
  };

  TestSuite.prototype.invokeAsyncWithTimeline_ = function(functionName, callback) {
    const test = this;

    this.startTimeline(onRecordingStarted);

    function onRecordingStarted() {
      test.invokePageFunctionAsync(functionName, pageActionsDone);
    }

    function pageActionsDone() {
      test.stopTimeline(callback);
    }
  };

  TestSuite.prototype.enableExperiment = function(name) {
    Runtime.experiments.enableForTest(name);
  };

  TestSuite.prototype.checkInputEventsPresent = function() {
    const expectedEvents = new Set(arguments);
    const model = UI.panels.timeline._performanceModel.timelineModel();
    const asyncEvents = model.virtualThreads().find(thread => thread.isMainFrame).asyncEventsByGroup;
    const input = asyncEvents.get(TimelineModel.TimelineModel.AsyncEventGroup.input) || [];
    const prefix = 'InputLatency::';
    for (const e of input) {
      if (!e.name.startsWith(prefix))
        continue;
      if (e.steps.length < 2)
        continue;
      if (e.name.startsWith(prefix + 'Mouse') &&
          typeof TimelineModel.TimelineData.forEvent(e.steps[0]).timeWaitingForMainThread !== 'number')
        throw `Missing timeWaitingForMainThread on ${e.name}`;
      expectedEvents.delete(e.name.substr(prefix.length));
    }
    if (expectedEvents.size)
      throw 'Some expected events are not found: ' + Array.from(expectedEvents.keys()).join(',');
  };

  TestSuite.prototype.testInspectedElementIs = async function(nodeName) {
    this.takeControl();
    await self.runtime.loadModulePromise('elements');
    if (!Elements.ElementsPanel._firstInspectElementNodeNameForTest)
      await new Promise(f => this.addSniffer(Elements.ElementsPanel, '_firstInspectElementCompletedForTest', f));
    this.assertEquals(nodeName, Elements.ElementsPanel._firstInspectElementNodeNameForTest);
    this.releaseControl();
  };

  TestSuite.prototype.testDisposeEmptyBrowserContext = async function(url) {
    this.takeControl();
    const targetAgent = SDK.targetManager.mainTarget().targetAgent();
    const {browserContextId} = await targetAgent.invoke_createBrowserContext();
    const response1 = await targetAgent.invoke_getBrowserContexts();
    this.assertEquals(response1.browserContextIds.length, 1);
    await targetAgent.invoke_disposeBrowserContext({browserContextId});
    const response2 = await targetAgent.invoke_getBrowserContexts();
    this.assertEquals(response2.browserContextIds.length, 0);
    this.releaseControl();
  };

  TestSuite.prototype.testCreateBrowserContext = async function(url) {
    this.takeControl();
    const browserContextIds = [];
    const targetAgent = SDK.targetManager.mainTarget().targetAgent();

    const target1 = await createIsolatedTarget(url);
    const target2 = await createIsolatedTarget(url);

    const response = await targetAgent.invoke_getBrowserContexts();
    this.assertEquals(response.browserContextIds.length, 2);
    this.assertTrue(response.browserContextIds.includes(browserContextIds[0]));
    this.assertTrue(response.browserContextIds.includes(browserContextIds[1]));

    await evalCode(target1, 'localStorage.setItem("page1", "page1")');
    await evalCode(target2, 'localStorage.setItem("page2", "page2")');

    this.assertEquals(await evalCode(target1, 'localStorage.getItem("page1")'), 'page1');
    this.assertEquals(await evalCode(target1, 'localStorage.getItem("page2")'), null);
    this.assertEquals(await evalCode(target2, 'localStorage.getItem("page1")'), null);
    this.assertEquals(await evalCode(target2, 'localStorage.getItem("page2")'), 'page2');

    const removedTargets = [];
    SDK.targetManager.observeTargets({targetAdded: () => {}, targetRemoved: target => removedTargets.push(target)});
    await Promise.all([disposeBrowserContext(browserContextIds[0]), disposeBrowserContext(browserContextIds[1])]);
    this.assertEquals(removedTargets.length, 2);
    this.assertEquals(removedTargets.indexOf(target1) !== -1, true);
    this.assertEquals(removedTargets.indexOf(target2) !== -1, true);

    this.releaseControl();

    /**
     * @param {string} url
     * @return {!Promise<!SDK.Target>}
     */
    async function createIsolatedTarget(url) {
      const {browserContextId} = await targetAgent.invoke_createBrowserContext();
      browserContextIds.push(browserContextId);

      const {targetId} = await targetAgent.invoke_createTarget({url: 'about:blank', browserContextId});
      await targetAgent.invoke_attachToTarget({targetId});

      const target = SDK.targetManager.targets().find(target => target.id() === targetId);
      const pageAgent = target.pageAgent();
      await pageAgent.invoke_enable();
      await pageAgent.invoke_navigate({url});
      return target;
    }

    async function disposeBrowserContext(browserContextId) {
      const targetAgent = SDK.targetManager.mainTarget().targetAgent();
      await targetAgent.invoke_disposeBrowserContext({browserContextId});
    }

    async function evalCode(target, code) {
      return (await target.runtimeAgent().invoke_evaluate({expression: code})).result.value;
    }
  };

  TestSuite.prototype.testInputDispatchEventsToOOPIF = async function() {
    this.takeControl();

    await new Promise(callback => this._waitForTargets(2, callback));

    async function takeLogs(target) {
      const code = `
        (function() {
          var result = window.logs.join(' ');
          window.logs = [];
          return result;
        })()
      `;
      return (await target.runtimeAgent().invoke_evaluate({expression: code})).result.value;
    }

    let parentFrameOutput;
    let childFrameOutput;

    const inputAgent = SDK.targetManager.mainTarget().inputAgent();
    const runtimeAgent = SDK.targetManager.mainTarget().runtimeAgent();
    await inputAgent.invoke_dispatchMouseEvent({type: 'mousePressed', button: 'left', clickCount: 1, x: 10, y: 10});
    await inputAgent.invoke_dispatchMouseEvent({type: 'mouseMoved', button: 'left', clickCount: 1, x: 10, y: 20});
    await inputAgent.invoke_dispatchMouseEvent({type: 'mouseReleased', button: 'left', clickCount: 1, x: 10, y: 20});
    await inputAgent.invoke_dispatchMouseEvent({type: 'mousePressed', button: 'left', clickCount: 1, x: 230, y: 140});
    await inputAgent.invoke_dispatchMouseEvent({type: 'mouseMoved', button: 'left', clickCount: 1, x: 230, y: 150});
    await inputAgent.invoke_dispatchMouseEvent({type: 'mouseReleased', button: 'left', clickCount: 1, x: 230, y: 150});
    parentFrameOutput = 'Event type: mousedown button: 0 x: 10 y: 10 Event type: mouseup button: 0 x: 10 y: 20';
    this.assertEquals(parentFrameOutput, await takeLogs(SDK.targetManager.targets()[0]));
    childFrameOutput = 'Event type: mousedown button: 0 x: 30 y: 40 Event type: mouseup button: 0 x: 30 y: 50';
    this.assertEquals(childFrameOutput, await takeLogs(SDK.targetManager.targets()[1]));


    await inputAgent.invoke_dispatchKeyEvent({type: 'keyDown', key: 'a'});
    await runtimeAgent.invoke_evaluate({expression: `document.querySelector('iframe').focus()`});
    await inputAgent.invoke_dispatchKeyEvent({type: 'keyDown', key: 'a'});
    parentFrameOutput = 'Event type: keydown';
    this.assertEquals(parentFrameOutput, await takeLogs(SDK.targetManager.targets()[0]));
    childFrameOutput = 'Event type: keydown';
    this.assertEquals(childFrameOutput, await takeLogs(SDK.targetManager.targets()[1]));

    await inputAgent.invoke_dispatchTouchEvent({type: 'touchStart', touchPoints: [{x: 10, y: 10}]});
    await inputAgent.invoke_dispatchTouchEvent({type: 'touchEnd', touchPoints: []});
    await inputAgent.invoke_dispatchTouchEvent({type: 'touchStart', touchPoints: [{x: 230, y: 140}]});
    await inputAgent.invoke_dispatchTouchEvent({type: 'touchEnd', touchPoints: []});
    parentFrameOutput = 'Event type: touchstart touch x: 10 touch y: 10';
    this.assertEquals(parentFrameOutput, await takeLogs(SDK.targetManager.targets()[0]));
    childFrameOutput = 'Event type: touchstart touch x: 30 touch y: 40';
    this.assertEquals(childFrameOutput, await takeLogs(SDK.targetManager.targets()[1]));

    this.releaseControl();
  };

  TestSuite.prototype.testLoadResourceForFrontend = async function(baseURL) {
    const test = this;
    const loggedHeaders = new Set(['cache-control', 'pragma']);
    function testCase(url, headers, expectedStatus, expectedHeaders, expectedContent) {
      return new Promise(fulfill => {
        Host.ResourceLoader.load(url, headers, callback);

        function callback(statusCode, headers, content) {
          test.assertEquals(expectedStatus, statusCode);

          const headersArray = [];
          for (const name in headers) {
            const nameLower = name.toLowerCase();
            if (loggedHeaders.has(nameLower))
              headersArray.push(nameLower);
          }
          headersArray.sort();
          test.assertEquals(expectedHeaders.join(', '), headersArray.join(', '));
          test.assertEquals(expectedContent, content);
          fulfill();
        }
      });
    }

    this.takeControl();
    await testCase(baseURL + 'non-existent.html', undefined, 404, [], '');
    await testCase(baseURL + 'hello.html', undefined, 200, [], '<!doctype html>\n<p>hello</p>\n');
    await testCase(baseURL + 'echoheader?x-devtools-test', {'x-devtools-test': 'Foo'}, 200, ['cache-control'], 'Foo');
    await testCase(baseURL + 'set-header?pragma:%20no-cache', undefined, 200, ['pragma'], 'pragma: no-cache');

    await SDK.targetManager.mainTarget().runtimeAgent().invoke_evaluate({
      expression: `fetch("/set-cookie?devtools-test-cookie=Bar",
                         {credentials: 'include'})`,
      awaitPromise: true
    });
    await testCase(baseURL + 'echoheader?Cookie', undefined, 200, ['cache-control'], 'devtools-test-cookie=Bar');

    this.releaseControl();
  };

  /**
   * Serializes array of uiSourceCodes to string.
   * @param {!Array.<!Workspace.UISourceCode>} uiSourceCodes
   * @return {string}
   */
  TestSuite.prototype.uiSourceCodesToString_ = function(uiSourceCodes) {
    const names = [];
    for (let i = 0; i < uiSourceCodes.length; i++)
      names.push('"' + uiSourceCodes[i].url() + '"');
    return names.join(',');
  };

  /**
   * Returns all loaded non anonymous uiSourceCodes.
   * @return {!Array.<!Workspace.UISourceCode>}
   */
  TestSuite.prototype.nonAnonymousUISourceCodes_ = function() {
    /**
     * @param {!Workspace.UISourceCode} uiSourceCode
     */
    function filterOutService(uiSourceCode) {
      return !uiSourceCode.project().isServiceProject();
    }

    const uiSourceCodes = Workspace.workspace.uiSourceCodes();
    return uiSourceCodes.filter(filterOutService);
  };

  /*
 * Evaluates the code in the console as if user typed it manually and invokes
 * the callback when the result message is received and added to the console.
 * @param {string} code
 * @param {function(string)} callback
 */
  TestSuite.prototype.evaluateInConsole_ = function(code, callback) {
    function innerEvaluate() {
      UI.context.removeFlavorChangeListener(SDK.ExecutionContext, showConsoleAndEvaluate, this);
      const consoleView = Console.ConsoleView.instance();
      consoleView._prompt._appendCommand(code);

      this.addSniffer(Console.ConsoleView.prototype, '_consoleMessageAddedForTest', function(viewMessage) {
        callback(viewMessage.toMessageElement().deepTextContent());
      }.bind(this));
    }

    function showConsoleAndEvaluate() {
      Common.console.showPromise().then(innerEvaluate.bind(this));
    }

    if (!UI.context.flavor(SDK.ExecutionContext)) {
      UI.context.addFlavorChangeListener(SDK.ExecutionContext, showConsoleAndEvaluate, this);
      return;
    }
    showConsoleAndEvaluate.call(this);
  };

  /**
   * Checks that all expected scripts are present in the scripts list
   * in the Scripts panel.
   * @param {!Array.<string>} expected Regular expressions describing
   *     expected script names.
   * @return {boolean} Whether all the scripts are in "scripts-files" select
   *     box
   */
  TestSuite.prototype._scriptsAreParsed = function(expected) {
    const uiSourceCodes = this.nonAnonymousUISourceCodes_();
    // Check that at least all the expected scripts are present.
    const missing = expected.slice(0);
    for (let i = 0; i < uiSourceCodes.length; ++i) {
      for (let j = 0; j < missing.length; ++j) {
        if (uiSourceCodes[i].name().search(missing[j]) !== -1) {
          missing.splice(j, 1);
          break;
        }
      }
    }
    return missing.length === 0;
  };

  /**
   * Waits for script pause, checks expectations, and invokes the callback.
   * @param {function():void} callback
   */
  TestSuite.prototype._waitForScriptPause = function(callback) {
    this.addSniffer(SDK.DebuggerModel.prototype, '_pausedScript', callback);
  };

  /**
   * Waits until all the scripts are parsed and invokes the callback.
   */
  TestSuite.prototype._waitUntilScriptsAreParsed = function(expectedScripts, callback) {
    const test = this;

    function waitForAllScripts() {
      if (test._scriptsAreParsed(expectedScripts))
        callback();
      else
        test.addSniffer(UI.panels.sources.sourcesView(), '_addUISourceCode', waitForAllScripts);
    }

    waitForAllScripts();
  };

  TestSuite.prototype._waitForTargets = function(n, callback) {
    checkTargets.call(this);

    function checkTargets() {
      if (SDK.targetManager.targets().length >= n)
        callback.call(null);
      else
        this.addSniffer(SDK.TargetManager.prototype, 'createTarget', checkTargets.bind(this));
    }
  };

  TestSuite.prototype._waitForExecutionContexts = function(n, callback) {
    const runtimeModel = SDK.targetManager.mainTarget().model(SDK.RuntimeModel);
    checkForExecutionContexts.call(this);

    function checkForExecutionContexts() {
      if (runtimeModel.executionContexts().length >= n)
        callback.call(null);
      else
        this.addSniffer(SDK.RuntimeModel.prototype, '_executionContextCreated', checkForExecutionContexts.bind(this));
    }
  };


  window.uiTests = new TestSuite(window.domAutomationController);
})(window);
