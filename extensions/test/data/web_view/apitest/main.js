// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};

// TODO(lfg) Move these functions to a common js.
embedder.setUp_ = function(config) {
  if (!config || !config.testServer) {
    return;
  }
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.closeSocketURL = embedder.baseGuestURL + '/close-socket';
  embedder.emptyGuestURL = embedder.baseGuestURL + '/empty_guest.html';
  embedder.noReferrerGuestURL =
      embedder.baseGuestURL + '/guest_noreferrer.html';
  embedder.detectUserAgentURL = embedder.baseGuestURL + '/detect-user-agent';
  embedder.redirectGuestURL = embedder.baseGuestURL + '/server-redirect';
  embedder.redirectGuestURLDest =
      embedder.baseGuestURL + '/guest_redirect.html';
  embedder.windowOpenGuestURL = embedder.baseGuestURL + '/guest.html';
  embedder.sameDocumentNavigationURL =
      embedder.baseGuestURL + '/guest_same_document_navigation.html';
};

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    window.console.warn('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};

embedder.test = {};

embedder.test.assertEq = function(a, b, message) {
  if (a != b) {
    window.console.warn(
        'assertion failed: ' + a + ' != ' + b +
        (message ? (': ' + message) : ''));
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    window.console.warn('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    window.console.warn('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};


// Tests begin.

// This test verifies that the allowtransparency property cannot be changed
// once set. The attribute can only be deleted.
function testAllowTransparencyAttribute() {
  var webview = document.createElement('webview');
  webview.src = 'data:text/html,webview test';
  embedder.test.assertFalse(webview.hasAttribute('allowtransparency'));
  embedder.test.assertFalse(webview.allowtransparency);
  webview.allowtransparency = true;

  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(webview.hasAttribute('allowtransparency'));
    embedder.test.assertTrue(webview.allowtransparency);
    webview.allowtransparency = false;
    embedder.test.assertFalse(webview.hasAttribute('allowtransparency'));
    embedder.test.assertFalse(webview.allowtransparency);
    webview.allowtransparency = '';
    embedder.test.assertFalse(webview.hasAttribute('allowtransparency'));
    embedder.test.assertFalse(webview.allowtransparency);
    webview.allowtransparency = 'some string';
    embedder.test.assertTrue(webview.hasAttribute('allowtransparency'));
    embedder.test.assertTrue(webview.allowtransparency);
    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

function testAPIMethodExistence() {
  // See public-facing API functions in web_view_api_methods.js
  var WEB_VIEW_API_METHODS = [
    'addContentScripts',
    'back',
    'canGoBack',
    'canGoForward',
    'captureVisibleRegion',
    'clearData',
    'executeScript',
    'find',
    'forward',
    'getAudioState',
    'getProcessId',
    'getUserAgent',
    'getZoom',
    'getZoomMode',
    'go',
    'insertCSS',
    'isAudioMuted',
    'isSpatialNavigationEnabled',
    'isUserAgentOverridden',
    'loadDataWithBaseUrl',
    'print',
    'removeContentScripts',
    'reload',
    'setAudioMuted',
    'setSpatialNavigationEnabled',
    'setUserAgentOverride',
    'setZoom',
    'setZoomMode',
    'stop',
    'stopFinding',
    'terminate'
  ];

  var webview = document.createElement('webview');

  for (var methodName of WEB_VIEW_API_METHODS) {
    embedder.test.assertEq(
        'function', typeof webview[methodName],
        methodName + ' should be defined');
  }

  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function(e) {
    // Check contentWindow.
    embedder.test.assertEq('object', typeof webview.contentWindow);
    embedder.test.assertEq('function',
                           typeof webview.contentWindow.postMessage);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,webview check api');
  document.body.appendChild(webview);
}

function testCustomElementCallbacksInaccessible() {
  var CUSTOM_ELEMENT_CALLBACKS = [
    'connectedCallback',
    'disconnectedCallback',
    'attributeChangedCallback',
    'adoptedCallback'
  ];

  var webview = document.createElement('webview');
  for (var callbackName of CUSTOM_ELEMENT_CALLBACKS) {
    embedder.test.assertEq(
        'undefined', typeof webview[callbackName],
        callbackName + ' should not be accessible');
  }

  embedder.test.assertEq(
      'undefined', typeof webview.constructor['observedAttributes'],
      'observedAttributes should not be accessible');

  embedder.test.succeed();
}

// This test verifies that assigning the src attribute the same value it had
// prior to a crash spawns off a new guest process.
function testAssignSrcAfterCrash() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var terminated = false;
  webview.addEventListener('loadstop', function(evt) {
    if (!terminated) {
      webview.terminate();
      return;
    }
    // The guest has recovered after being terminated.
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(evt) {
    terminated = true;
    webview.setAttribute('src', 'data:text/html,test page');
  });
  webview.setAttribute('src', 'data:text/html,test page');
  document.body.appendChild(webview);
}

// Makes sure 'sizechanged' event is fired only if autosize attribute is
// specified.
// After loading <webview> without autosize attribute and a size, say size1,
// we set autosize attribute and new min size with size2. We would get (only
// one) sizechanged event with size1 as old size and size2 as new size.
function testAutosizeAfterNavigation() {
  var webview = document.createElement('webview');

  var step = 1;
  var autosizeWidth = -1;
  var autosizeHeight = -1;
  var sizeChangeHandler = function(e) {
    switch (step) {
      case 1:
        embedder.test.assertTrue(e.newWidth >= 60 && e.newWidth <= 70);
        embedder.test.assertTrue(e.newHeight >= 110 && e.newHeight <= 120);

        // Remove autosize attribute and expect webview to retain the same size.
        autosizeWidth = e.newWidth;
        autosizeHeight = e.newHeight;
        webview.removeAttribute('autosize');
        break;
      case 2:
        // Expect the autosized size.
        embedder.test.assertEq(autosizeWidth, e.newWidth);
        embedder.test.assertEq(autosizeHeight, e.newHeight);

        embedder.test.succeed();
        break;
      default:
        window.console.log('Unexpected sizechanged event, step = ' + step);
        embedder.test.fail();
        break;
    }

    ++step;
  };

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.addEventListener('loadstop', function(e) {
    webview.setAttribute('autosize', true);
    webview.setAttribute('minwidth', 60);
    webview.setAttribute('maxwidth', 70);
    webview.setAttribute('minheight', 110);
    webview.setAttribute('maxheight', 120);
  });

  webview.style.width = '50px';
  webview.style.height = '100px';
  webview.setAttribute('src', 'data:text/html,webview test sizechanged event');
  document.body.appendChild(webview);
}

// This test verifies that if a browser plugin is in autosize mode before
// navigation then the guest starts auto-sized.
function testAutosizeBeforeNavigation() {
  var webview = document.createElement('webview');

  webview.setAttribute('autosize', 'true');
  webview.setAttribute('minwidth', 200);
  webview.setAttribute('maxwidth', 210);
  webview.setAttribute('minheight', 100);
  webview.setAttribute('maxheight', 110);

  webview.addEventListener('sizechanged', function(e) {
    embedder.test.assertTrue(e.newWidth >= 200 && e.newWidth <= 210);
    embedder.test.assertTrue(e.newHeight >= 100 && e.newHeight <= 110);
    embedder.test.succeed();
  });

  webview.setAttribute('src', 'data:text/html,webview test sizechanged event');
  document.body.appendChild(webview);
}

// This test verifies that a lengthy page with autosize enabled will report
// the correct height in the sizechanged event.
function testAutosizeHeight() {
  var webview = document.createElement('webview');

  webview.autosize = true;
  webview.minwidth = 200;
  webview.maxwidth = 210;
  webview.minheight = 40;
  webview.maxheight = 200;

  var step = 1;
  var finalWidth = 200;
  var finalHeight = 50;
  webview.addEventListener('sizechanged', function(e) {
    embedder.test.assertTrue(e.newHeight >= webview.minheight);
    embedder.test.assertTrue(e.newHeight <= webview.maxheight);
    embedder.test.assertTrue(e.newWidth >= webview.minwidth);
    embedder.test.assertTrue(e.newWidth <= webview.maxwidth);
    if (step == 1)
      webview.maxheight = 50;

    // We are done once the size settles on the final width and height.
    if (e.newHeight == finalHeight && e.newWidth == finalWidth)
      embedder.test.succeed();
    ++step;
  });

  webview.src = 'data:text/html,' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>';
  document.body.appendChild(webview);
}

// This test verifies that all autosize attributes can be removed
// without crashing the plugin, or throwing errors.
function testAutosizeRemoveAttributes() {
  var webview = document.createElement('webview');

  var step = 1;
  var sizeChangeHandler = function(e) {
    switch (step) {
      case 1:
        // This is the sizechanged event for autosize.

        // Remove attributes.
        webview.removeAttribute('minwidth');
        webview.removeAttribute('maxwidth');
        webview.removeAttribute('minheight');
        webview.removeAttribute('maxheight');
        webview.removeAttribute('autosize');

        // We'd get one more sizechanged event after we turn off
        // autosize.
        webview.style.width = '500px';
        webview.style.height = '500px';
        break;
      case 2:
        embedder.test.succeed();
        break;
    }

    ++step;
  };

  webview.addEventListener('loadstop', function(e) {
    webview.minwidth = 300;
    webview.maxwidth = 700;
    webview.minheight = 600;
    webview.maxheight = 400;
    webview.autosize = true;
  });

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.style.width = '640px';
  webview.style.height = '480px';
  webview.setAttribute('src', 'data:text/html,webview check autosize');
  document.body.appendChild(webview);
}

// This test verifies that autosize works when some of the parameters are unset.
function testAutosizeWithPartialAttributes() {
  window.console.log('testAutosizeWithPartialAttributes');
  var webview = document.createElement('webview');

  var step = 1;
  var sizeChangeHandler = function(e) {
    window.console.log('sizeChangeHandler, new: ' +
                       e.newWidth + ' X ' + e.newHeight);
    switch (step) {
      case 1:
        // Expect 300x200.
        embedder.test.assertEq(300, e.newWidth);
        embedder.test.assertEq(200, e.newHeight);

        // Change the min size to cause a relayout.
        webview.minwidth = 500;
        break;
      case 2:
        embedder.test.assertTrue(e.newWidth >= webview.minwidth);
        embedder.test.assertTrue(e.newWidth <= webview.maxwidth);

        // Tests when minwidth > maxwidth, minwidth = maxwidth.
        // i.e. minwidth is essentially 700.
        webview.minwidth = 800;
        break;
      case 3:
        // Expect 700X?
        embedder.test.assertEq(700, e.newWidth);
        embedder.test.assertTrue(e.newHeight >= 200);
        embedder.test.assertTrue(e.newHeight <= 600);

        embedder.test.succeed();
        break;
      default:
        window.console.log('Unexpected sizechanged event, step = ' + step);
        embedder.test.fail();
        break;
    }

    ++step;
  };

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.addEventListener('loadstop', function(e) {
    webview.minwidth = 300;
    webview.maxwidth = 700;
    webview.minheight = 200;
    webview.maxheight = 600;
    webview.autosize = true;
  });

  webview.style.width = '640px';
  webview.style.height = '480px';
  webview.setAttribute('src', 'data:text/html,webview check autosize');
  document.body.appendChild(webview);
}

// This test registers two event listeners on a same event (loadcommit).
// Each of the listener tries to change some properties on the event param,
// which should not be possible.
function testCannotMutateEventName() {
  var webview = document.createElement('webview');
  var url = 'data:text/html,<body>Two</body>';
  var loadCommitACalled = false;
  var loadCommitBCalled = false;

  var maybeFinishTest = function(e) {
    if (loadCommitACalled && loadCommitBCalled) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.succeed();
    }
  };

  var onLoadCommitA = function(e) {
    if (e.url == url) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertFalse(loadCommitACalled);
      loadCommitACalled = true;
      // Try mucking with properities inside |e|.
      e.type = 'modified';
      maybeFinishTest(e);
    }
  };
  var onLoadCommitB = function(e) {
    if (e.url == url) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertFalse(loadCommitBCalled);
      loadCommitBCalled = true;
      // Try mucking with properities inside |e|.
      e.type = 'modified';
      maybeFinishTest(e);
    }
  };

  // The test starts from here, by setting the src to |url|. Event
  // listener registration works because we already have a (dummy) src set
  // on the <webview> tag.
  webview.addEventListener('loadcommit', onLoadCommitA);
  webview.addEventListener('loadcommit', onLoadCommitB);
  webview.setAttribute('src', url);
  document.body.appendChild(webview);
}

// This test verifies that the loadstop event fires when loading a webview
// accessible resource from a partition that is privileged if the src URL
// is not fully qualified.
function testChromeExtensionRelativePath() {
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'guest_with_inline_script.html');
  document.body.appendChild(webview);
}

// This test verifies that the loadstop event fires when loading a webview
// accessible resource from a partition that is privileged.
function testChromeExtensionURL() {
  var localResource = chrome.runtime.getURL('guest_with_inline_script.html');
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', localResource);
  document.body.appendChild(webview);
}

// This test verifies that guests are blocked from navigating the webview to a
// data URL.
function testContentInitiatedNavigationToDataUrlBlocked() {
  var navUrl = "data:text/html,foo";
  var webview = document.createElement('webview');
  webview.addEventListener('consolemessage', function(e) {
    if (e.message.startsWith(
        'Not allowed to navigate top frame to data URL:')) {
      embedder.test.succeed();
    }
  });
  webview.addEventListener('loadstop', function(e) {
    if (webview.getAttribute('src') == navUrl) {
      embedder.test.fail();
    }
  });
  webview.setAttribute('src',
      'data:text/html,<script>window.location.href = "' + navUrl +
      '";</scr' + 'ipt>');
  document.body.appendChild(webview);
}

// This test verifies that the load event fires when the a new page is
// loaded.
// TODO(fsamuel): Add a test to verify that subframe loads within a guest
// do not fire the 'contentload' event.
function testContentLoadEvent() {
  var webview = document.createElement('webview');
  webview.addEventListener('contentload', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test verifies that the basic use cases of the declarative WebRequest API
// work as expected. This test demonstrates that rules can be added prior to
// navigation and attachment.
// 1. It adds a rule to block URLs that contain guest.
// 2. It attempts to navigate to a guest.html page.
// 3. It detects the appropriate loadabort message.
// 4. It removes the rule blocking the page and reloads.
// 5. The page loads successfully.
function testDeclarativeWebRequestAPI() {
  var step = 1;
  var webview = new WebView();
  var rule = {
    conditions: [
      new chrome.webViewRequest.RequestMatcher(
        {
          url: { urlContains: 'guest' }
        }
      )
    ],
    actions: [
      new chrome.webViewRequest.CancelRequest()
    ]
  };
  webview.request.onRequest.addRules([rule]);
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(1, step);
    embedder.test.assertEq('ERR_BLOCKED_BY_CLIENT', e.reason);
    step = 2;
    webview.request.onRequest.removeRules();
    webview.reload();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq(2, step);
    embedder.test.succeed();
  });
  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

function testDeclarativeWebRequestAPISendMessage() {
  var webview = new WebView();
  window.console.log(embedder.emptyGuestURL);
  var rule = {
    conditions: [
      new chrome.webViewRequest.RequestMatcher(
        {
          url: { urlContains: 'guest' }
        }
      )
    ],
    actions: [
      new chrome.webViewRequest.SendMessageToExtension({ message: 'bleep' })
    ]
  };
  webview.request.onRequest.addRules([rule]);
  webview.request.onMessage.addListener(function(e) {
    embedder.test.assertEq('bleep', e.message);
    embedder.test.succeed();
  });
  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// This test registers two listeners on an event (loadcommit) and removes
// the <webview> tag when the first listener fires.
// Current expected behavior is that the second event listener will still
// fire without crashing.
function testDestroyOnEventListener() {
  var webview = document.createElement('webview');
  var url = 'data:text/html,<body>Destroy test</body>';

  var loadCommitCount = 0;
  function loadCommitCommon(e) {
    embedder.test.assertEq('loadcommit', e.type);
    if (url != e.url)
      return;
    ++loadCommitCount;
    if (loadCommitCount == 2) {
      // Pass in a timeout so that we can catch if any additional loadcommit
      // occurs.
      setTimeout(function() {
        embedder.test.succeed();
      }, 0);
    } else if (loadCommitCount > 2) {
      embedder.test.fail();
    }
  };

  // The test starts from here, by setting the src to |url|.
  webview.addEventListener('loadcommit', function(e) {
    window.console.log('loadcommit1');
    webview.parentNode.removeChild(webview);
    loadCommitCommon(e);
  });
  webview.addEventListener('loadcommit', function(e) {
    window.console.log('loadcommit2');
    loadCommitCommon(e);
  });
  webview.setAttribute('src', url);
  document.body.appendChild(webview);
}

// Tests that a <webview> that starts with "display: none" style loads
// properly.
function testDisplayNoneWebviewLoad() {
  var webview = document.createElement('webview');
  var visible = false;
  webview.style.display = 'none';
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(visible);
    embedder.test.succeed();
  });
  // Set the .src while we are "display: none".
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);

  setTimeout(function() {
    visible = true;
    // This should trigger loadstop.
    webview.style.display = '';
  }, 0);
}

