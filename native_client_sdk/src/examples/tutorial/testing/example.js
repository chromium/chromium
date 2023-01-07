// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
}

var currentTestEl = null;

function startCommand(testName) {
  var testListEl = document.getElementById('tests');
  var testEl = document.createElement('li');
  var testRowEl = document.createElement('div');
  var testNameEl = document.createElement('span');
  var testResultEl = document.createElement('span');
  testRowEl.classList.add('row');
  testNameEl.classList.add('name');
  testNameEl.textContent = testName;
  testResultEl.classList.add('result');
  testRowEl.appendChild(testNameEl);
  testRowEl.appendChild(testResultEl);
  testEl.appendChild(testRowEl);
  testListEl.appendChild(testEl);

  currentTestEl = testEl;
}

function failCommand(fileName, lineNumber, summary) {
  var testMessageEl = document.createElement('pre');
  testMessageEl.textContent += fileName + ':' + lineNumber + ': ' + summary;
  currentTestEl.appendChild(testMessageEl);
}

function endCommand(testName, testResult) {
  var testRowEl = currentTestEl.querySelector('.row');
  var testResultEl = currentTestEl.querySelector('.result');
  testRowEl.classList.add(testResult);
  testResultEl.textContent = testResult;
}

function handleMessage(event) {
  var msg = event.data;
  var firstColon = msg.indexOf(':');
  var cmd = msg.substr(0, firstColon);

  if (cmd == 'testend') {
    event.srcElement.postMessage({'testend' : ''});
    return;
  }

  var cmdFunctionName = cmd + 'Command';
  var cmdFunction = window[cmdFunctionName];
  if (typeof(cmdFunction) !== 'function') {
    console.log('Unknown command: ' + cmd);
    console.log('  message: ' + msg);
    return;
  }

  var argCount = cmdFunction.length;

  // Don't use split, because it will split all commas (for example any commas
  // in the test failure summary).
  var argList = msg.substr(firstColon + 1);
  args = [];
  for (var i = 0; i < argCount - 1; ++i) {
    var arg;
    var comma = argList.indexOf(',');
    if (comma === -1) {
      if (i !== argCount - 1) {
        console.log('Bad arg count to command "' + cmd + '", expected ' +
                    argCount);
        console.log('  message: ' + msg);
      } else {
        arg = argList;
      }
    } else {
      arg = argList.substr(0, comma);
      argList = argList.substr(comma + 1);
    }
    args.push(arg);
  }

  // Last argument is the rest of the message.
  args.push(argList);

  cmdFunction.apply(null, args);
}
