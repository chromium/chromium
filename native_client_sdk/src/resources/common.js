// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set to true when the Document is loaded IFF "test=true" is in the query
// string.
var isTest = false;

// Set to true when loading a "Release" NaCl module, false when loading a
// "Debug" NaCl module.
var isRelease = true;

// Javascript module pattern:
//   see http://en.wikipedia.org/wiki/Unobtrusive_JavaScript#Namespaces
// In essence, we define an anonymous function which is immediately called and
// returns a new object. The new object contains only the exported definitions;
// all other definitions in the anonymous function are inaccessible to external
// code.
var common = (function() {

  function isHostToolchain(tool) {
    return tool == 'win' || tool == 'linux' || tool == 'mac';
  }

  /**
   * Return the mime type for NaCl plugin.
   *
   * @param {string} tool The name of the toolchain, e.g. "glibc", "newlib" etc.
   * @return {string} The mime-type for the kind of NaCl plugin matching
   * the given toolchain.
   */
  function mimeTypeForTool(tool) {
    // For NaCl modules use application/x-nacl.
    var mimetype = 'application/x-nacl';
    if (isHostToolchain(tool)) {
      // For non-NaCl PPAPI plugins use the x-ppapi-debug/release
      // mime type.
      if (isRelease)
        mimetype = 'application/x-ppapi-release';
      else
        mimetype = 'application/x-ppapi-debug';
    } else if (tool == 'pnacl') {
      mimetype = 'application/x-pnacl';
    }
    return mimetype;
  }

  /**
   * Check if the browser supports NaCl plugins.
   *
   * @param {string} tool The name of the toolchain, e.g. "glibc", "newlib" etc.
   * @return {bool} True if the browser supports the type of NaCl plugin
   * produced by the given toolchain.
   */
  function browserSupportsNaCl(tool) {
    // Assume host toolchains always work with the given browser.
    // The below mime-type checking might not work with
    // --register-pepper-plugins.
    if (isHostToolchain(tool)) {
      return true;
    }
    var mimetype = mimeTypeForTool(tool);
    return navigator.mimeTypes[mimetype] !== undefined;
  }

  /**
   * Inject a script into the DOM, and call a callback when it is loaded.
   *
   * @param {string} url The url of the script to load.
   * @param {Function} onload The callback to call when the script is loaded.
   * @param {Function} onerror The callback to call if the script fails to load.
   */
  function injectScript(url, onload, onerror) {
    var scriptEl = document.createElement('script');
    scriptEl.type = 'text/javascript';
    scriptEl.src = url;
    scriptEl.onload = onload;
    if (onerror) {
      scriptEl.addEventListener('error', onerror, false);
    }
    document.head.appendChild(scriptEl);
  }

  /**
   * Run all tests for this example.
   *
   * @param {Object} moduleEl The module DOM element.
   */
  function runTests(moduleEl) {
    console.log('runTests()');
    common.tester = new Tester();

    // All NaCl SDK examples are OK if the example exits cleanly; (i.e. the
    // NaCl module returns 0 or calls exit(0)).
    //
    // Without this exception, the browser_tester thinks that the module
    // has crashed.
    common.tester.exitCleanlyIsOK();

    common.tester.addAsyncTest('loaded', function(test) {
      test.pass();
    });

    if (typeof window.addTests !== 'undefined') {
      window.addTests();
    }

    common.tester.waitFor(moduleEl);
    common.tester.run();
  }

  /**
   * Create the Native Client <embed> element as a child of the DOM element
   * named "listener".
   *
   * @param {string} name The name of the example.
   * @param {string} tool The name of the toolchain, e.g. "glibc", "newlib" etc.
   * @param {string} path Directory name where .nmf file can be found.
   * @param {number} width The width to create the plugin.
   * @param {number} height The height to create the plugin.
   * @param {Object} attrs Dictionary of attributes to set on the module.
   */
  function createNaClModule(name, tool, path, width, height, attrs) {
    var moduleEl = document.createElement('embed');
    moduleEl.setAttribute('name', 'nacl_module');
    moduleEl.setAttribute('id', 'nacl_module');
    moduleEl.setAttribute('width', width);
    moduleEl.setAttribute('height', height);
    moduleEl.setAttribute('path', path);
    moduleEl.setAttribute('src', path + '/' + name + '.nmf');

    // Add any optional arguments
    if (attrs) {
      for (var key in attrs) {
        moduleEl.setAttribute(key, attrs[key]);
      }
    }

    var mimetype = mimeTypeForTool(tool);
    moduleEl.setAttribute('type', mimetype);

    // The <EMBED> element is wrapped inside a <DIV>, which has both a 'load'
    // and a 'message' event listener attached.  This wrapping method is used
    // instead of attaching the event listeners directly to the <EMBED> element
    // to ensure that the listeners are active before the NaCl module 'load'
    // event fires.
    var listenerDiv = document.getElementById('listener');
    listenerDiv.appendChild(moduleEl);

    // Request the offsetTop property to force a relayout. As of Apr 10, 2014
    // this is needed if the module is being loaded on a Chrome App's
    // background page (see crbug.com/350445).
    moduleEl.offsetTop;

    // Host plugins don't send a moduleDidLoad message. We'll fake it here.
    var isHost = isHostToolchain(tool);
    if (isHost) {
      window.setTimeout(function() {
        moduleEl.readyState = 1;
        moduleEl.dispatchEvent(new CustomEvent('loadstart'));
        moduleEl.readyState = 4;
        moduleEl.dispatchEvent(new CustomEvent('load'));
        moduleEl.dispatchEvent(new CustomEvent('loadend'));
      }, 100);  // 100 ms
    }

    // This is code that is only used to test the SDK.
    if (isTest) {
      var loadNaClTest = function() {
        injectScript('nacltest.js', function() {
          runTests(moduleEl);
        });
      };

      // Try to load test.js for the example. Whether or not it exists, load
      // nacltest.js.
      injectScript('test.js', loadNaClTest, loadNaClTest);
    }
  }

  /**
   * Add the default "load" and "message" event listeners to the element with
   * id "listener".
   *
   * The "load" event is sent when the module is successfully loaded. The
   * "message" event is sent when the naclModule posts a message using
   * PPB_Messaging.PostMessage() (in C) or pp::Instance().PostMessage() (in
   * C++).
   */
  function attachDefaultListeners() {
    var listenerDiv = document.getElementById('listener');
    listenerDiv.addEventListener('load', moduleDidLoad, true);
    listenerDiv.addEventListener('message', handleMessage, true);
    listenerDiv.addEventListener('error', handleError, true);
    listenerDiv.addEventListener('crash', handleCrash, true);
    if (typeof window.attachListeners !== 'undefined') {
      window.attachListeners();
    }
  }

  /**
   * Called when the NaCl module fails to load.
   *
   * This event listener is registered in createNaClModule above.
   */
  function handleError(event) {
    // We can't use common.naclModule yet because the module has not been
    // loaded.
    var moduleEl = document.getElementById('nacl_module');
    updateStatus('ERROR [' + moduleEl.lastError + ']');
  }

  /**
   * Called when the Browser can not communicate with the Module
   *
   * This event listener is registered in attachDefaultListeners above.
   */
  function handleCrash(event) {
    if (common.naclModule.exitStatus == -1) {
      updateStatus('CRASHED');
    } else {
      updateStatus('EXITED [' + common.naclModule.exitStatus + ']');
    }
    if (typeof window.handleCrash !== 'undefined') {
      window.handleCrash(common.naclModule.lastError);
    }
  }

  /**
   * Called when the NaCl module is loaded.
   *
   * This event listener is registered in attachDefaultListeners above.
   */
  function moduleDidLoad() {
    common.naclModule = document.getElementById('nacl_module');
    updateStatus('RUNNING');

    if (typeof window.moduleDidLoad !== 'undefined') {
      window.moduleDidLoad();
    }
  }

  /**
   * Hide the NaCl module's embed element.
   *
   * We don't want to hide by default; if we do, it is harder to determine that
   * a plugin failed to load. Instead, call this function inside the example's
   * "moduleDidLoad" function.
   *
   */
  function hideModule() {
    // Setting common.naclModule.style.display = "None" doesn't work; the
    // module will no longer be able to receive postMessages.
    common.naclModule.style.height = '0';
  }

  /**
   * Remove the NaCl module from the page.
   */
  function removeModule() {
    common.naclModule.parentNode.removeChild(common.naclModule);
    common.naclModule = null;
  }

  /**
   * Return true when |s| starts with the string |prefix|.
   *
   * @param {string} s The string to search.
   * @param {string} prefix The prefix to search for in |s|.
   */
  function startsWith(s, prefix) {
    // indexOf would search the entire string, lastIndexOf(p, 0) only checks at
    // the first index. See: http://stackoverflow.com/a/4579228
    return s.lastIndexOf(prefix, 0) === 0;
  }

  /** Maximum length of logMessageArray. */
  var kMaxLogMessageLength = 20;

  /** An array of messages to display in the element with id "log". */
  var logMessageArray = [];

  /**
   * Add a message to an element with id "log".
   *
   * This function is used by the default "log:" message handler.
   *
   * @param {string} message The message to log.
   */
  function logMessage(message) {
    logMessageArray.push(message);
    if (logMessageArray.length > kMaxLogMessageLength)
      logMessageArray.shift();

    document.getElementById('log').textContent = logMessageArray.join('\n');
    console.log(message);
  }

  /**
   */
  var defaultMessageTypes = {
    'alert': alert,
    'log': logMessage
  };

  /**
   * Called when the NaCl module sends a message to JavaScript (via
   * PPB_Messaging.PostMessage())
   *
   * This event listener is registered in createNaClModule above.
   *
   * @param {Event} message_event A message event. message_event.data contains
   *     the data sent from the NaCl module.
   */
  function handleMessage(message_event) {
    if (typeof message_event.data === 'string') {
      for (var type in defaultMessageTypes) {
        if (defaultMessageTypes.hasOwnProperty(type)) {
          if (startsWith(message_event.data, type + ':')) {
            func = defaultMessageTypes[type];
            func(message_event.data.slice(type.length + 1));
            return;
          }
        }
      }
    }

    if (typeof window.handleMessage !== 'undefined') {
      window.handleMessage(message_event);
      return;
    }

    logMessage('Unhandled message: ' + message_event.data);
  }

  /**
   * Called when the DOM content has loaded; i.e. the page's document is fully
   * parsed. At this point, we can safely query any elements in the document via
   * document.querySelector, document.getElementById, etc.
   *
   * @param {string} name The name of the example.
   * @param {string} tool The name of the toolchain, e.g. "glibc", "newlib" etc.
   * @param {string} path Directory name where .nmf file can be found.
   * @param {number} width The width to create the plugin.
   * @param {number} height The height to create the plugin.
   * @param {Object} attrs Optional dictionary of additional attributes.
   */
  function domContentLoaded(name, tool, path, width, height, attrs) {
    // If the page loads before the Native Client module loads, then set the
    // status message indicating that the module is still loading.  Otherwise,
    // do not change the status message.
    updateStatus('Page loaded.');
    if (!browserSupportsNaCl(tool)) {
      updateStatus(
          'Browser does not support NaCl (' + tool + '), or NaCl is disabled');
    } else if (common.naclModule == null) {
      updateStatus('Creating embed: ' + tool);

      // We use a non-zero sized embed to give Chrome space to place the bad
      // plug-in graphic, if there is a problem.
      width = typeof width !== 'undefined' ? width : 200;
      height = typeof height !== 'undefined' ? height : 200;
      attachDefaultListeners();
      createNaClModule(name, tool, path, width, height, attrs);
    } else {
      // It's possible that the Native Client module onload event fired
      // before the page's onload event.  In this case, the status message
      // will reflect 'SUCCESS', but won't be displayed.  This call will
      // display the current message.
      updateStatus('Waiting.');
    }
  }

  /** Saved text to display in the element with id 'statusField'. */
  var statusText = 'NO-STATUSES';

  /**
   * Set the global status message. If the element with id 'statusField'
   * exists, then set its HTML to the status message as well.
   *
   * @param {string} opt_message The message to set. If null or undefined, then
   *     set element 'statusField' to the message from the last call to
   *     updateStatus.
   */
  function updateStatus(opt_message) {
    if (opt_message) {
      statusText = opt_message;
    }
    var statusField = document.getElementById('statusField');
    if (statusField) {
      statusField.innerHTML = statusText;
    }
  }

  // The symbols to export.
  return {
    /** A reference to the NaCl module, once it is loaded. */
    naclModule: null,

    attachDefaultListeners: attachDefaultListeners,
    domContentLoaded: domContentLoaded,
    createNaClModule: createNaClModule,
    hideModule: hideModule,
    removeModule: removeModule,
    logMessage: logMessage,
    updateStatus: updateStatus
  };

}());