function testDisplayNoneWebviewRemoveChild() {
  var webview = document.createElement('webview');
  var visibleAndInDOM = false;
  webview.style.display = 'none';
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(visibleAndInDOM);
    embedder.test.succeed();
  });
  // Set the .src while we are "display: none".
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);

  setTimeout(function() {
    webview.parentNode.removeChild(webview);
    webview.style.display = '';
    visibleAndInDOM = true;
    // This should trigger loadstop.
    document.body.appendChild(webview);
  }, 0);
}

// This test verifies that the loadstart, loadstop, and exit events fire as
// expected.
function testEventName() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);

  webview.addEventListener('loadstart', function(evt) {
    embedder.test.assertEq('loadstart', evt.type);
  });

  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq('loadstop', evt.type);
    webview.terminate();
  });

  webview.addEventListener('exit', function(evt) {
    embedder.test.assertEq('exit', evt.type);
    embedder.test.succeed();
  });

  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

function testExecuteScript() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadstop', function() {
    webview.executeScript(
      {code:'document.body.style.backgroundColor = "red";'},
      function(results) {
        embedder.test.assertEq(1, results.length);
        embedder.test.assertEq('red', results[0]);
        embedder.test.succeed();
      });
  });
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

function testExecuteScriptFail() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);
  setTimeout(function() {
    webview.executeScript(
        {code:'document.body.style.backgroundColor = "red";'},
        function(results) {
          embedder.test.fail();
        });
    setTimeout(function() {
      embedder.test.succeed();
    }, 0);
  }, 0);
}

