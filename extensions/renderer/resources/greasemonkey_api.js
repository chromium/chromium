// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

// Partial implementation of the Greasemonkey API, see:
// http://wiki.greasespot.net/Greasemonkey_Manual:APIs

function GM_addStyle(css) {
  var parent = document.getElementsByTagName("head")[0];
  if (!parent) {
    parent = document.documentElement;
  }
  var style = document.createElement("style");
  style.type = "text/css";
  var textNode = document.createTextNode(css);
  style.appendChild(textNode);
  parent.appendChild(style);
}

function GM_xmlhttpRequest(details) {
  function setupEvent(xhr, url, eventName, callback) {
    xhr[eventName] = function () {
      var isComplete = xhr.readyState == 4;
      var responseState = {
        responseText: xhr.responseText,
        readyState: xhr.readyState,
        responseHeaders: isComplete ? xhr.getAllResponseHeaders() : "",
        status: isComplete ? xhr.status : 0,
        statusText: isComplete ? xhr.statusText : "",
        finalUrl: isComplete ? url : ""
      };
      callback(responseState);
    };
  }

  var xhr = new XMLHttpRequest();
  var eventNames = ["onload", "onerror", "onreadystatechange"];
  for (var i = 0; i < eventNames.length; i++ ) {
    var eventName = eventNames[i];
    if (eventName in details) {
      setupEvent(xhr, details.url, eventName, details[eventName]);
    }
  }

  xhr.open(details.method, details.url);

  if (details.overrideMimeType) {
    xhr.overrideMimeType(details.overrideMimeType);
  }
  if (details.headers) {
    for (var header in details.headers) {
      xhr.setRequestHeader(header, details.headers[header]);
    }
  }
  xhr.send(details.data ? details.data : null);
}

function GM_openInTab(url) {
  window.open(url, "");
}

function GM_log(message) {
  window.console.log(message);
}

(function() {
  function generateGreasemonkeyStub(name) {
    return function() {
      console.log("%s is not supported.", name);
    };
  }

  var apis = ["GM_getValue", "GM_setValue", "GM_registerMenuCommand"];
  for (var i = 0, api; api = apis[i]; i++) {
    window[api] = generateGreasemonkeyStub(api);
  }
})();