// Listen for the DOM content to be loaded. This event is fired when parsing of
// the page's document has finished.
document.addEventListener('DOMContentLoaded', function() {
  var body = document.body;

  // The data-* attributes on the body can be referenced via body.dataset.
  if (body.dataset) {
    var loadFunction;
    if (!body.dataset.customLoad) {
      loadFunction = common.domContentLoaded;
    } else if (typeof window.domContentLoaded !== 'undefined') {
      loadFunction = window.domContentLoaded;
    }

    // From https://developer.mozilla.org/en-US/docs/DOM/window.location
    var searchVars = {};
    if (window.location.search.length > 1) {
      var pairs = window.location.search.substr(1).split('&');
      for (var key_ix = 0; key_ix < pairs.length; key_ix++) {
        var keyValue = pairs[key_ix].split('=');
        searchVars[unescape(keyValue[0])] =
            keyValue.length > 1 ? unescape(keyValue[1]) : '';
      }
    }

    if (loadFunction) {
      var toolchains = body.dataset.tools.split(' ');
      var configs = body.dataset.configs.split(' ');

      var attrs = {};
      if (body.dataset.attrs) {
        var attr_list = body.dataset.attrs.split(' ');
        for (var key in attr_list) {
          var attr = attr_list[key].split('=');
          var key = attr[0];
          var value = attr[1];
          attrs[key] = value;
        }
      }

      var tc = toolchains.indexOf(searchVars.tc) !== -1 ?
          searchVars.tc : toolchains[0];

      // If the config value is included in the search vars, use that.
      // Otherwise default to Release if it is valid, or the first value if
      // Release is not valid.
      if (configs.indexOf(searchVars.config) !== -1)
        var config = searchVars.config;
      else if (configs.indexOf('Release') !== -1)
        var config = 'Release';
      else
        var config = configs[0];

      var pathFormat = body.dataset.path;
      var path = pathFormat.replace('{tc}', tc).replace('{config}', config);

      isTest = searchVars.test === 'true';
      isRelease = path.toLowerCase().indexOf('release') != -1;

      loadFunction(body.dataset.name, tc, path, body.dataset.width,
                   body.dataset.height, attrs);
    }
  }
});