// This test verifies that the call to executeScript will fail and return null
// if the webview has been navigated between the time the call was made and the
// time it arrives in the guest process.
function testExecuteScriptIsAbortedWhenWebViewSourceIsChanged() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadstop', function onLoadStop(e) {
    window.console.log('2. Inject script to trigger a guest-initiated ' +
        'navigation.');
    var navUrl = 'data:text/html,trigger nav';
    webview.executeScript({
      code: 'window.location.href = "' + navUrl + '";'
    });

    window.console.log('3. Listening for the load that will be started as a ' +
        'result of 2.');
    webview.addEventListener('loadstart', function onLoadStart(e) {
      embedder.test.assertEq('about:blank', webview.src);
      window.console.log('4. Attempting to inject script into about:blank. ' +
          'This is expected to fail.');
      webview.executeScript(
        { code: 'document.body.style.backgroundColor = "red";' },
        function(results) {
          window.console.log(
              '5. Verify that executeScript has, indeed, failed.');
          embedder.test.assertEq(null, results);
          embedder.test.assertEq(navUrl, webview.src);
          embedder.test.succeed();
        }
      );
      webview.removeEventListener('loadstart', onLoadStart);
    });
    webview.removeEventListener('loadstop', onLoadStop);
  });

  window.console.log('1. Performing initial navigation.');
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);
}

