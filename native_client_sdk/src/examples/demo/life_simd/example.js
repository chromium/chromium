// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
}


// Add event listeners after the NaCl module has loaded.  These listeners will
// forward messages to the NaCl module via postMessage()
function attachListeners() {
  document.getElementById('benchmark').addEventListener('click',
    function() {
      common.naclModule.postMessage({'message' : 'run_benchmark'});
      common.updateStatus('BENCHMARKING... (please wait)');
    });
  document.getElementById('simd').addEventListener('click',
    function() {
      var simd = document.getElementById('simd');
      common.naclModule.postMessage({'message' : 'set_simd',
          'value' : simd.checked});
    });
  document.getElementById('multithread').addEventListener('click',
    function() {
      var multithread = document.getElementById('multithread');
      common.naclModule.postMessage({'message' : 'set_threading',
          'value' : multithread.checked});
    });
  document.getElementById('large').addEventListener('click',
    function() {
      var large = document.getElementById('large');
      var nacl = document.getElementById('nacl_module');
      nacl.setAttribute('width', large.checked ? 1280 : 640);
      nacl.setAttribute('height', large.checked ? 1024 : 640);
    });
}


// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  if (message_event.data.message == 'benchmark_result') {
    // benchmark result
    var result = message_event.data.value;
    console.log('Benchmark result:' + result);
    result = (Math.round(result * 1000) / 1000).toFixed(3);
    document.getElementById('result').textContent =
      'Result: ' + result + ' seconds';
    common.updateStatus('SUCCESS');
  }
}

