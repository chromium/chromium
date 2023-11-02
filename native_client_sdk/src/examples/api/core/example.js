// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var itrMax = 1000;
var itrCount = 0;
var itrSend = new Float64Array(itrMax);
var itrNaCl = new Float64Array(itrMax);
var itrRecv = new Float64Array(itrMax);
var delay = 0;

function getTimeInMilliseconds() {
  return (new Date()).getTime();
}

function attachListeners() {
  document.getElementById('start').addEventListener('click', startTest);
  countEl = document.getElementById('count');
  countEl.textContent = itrMax;
}

function startTest() {
  if (common.naclModule) {
    var startEl = document.getElementById('start');
    startEl.disabled = true;

    var delayEl = document.getElementById('delay');
    delay = parseInt(delayEl.value, 10);

    common.updateStatus('Running Test');
    itrCount = 0;
    itrSend[0] = getTimeInMilliseconds();
    common.naclModule.postMessage(delay);
  }
}

function setStats(nacl, compute, total) {
  var statNaClEl = document.getElementById('NaCl');
  var statRoundEl = document.getElementById('Round');
  var statTotalEl = document.getElementById('Total');

  statNaClEl.textContent = (nacl / itrMax) + ' ms';
  statRoundEl.textContent = (compute / itrMax) + ' ms';
  statTotalEl.textContent = (total / itrMax) + ' ms';
}

// Called by the common.js module.
function handleMessage(message_event) {
  // Convert NaCl Seconds elapsed to MS
  itrNaCl[itrCount] = message_event.data * 1000.0;
  itrRecv[itrCount] = getTimeInMilliseconds();
  itrCount++;

  if (itrCount === itrMax) {
    common.updateStatus('Test Finished');
    var startEl = document.getElementById('start');
    startEl.disabled = false;

    var naclMS = 0.0;
    var computeMS = 0.0;
    for (var i = 0; i < itrMax; i++) {
      naclMS += itrNaCl[i];
      computeMS += itrRecv[i] - itrSend[i];
    }

    setStats(naclMS, computeMS, itrRecv[itrMax - 1] - itrSend[0]);
  } else {
    itrSend[itrCount] = getTimeInMilliseconds();
    common.naclModule.postMessage(delay);
  }
}