function testFindAPI() {
  var webview = new WebView();
  webview.src = 'data:text/html,Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br><br>' +
      '<a href="about:blank">Click here!</a>';

  var loadstopListener2 = function(e) {
    embedder.test.assertEq(webview.src, "about:blank");
    // Test find results when looking for nothing.
    webview.find("", {}, function(results) {
      embedder.test.assertEq(results.numberOfMatches, 0);
      embedder.test.assertEq(results.activeMatchOrdinal, 0);
      embedder.test.assertEq(results.selectionRect.left, 0);
      embedder.test.assertEq(results.selectionRect.top, 0);
      embedder.test.assertEq(results.selectionRect.width, 0);
      embedder.test.assertEq(results.selectionRect.height, 0);

      embedder.test.succeed();
    });
  }

  var loadstopListener1 = function(e) {
    // Test find results.
    webview.find("dog", {}, function(results) {
      embedder.test.assertEq(results.numberOfMatches, 100);
      embedder.test.assertTrue(results.selectionRect.width > 0);
      embedder.test.assertTrue(results.selectionRect.height > 0);

      // Test finding next active matches.
      webview.find("dog");
      webview.find("dog");
      webview.find("dog");
      webview.find("dog");
      webview.find("dog", {}, function(results) {
        embedder.test.assertEq(results.activeMatchOrdinal, 6);
        webview.find("dog", {backward: true});
        webview.find("dog", {backward: true}, function(results) {
          // Test the |backward| find option.
          embedder.test.assertEq(results.activeMatchOrdinal, 4);

          // Test the |matchCase| find option.
          webview.find("Dog", {matchCase: true}, function(results) {
            embedder.test.assertEq(results.numberOfMatches, 40);

            // Test canceling find requests.
            webview.find("dog");
            webview.stopFinding();
            webview.find("dog");
            webview.find("cat");

            // Test find results when looking for something that isn't there.
            webview.find("fish", {}, function(results) {
              embedder.test.assertEq(results.numberOfMatches, 0);
              embedder.test.assertEq(results.activeMatchOrdinal, 0);
              embedder.test.assertEq(results.selectionRect.left, 0);
              embedder.test.assertEq(results.selectionRect.top, 0);
              embedder.test.assertEq(results.selectionRect.width, 0);
              embedder.test.assertEq(results.selectionRect.height, 0);

              // Test following a link with stopFinding().
              webview.removeEventListener('loadstop', loadstopListener1);
              webview.addEventListener('loadstop', loadstopListener2);
              webview.find("click here!", {}, function() {
                webview.stopFinding("activate");
              });
            });
          });
        });
      });
    });
  };

  webview.addEventListener('loadstop', loadstopListener1);
  document.body.appendChild(webview);
};

function testFindAPI_findupdate() {
  var webview = new WebView();
  webview.src = 'data:text/html,Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br>' +
      'Dog dog dog Dog dog dogcatDog dogDogdog<br><br>' +
      '<a href="about:blank">Click here!</a>';
  var canceledTest = false;
  webview.addEventListener('loadstop', function(e) {
    // Test the |findupdate| event.
    webview.addEventListener('findupdate', function(e) {
      if (e.activeMatchOrdinal > 0) {
        // embedder.test.assertTrue(e.numberOfMatches >= e.activeMatchOrdinal)
        // This currently fails because of http://crbug.com/342445 .
        embedder.test.assertTrue(e.selectionRect.width > 0);
        embedder.test.assertTrue(e.selectionRect.height > 0);
      }

      if (e.finalUpdate) {
        if (e.canceled) {
          canceledTest = true;
        } else {
          embedder.test.assertEq(e.searchText, "dog");
          embedder.test.assertEq(e.numberOfMatches, 100);
          embedder.test.assertEq(e.activeMatchOrdinal, 1);
          embedder.test.assertTrue(canceledTest);
          embedder.test.succeed();
        }
      }
    });
    wv.find("dog");
    wv.find("cat");
    wv.find("dog");
  });

  document.body.appendChild(webview);
};

// This test verifies that getProcessId is defined and returns a non-zero
// value corresponding to the processId of the guest process.
function testGetProcessId() {
  var webview = document.createElement('webview');
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  var firstLoad = function() {
    webview.removeEventListener('loadstop', firstLoad);
    embedder.test.assertTrue(webview.getProcessId() > 0);
    embedder.test.succeed();
  };
  webview.addEventListener('loadstop', firstLoad);
  document.body.appendChild(webview);
}

function testHiddenBeforeNavigation() {
  var webview = document.createElement('webview');
  webview.style.visibility = 'hidden';

  var postMessageHandler = function(e) {
    var data = JSON.parse(e.data);
    window.removeEventListener('message', postMessageHandler);
    if (data[0] == 'visibilityState-response') {
      embedder.test.assertEq('hidden', data[1]);
      embedder.test.succeed();
    } else {
      window.console.warn('Unexpected message: ' + data);
      embedder.test.fail();
    }
  };

  webview.addEventListener('loadstop', function(e) {
    window.console.warn('webview.loadstop');
    window.addEventListener('message', postMessageHandler);
    webview.addEventListener('consolemessage', function(e) {
      window.console.warn('g: ' + e.message);
    });

    webview.executeScript(
      {file: 'inject_hidden_test.js'},
      function(results) {
        if (!results || !results.length) {
          window.console.warn('Failed to inject script: inject_hidden_test.js');
          embedder.test.fail();
          return;
        }

        window.console.warn('script injection success');
        webview.contentWindow.postMessage(
            JSON.stringify(['visibilityState-request']), '*');
      });
  });

  webview.setAttribute('src', 'data:text/html,<html><body></body></html>');
  document.body.appendChild(webview);
}

// Makes sure inline scripts works inside guest that was loaded from
// accessible_resources.
function testInlineScriptFromAccessibleResources() {
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('consolemessage', function(e) {
    window.console.log('consolemessage: ' + e.message);
    if (e.message == 'guest_with_inline_script.html: Inline script ran') {
      embedder.test.succeed();
    }
  });
  webview.setAttribute('src', 'guest_with_inline_script.html');
  document.body.appendChild(webview);
}

// This tests verifies that webview fires a loadabort event instead of crashing
// the browser if we attempt to navigate to a chrome-extension: URL with an
// extension ID that does not exist.
function testInvalidChromeExtensionURL() {
  var invalidResource = 'chrome-extension://abc123/guest.html';
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', invalidResource);
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires when loading a webview
// accessible resource from a partition that is not privileged.
function testLoadAbortChromeExtensionURLWrongPartition() {
  var localResource = chrome.runtime.getURL('guest.html');
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_BLOCKED_BY_CLIENT', e.reason);
    embedder.test.succeed();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.fail();
  });
  webview.setAttribute('src', localResource);
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires as expected and with the
// appropriate fields when an empty response is returned.
function testLoadAbortEmptyResponse() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_EMPTY_RESPONSE', e.reason);
    embedder.test.succeed();
  });
  webview.setAttribute('src', embedder.closeSocketURL);
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires as expected when an illegal
// chrome URL is provided.
function testLoadAbortIllegalChromeURL() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
  });
  webview.addEventListener('loadstop', function(e)  {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.src = 'chrome://newtab';
  document.body.appendChild(webview);
}

function testLoadAbortIllegalFileURL() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.src = 'file://foo';
  document.body.appendChild(webview);
}

function testLoadAbortIllegalJavaScriptURL() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'javascript:void(document.bgColor="#0000FF")');
  document.body.appendChild(webview);
}

// Verifies that navigating to invalid URL (e.g. 'http:') doesn't cause a crash.
function testLoadAbortInvalidNavigation() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_INVALID_URL', e.reason);
    embedder.test.assertEq('', e.url);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(e) {
    // We should not crash.
    embedder.test.fail();
  });
  webview.src = 'http:';
  document.body.appendChild(webview);
}

// This test verifies that canGoBack is true for failed navigations.
function testCanGoBack() {
  var testPage = 'data:text/html,test page';
  var badUrl = 'http://foo.bar/';
  var webview = document.createElement('webview');
  webview.addEventListener('loadcommit', function(evt) {
    if (evt.url == testPage) {
      webview.src = badUrl;
    } else if (evt.url == badUrl) {
      embedder.test.assertTrue(webview.canGoBack());
      embedder.test.succeed();
    }
  });
  webview.src = testPage;
  document.body.appendChild(webview);
}

// Verifies that navigation to a URL that is valid but not web-safe or
// pseudo-scheme fires loadabort and doesn't cause a crash.
function testLoadAbortNonWebSafeScheme() {
  var webview = document.createElement('webview');
  var chromeGuestURL = 'chrome-guest://abc123/';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
    embedder.test.assertEq(chromeGuestURL, e.url);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(e) {
    // We should not crash.
    embedder.test.fail();
  });
  webview.src = chromeGuestURL;
  document.body.appendChild(webview);
};

// Verifies that cancelling a navigation due to an unsupported protocol doesn't
// cause a crash.
function testLoadAbortUnknownScheme() {
  var webview = document.createElement('webview');
  var ftpURL = 'ftp://example.com/';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq('ERR_UNKNOWN_URL_SCHEME', e.reason);
    embedder.test.assertEq(ftpURL, e.url);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq(ftpURL, webview.src);
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(e) {
    // We should not crash.
    embedder.test.fail();
  });
  webview.src = ftpURL;
  document.body.appendChild(webview);
}

// This test verifies that the loadStart isn't sent for same-document
// navigations, while loadCommit is (per docs).
function testLoadEventsSameDocumentNavigation() {
  var webview = new WebView();
  var loadStartCount = 0;
  var loadCommitCount = 0;
  webview.addEventListener('loadstart', function(evt) {
    loadStartCount++;
  });
  webview.addEventListener('loadcommit', function(e) {
    loadCommitCount++;
  });
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq(1, loadStartCount);
    embedder.test.assertEq(2, loadCommitCount);
    embedder.test.succeed();
  });

  webview.src = embedder.sameDocumentNavigationURL;
  document.body.appendChild(webview);
}

// Tests that the 'loadprogress' event is triggered correctly.
function testLoadProgressEvent() {
  var webview = document.createElement('webview');
  var progress = 0;

  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq(1, progress);
    embedder.test.succeed();
  });

  webview.addEventListener('loadprogress', function(evt) {
    progress = evt.progress;
  });

  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test verifies that the loadstart event fires at the beginning of a load
// and the loadredirect event fires when a redirect occurs.
function testLoadStartLoadRedirect() {
  var webview = document.createElement('webview');
  var loadstartCalled = false;
  webview.setAttribute('src', embedder.redirectGuestURL);
  webview.addEventListener('loadstart', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.redirectGuestURL, e.url);
    loadstartCalled = true;
  });
  webview.addEventListener('loadredirect', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.redirectGuestURL,
        e.oldUrl.replace('127.0.0.1', 'localhost'));
    embedder.test.assertEq(embedder.redirectGuestURLDest,
        e.newUrl.replace('127.0.0.1', 'localhost'));
    if (loadstartCalled) {
      embedder.test.succeed();
    } else {
      embedder.test.fail();
    }
  });
  document.body.appendChild(webview);
}

// This test ensures if the guest isn't there and we resize the guest (from JS),
// it remembers the size correctly.
function testNavigateAfterResize() {
  var webview = document.createElement('webview');

  var postMessageHandler = function(e) {
    var data = JSON.parse(e.data);
    webview.removeEventListener('message', postMessageHandler);
    if (data[0] == 'dimension-response') {
      var actualWidth = data[1];
      var actualHeight = data[2];
      embedder.test.assertEq(100, actualWidth);
      embedder.test.assertEq(125, actualHeight);
      embedder.test.succeed();
    }
  };
  window.addEventListener('message', postMessageHandler);

  webview.addEventListener('consolemessage', function(e) {
    window.console.warn('guest log: ' + e.message);
  });

  webview.addEventListener('loadstop', function(e) {
    webview.executeScript(
      {file: 'navigate_after_resize.js'},
      function(results) {
        if (!results || !results.length) {
          window.console.warn('Failed to inject navigate_after_resize.js');
          embedder.test.fail();
          return;
        }
        var msg = ['dimension-request'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
  });

  // First set size.
  webview.style.width = '100px';
  webview.style.height = '125px';

  // Then navigate.
  webview.src = 'about:blank';
  document.body.appendChild(webview);
}

// This test verifies that multiple consecutive changes to the <webview> src
// attribute will cause a navigation.
function testNavOnConsecutiveSrcAttributeChanges() {
  var testPage1 = 'data:text/html,test page 1';
  var testPage2 = 'data:text/html,test page 2';
  var testPage3 = 'data:text/html,test page 3';
  var webview = document.createElement('webview');
  webview.partition = arguments.callee.name;
  var loadCommitCount = 0;
  webview.addEventListener('loadcommit', function(e) {
    if (e.url == testPage3) {
      embedder.test.succeed();
    }
    loadCommitCount++;
    if (loadCommitCount > 3) {
      embedder.test.fail();
    }
  });
  document.body.appendChild(webview);
  webview.src = testPage1;
  webview.src = testPage2;
  webview.src = testPage3;
}

// This test verifies that we can set the <webview> src multiple times and the
// changes will cause a navigation.
function testNavOnSrcAttributeChange() {
  var testPage1 = 'data:text/html,test page 1';
  var testPage2 = 'data:text/html,test page 2';
  var testPage3 = 'data:text/html,test page 3';
  var tests = [testPage1, testPage2, testPage3];
  var webview = document.createElement('webview');
  webview.partition = arguments.callee.name;
  var loadCommitCount = 0;
  webview.addEventListener('loadcommit', function(evt) {
    var success = tests.indexOf(evt.url) > -1;
    embedder.test.assertTrue(success);
    ++loadCommitCount;
    if (loadCommitCount == tests.length) {
      embedder.test.succeed();
    } else if (loadCommitCount > tests.length) {
      embedder.test.fail();
    } else {
      webview.src = tests[loadCommitCount];
    }
  });
  webview.src = tests[0];
  document.body.appendChild(webview);
}

// This test verifies that new window attachment functions as expected.
//
// TODO(crbug.com/594215) Test that opening a new window with a data URL is
// blocked. There is currently no way to test this, as the block message is
// printed on the new window which never gets created, so the message is lost.
// Also test that opening a new window with a data URL when the webview is
// already on a data URL is allowed.
function testNewWindow() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    newwebview.addEventListener('loadstop', function(evt) {
      // If the new window finishes loading, the test is successful.
      embedder.test.succeed();
    });
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    e.window.attach(newwebview);
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

// This test verifies that the attach can be called inline without
// preventing default.
function testNewWindowNoPreventDefault() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    var newwebview = document.createElement('webview');
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    try {
      e.window.attach(newwebview);
      embedder.test.succeed();
    } catch (err) {
      embedder.test.fail();
    }
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

function testNewWindowNoReferrerLink() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    newwebview.addEventListener('loadstop', function(evt) {
      // If the new window finishes loading, the test is successful.
      embedder.test.succeed();
    });
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    e.window.attach(newwebview);
  });
  webview.setAttribute('src', embedder.noReferrerGuestURL);
  document.body.appendChild(webview);
}

// This test verifies "first-call-wins" semantics. That is, the first call
// to perform an action on the new window takes the action and all
// subsequent calls throw an exception.
function testNewWindowTwoListeners() {
  var webview = document.createElement('webview');
  var error = false;
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    document.body.appendChild(newwebview);
    try {
      e.window.attach(newwebview);
    } catch (err) {
      embedder.test.fail();
    }
  });
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    try {
      e.window.discard();
    } catch (err) {
      embedder.test.succeed();
    }
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

function testOnEventProperties() {
  var sequence = ['first', 'second', 'third', 'fourth'];
  var webview = document.createElement('webview');
  function createHandler(id) {
    return function(e) {
      embedder.test.assertEq(id, sequence.shift());
    };
  }

  webview.addEventListener('loadstart', createHandler('first'));
  webview.addEventListener('loadstart', createHandler('second'));
  webview.onloadstart = createHandler('third');
  webview.addEventListener('loadstart', createHandler('fourth'));
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq(0, sequence.length);

    // Test that setting another 'onloadstart' handler replaces the previous
    // handler.
    sequence = ['first', 'second', 'fourth'];
    webview.onloadstart = function() {
      embedder.test.assertEq(0, sequence.length);
      embedder.test.succeed();
    };

    webview.setAttribute('src', 'data:text/html,next navigation');
  });

  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test verifies that the partion attribute cannot be changed after the src
// has been set.
function testPartitionChangeAfterNavigation() {
  var webview = document.createElement('webview');
  var partitionAttribute = arguments.callee.name;
  webview.setAttribute('partition', partitionAttribute);

  var loadstopHandler = function(e) {
    webview.partition = 'illegal';
    embedder.test.assertEq(partitionAttribute, webview.partition);
    embedder.test.succeed();
  };
  webview.addEventListener('loadstop', loadstopHandler);

  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test verifies that removing partition attribute after navigation does
// not work, i.e. the partition remains the same.
function testPartitionRemovalAfterNavigationFails() {
  var webview = document.createElement('webview');

  var partition = 'testme';
  webview.setAttribute('partition', partition);

  var loadstopHandler = function(e) {
    // Removing after navigation should not change the partition.
    webview.removeAttribute('partition');
    embedder.test.assertEq('testme', webview.partition);
    embedder.test.succeed();
  };
  webview.addEventListener('loadstop', loadstopHandler);

  webview.setAttribute('src', 'data:text/html,<html><body>guest</body></html>');
  document.body.appendChild(webview);
}

// This test verifies that <webview> reloads the page if the src attribute is
// assigned the same value.
function testReassignSrcAttribute() {
  var dataUrl = 'data:text/html,test page';
  var webview = document.createElement('webview');
  webview.partition = arguments.callee.name;

  var loadStopCount = 0;
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq(dataUrl, webview.getAttribute('src'));
    ++loadStopCount;
    console.log('[' + loadStopCount + '] loadstop called');
    if (loadStopCount == 3) {
      embedder.test.succeed();
    } else if (loadStopCount > 3) {
      embedder.test.fail();
    } else {
      webview.src = dataUrl;
    }
  });
  webview.src = dataUrl;
  document.body.appendChild(webview);
}

// This test verifies that the reload method on webview functions as expected.
function testReload() {
  var triggerNavUrl = 'data:text/html,trigger navigation';
  var webview = document.createElement('webview');

  var loadCommitCount = 0;
  webview.addEventListener('loadstop', function(e) {
    if (loadCommitCount < 2) {
      webview.reload();
    } else if (loadCommitCount == 2) {
      embedder.test.succeed();
    } else {
      embedder.test.fail();
    }
  });
  webview.addEventListener('loadcommit', function(e) {
    embedder.test.assertEq(triggerNavUrl, e.url);
    embedder.test.assertTrue(e.isTopLevel);
    loadCommitCount++;
  });

  webview.setAttribute('src', triggerNavUrl);
  document.body.appendChild(webview);
}

// This test verifies that the reload method on webview functions as expected.
function testReloadAfterTerminate() {
  var triggerNavUrl = 'data:text/html,trigger navigation';
  var webview = document.createElement('webview');

  var step = 1;
  webview.addEventListener('loadstop', function(e) {
    switch (step) {
      case 1:
        webview.terminate();
        break;
      case 2:
        setTimeout(function() { embedder.test.succeed(); }, 0);
        break;
      default:
        window.console.log('Unexpected loadstop event, step = ' + step);
        embedder.test.fail();
        break;
    }
    ++step;
  });

  webview.addEventListener('exit', function(e) {
    // Trigger a focus state change of the guest to test for
    // http://crbug.com/413874.
    webview.blur();
    webview.focus();
    setTimeout(function() { webview.reload(); }, 0);
  });

  webview.src = triggerNavUrl;
  document.body.appendChild(webview);
}

// This test verifies that <webview> restores the src attribute if it is
// removed after navigation.
function testRemoveSrcAttribute() {
  var dataUrl = 'data:text/html,test page';
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var terminated = false;
  webview.addEventListener('loadstop', function(evt) {
    webview.removeAttribute('src');
    setTimeout(function() {
      embedder.test.assertEq(dataUrl, webview.getAttribute('src'));
      embedder.test.succeed();
    }, 0);
  });
  webview.setAttribute('src', dataUrl);
  document.body.appendChild(webview);
}

function testRemoveWebviewAfterNavigation() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);
  webview.src = 'data:text/html,trigger navigation';
  document.body.removeChild(webview);
  setTimeout(function() {
    embedder.test.succeed();
  }, 0);
}

// This test verifies that a <webview> is torn down gracefully when removed from
// the DOM on exit.
window.removeWebviewOnExitDoCrash = null;
function testRemoveWebviewOnExit() {
  var triggerNavUrl = 'data:text/html,trigger navigation';
  var webview = document.createElement('webview');

  webview.addEventListener('loadstop', function(e) {
    chrome.test.sendMessage('guest-loaded');
  });

  window.removeWebviewOnExitDoCrash = function() {
    webview.terminate();
  };

  webview.addEventListener('exit', function(e) {
    // We expected to be killed.
    if (e.reason != 'killed') {
      return;
    }
    webview.parentNode.removeChild(webview);
  });

  // Trigger a navigation to create a guest process.
  webview.setAttribute('src', embedder.emptyGuestURL);
  document.body.appendChild(webview);
}

function testResizeWebviewResizesContent() {
  var webview = document.createElement('webview');
  webview.src = 'about:blank';
  webview.addEventListener('loadstop', function(e) {
    webview.executeScript(
      {file: 'inject_resize_test.js'},
      function(results) {
        window.console.log('The resize test has been injected into webview.');
      }
    );
    webview.executeScript(
      {file: 'inject_comm_channel.js'},
      function(results) {
        window.console.log('The guest script for a two-way comm channel has ' +
            'been injected into webview.');
        // Establish a communication channel with the guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      }
    );
  });
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('A communication channel has been established with webview.');
      console.log('Resizing <webview> width from 300px to 400px.');
      webview.style.width = '400px';
      return;
    }
    if (data[0] == 'resize') {
      var width = data[1];
      var height = data[2];
      embedder.test.assertEq(400, width);
      embedder.test.assertEq(300, height);
      embedder.test.succeed();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });
  document.body.appendChild(webview);
}

// This test calls terminate() on guest after it has already been
// terminated. This makes sure we ignore the call gracefully.
function testTerminateAfterExit() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var loadstopSucceedsTest = false;
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq('loadstop', evt.type);
    if (loadstopSucceedsTest) {
      embedder.test.succeed();
      return;
    }

    webview.terminate();
  });

  webview.addEventListener('exit', function(evt) {
    embedder.test.assertEq('exit', evt.type);
    // Call terminate again.
    webview.terminate();
    // Load another page. The test would pass when loadstop is called on
    // this second page. This would hopefully catch if call to
    // webview.terminate() caused a browser crash.
    setTimeout(function() {
      loadstopSucceedsTest = true;
      webview.setAttribute('src', 'data:text/html,test second page');
    }, 0);
  });

  webview.setAttribute('src', 'data:text/html,test terminate() crash.');
  document.body.appendChild(webview);
}

// This test verifies that the WebRequest API onBeforeRequest event fires on
// webview.
function testWebRequestAPI() {
  var webview = new WebView();
  webview.request.onBeforeRequest.addListener(function(e) {
    embedder.test.succeed();
  }, { urls: ['<all_urls>']}) ;
  webview.src = embedder.windowOpenGuestURL;
  document.body.appendChild(webview);
}

// This test verifies that the WebRequest API onBeforeSendHeaders event fires on
// webview and supports headers. This tests verifies that we can modify HTTP
// headers via the WebRequest API and those modified headers will be sent to the
// HTTP server.
function testWebRequestAPIWithHeaders() {
  var webview = new WebView();
  var requestFilter = {
    urls: ['<all_urls>']
  };
  var extraInfoSpec = ['requestHeaders', 'blocking'];
  webview.request.onBeforeSendHeaders.addListener(function(details) {
    var headers = details.requestHeaders;
    for( var i = 0, l = headers.length; i < l; ++i ) {
      if (headers[i].name.toLowerCase() == 'user-agent') {
        headers[i].value = 'foobar';
        break;
      }
    }
    var blockingResponse = {};
    blockingResponse.requestHeaders = headers;
    return blockingResponse;
  }, requestFilter, extraInfoSpec);

  var loadstartCalled = false;

  var listener = function() {
    webview.removeEventListener('loadstop', listener);
    // Now load the real URL for the test.
    webview.src = embedder.detectUserAgentURL;

    webview.addEventListener('loadstart', function(e) {
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertEq(embedder.detectUserAgentURL, e.url);
      loadstartCalled = true;
    });

    webview.addEventListener('loadredirect', function(e) {
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertEq(embedder.detectUserAgentURL,
          e.oldUrl.replace('127.0.0.1', 'localhost'));
      embedder.test.assertEq(embedder.redirectGuestURLDest,
          e.newUrl.replace('127.0.0.1', 'localhost'));
      if (loadstartCalled) {
        embedder.test.succeed();
      } else {
        embedder.test.fail();
      }
    });
  };
  webview.addEventListener('loadstop', listener);

  // Load an empty URL to wait for the webRequest listener to be set up.
  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// Tests web request api support for "extraHeaders" with webviews. Regression
// test for crbug.com/938095.
function testWebRequestAPIWithExtraHeaders() {
  var echoCookieUrl = embedder.baseGuestURL + '/echoheader?Cookie';
  var setCookieUrl = embedder.baseGuestURL + '/set-cookie?foo=bar';

  var webview = new WebView();

  var requestFilter = {urls: [echoCookieUrl]};
  webview.request.onBeforeSendHeaders.addListener(function(details) {
    var cookieHeader = details.requestHeaders.find(function(header) {
      return header.name.toLowerCase() == 'cookie';
    });
    embedder.test.assertTrue(cookieHeader);
    embedder.test.assertEq('foo=bar', cookieHeader.value);
    // Modify the Cookie header.
    cookieHeader.value = 'foo=new_value';
    return {requestHeaders: details.requestHeaders};
  }, requestFilter, ['requestHeaders', 'blocking', 'extraHeaders']);

  var onSendHeadersSeen = false;
  webview.request.onSendHeaders.addListener(function(details) {
    var cookieHeader = details.requestHeaders.find(function(header) {
      return header.name.toLowerCase() == 'cookie';
    });
    embedder.test.assertTrue(cookieHeader);
    onSendHeadersSeen = true;
    // Verify the Cookie header was modified.
    chrome.test.assertEq('foo=new_value', cookieHeader.value);
  }, requestFilter, ['requestHeaders', 'extraHeaders']);

  var listener = function() {
    webview.removeEventListener('loadstop', listener);

    webview.addEventListener('loadstart', function(e) {
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertEq(echoCookieUrl, e.url);
    });

    webview.addEventListener('loadstop', function() {
      embedder.test.assertTrue(onSendHeadersSeen);

      // Ensure the header was modified.
      webview.executeScript(
          {code: 'document.body.innerText'}, function(results) {
            embedder.test.assertEq('foo=new_value', results[0]);
            embedder.test.succeed();
          });
    });

    // Now load a url to echo the cookie header.
    webview.src = echoCookieUrl;
  };
  webview.addEventListener('loadstop', listener);

  // Load a URL to set a cookie so that the Cookie header is set for future
  // requests.
  webview.src = setCookieUrl;
  document.body.appendChild(webview);
}

function testWebRequestAPIExistence() {
  var regularEventsToCheck = [
    // Declarative WebRequest API.
    'onMessage',
    // WebRequest API.
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onAuthRequired',
    'onBeforeRedirect',
    'onResponseStarted',
    'onCompleted',
    'onErrorOccurred'
  ];
  var declarativeEventsToCheck = [
    'onRequest',
  ];
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function(e) {
    for (var i = 0; i < regularEventsToCheck.length; ++i) {
      var eventName = regularEventsToCheck[i];
      var event = webview.request[eventName];
      embedder.test.assertEq('object', typeof event);
      embedder.test.assertEq('function', typeof event.addListener);
    }
    for (var i = 0; i < declarativeEventsToCheck.length; ++i) {
      var eventName = declarativeEventsToCheck[i];
      var event = webview.request[eventName];
      embedder.test.assertEq('function', typeof event.addRules);
      embedder.test.assertEq('function', typeof event.getRules);
      embedder.test.assertEq('function', typeof event.removeRules);
    }

    // Try to overwrite webview.request, shall not succeed.
    webview.request = '123';
    embedder.test.assertTrue(typeof webview.request !== 'string');

    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,webview check api');
  document.body.appendChild(webview);
}

// This test verifies that the WebRequest API onBeforeRequest event fires on
// clients*.google.com URLs.
function testWebRequestAPIGoogleProperty() {
  var webview = new WebView();
  webview.request.onBeforeRequest.addListener(function(e) {
    embedder.test.succeed();
    return {cancel: true};
  }, { urls: ['<all_urls>']}, ['blocking']) ;
  webview.src = 'http://clients6.google.com';
  document.body.appendChild(webview);
}

// This is a basic test to verify that image data is returned by
// captureVisibleRegion().
function testCaptureVisibleRegion() {
  var webview = document.createElement('webview');
  webview.setAttribute('src', 'data:text/html,webview test');

  webview.addEventListener('loadstop', function(e) {
    webview.captureVisibleRegion(
        {},
        function(imgdata) {
          if (chrome.runtime.lastError) {
            console.log(
                'webview.apitest.testCaptureVisibleRegion: ' +
                chrome.runtime.lastError.message);
            embedder.test.fail();
          } else {
            if (!imgdata.startsWith('data:image/jpeg;base64'))
              console.log('imgdata = ' + imgdata);

            embedder.test.assertTrue(
                imgdata.startsWith('data:image/jpeg;base64'));
            embedder.test.succeed();
          }
        });
  });
  document.body.appendChild(webview);
}

function captureVisibleRegionDoCapture() {}

// Ensure we use the closed encapsulation mode for the guest view shadow DOM
// to prevent script from interfering with our internal elements and producing
// unexpected behaviour.
function testClosedShadowRoot() {
  // Script could overwrite attachShadow to ignore the provided encapsulation
  // mode. Ensure this does not happen when creating the guest view shadow
  // DOM.
  Element.prototype.realAttachShadow = Element.prototype.attachShadow;
  Element.prototype.attachShadow = function() {
    window.console.log('Tainted attachShadow was called.');
    embedder.test.fail();
    return this.realAttachShadow({mode: 'open'});
  };

  var webview = document.createElement('webview');
  webview.src = 'data:text/html,webview test'
  webview.addEventListener('loadstop', () => {
    embedder.test.assertFalse(webview.shadowRoot);
    embedder.test.succeed();
  });
  document.body.appendChild(webview);
}

// Tests end.

embedder.test.testList = {
  'testAllowTransparencyAttribute': testAllowTransparencyAttribute,
  'testAPIMethodExistence': testAPIMethodExistence,
  'testCustomElementCallbacksInaccessible':
      testCustomElementCallbacksInaccessible,
  'testAssignSrcAfterCrash': testAssignSrcAfterCrash,
  'testAutosizeAfterNavigation': testAutosizeAfterNavigation,
  'testAutosizeBeforeNavigation': testAutosizeBeforeNavigation,
  'testAutosizeHeight': testAutosizeHeight,
  'testAutosizeRemoveAttributes': testAutosizeRemoveAttributes,
  'testAutosizeWithPartialAttributes': testAutosizeWithPartialAttributes,
  'testCanGoBack': testCanGoBack,
  'testCannotMutateEventName': testCannotMutateEventName,
  'testChromeExtensionRelativePath': testChromeExtensionRelativePath,
  'testChromeExtensionURL': testChromeExtensionURL,
  'testContentInitiatedNavigationToDataUrlBlocked':
      testContentInitiatedNavigationToDataUrlBlocked,
  'testContentLoadEvent': testContentLoadEvent,
  'testDeclarativeWebRequestAPI': testDeclarativeWebRequestAPI,
  'testDeclarativeWebRequestAPISendMessage':
      testDeclarativeWebRequestAPISendMessage,
  'testDestroyOnEventListener': testDestroyOnEventListener,
  'testDisplayNoneWebviewLoad': testDisplayNoneWebviewLoad,
  'testDisplayNoneWebviewRemoveChild': testDisplayNoneWebviewRemoveChild,
  'testEventName': testEventName,
  'testExecuteScript': testExecuteScript,
  'testExecuteScriptFail': testExecuteScriptFail,
  'testExecuteScriptIsAbortedWhenWebViewSourceIsChanged':
      testExecuteScriptIsAbortedWhenWebViewSourceIsChanged,
  'testFindAPI': testFindAPI,
  'testFindAPI_findupdate': testFindAPI,
  'testGetProcessId': testGetProcessId,
  'testHiddenBeforeNavigation': testHiddenBeforeNavigation,
  'testInlineScriptFromAccessibleResources':
      testInlineScriptFromAccessibleResources,
  'testInvalidChromeExtensionURL': testInvalidChromeExtensionURL,
  'testLoadAbortChromeExtensionURLWrongPartition':
      testLoadAbortChromeExtensionURLWrongPartition,
  'testLoadAbortEmptyResponse': testLoadAbortEmptyResponse,
  'testLoadAbortIllegalChromeURL': testLoadAbortIllegalChromeURL,
  'testLoadAbortIllegalFileURL': testLoadAbortIllegalFileURL,
  'testLoadAbortIllegalJavaScriptURL': testLoadAbortIllegalJavaScriptURL,
  'testLoadAbortInvalidNavigation': testLoadAbortInvalidNavigation,
  'testLoadAbortNonWebSafeScheme': testLoadAbortNonWebSafeScheme,
  'testLoadAbortUnknownScheme': testLoadAbortUnknownScheme,
  'testLoadEventsSameDocumentNavigation': testLoadEventsSameDocumentNavigation,
  'testLoadProgressEvent': testLoadProgressEvent,
  'testLoadStartLoadRedirect': testLoadStartLoadRedirect,
  'testNavigateAfterResize': testNavigateAfterResize,
  'testNavOnConsecutiveSrcAttributeChanges':
      testNavOnConsecutiveSrcAttributeChanges,
  'testNavOnSrcAttributeChange': testNavOnSrcAttributeChange,
  'testNewWindow': testNewWindow,
  'testNewWindowNoPreventDefault': testNewWindowNoPreventDefault,
  'testNewWindowNoReferrerLink': testNewWindowNoReferrerLink,
  'testNewWindowTwoListeners': testNewWindowTwoListeners,
  'testOnEventProperties': testOnEventProperties,
  'testPartitionChangeAfterNavigation': testPartitionChangeAfterNavigation,
  'testPartitionRemovalAfterNavigationFails':
      testPartitionRemovalAfterNavigationFails,
  'testReassignSrcAttribute': testReassignSrcAttribute,
  'testReload': testReload,
  'testReloadAfterTerminate': testReloadAfterTerminate,
  'testRemoveSrcAttribute': testRemoveSrcAttribute,
  'testRemoveWebviewAfterNavigation': testRemoveWebviewAfterNavigation,
  'testRemoveWebviewOnExit': testRemoveWebviewOnExit,
  'testResizeWebviewResizesContent': testResizeWebviewResizesContent,
  'testTerminateAfterExit': testTerminateAfterExit,
  'testWebRequestAPI': testWebRequestAPI,
  'testWebRequestAPIWithHeaders': testWebRequestAPIWithHeaders,
  'testWebRequestAPIWithExtraHeaders': testWebRequestAPIWithExtraHeaders,
  'testWebRequestAPIExistence': testWebRequestAPIExistence,
  'testWebRequestAPIGoogleProperty': testWebRequestAPIGoogleProperty,
  'testCaptureVisibleRegion': testCaptureVisibleRegion,
  'testClosedShadowRoot': testClosedShadowRoot,
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage('LAUNCHED');
  });
};
