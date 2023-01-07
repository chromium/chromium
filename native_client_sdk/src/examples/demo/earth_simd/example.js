// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
}

function postThreadFunc(numThreads) {
  return function () {
    common.naclModule.postMessage({'message' : 'set_threads',
                                   'value' : numThreads});
  }
}

// Add event listeners after the NaCl module has loaded.  These listeners will
// forward messages to the NaCl module via postMessage()
function attachListeners() {
  document.getElementById('benchmark').addEventListener('click',
    function() {
      common.naclModule.postMessage({'message' : 'run benchmark'});
      common.updateStatus('BENCHMARKING... (please wait)');
    });
  var threads = [0, 1, 2, 4, 6, 8, 12, 16, 24, 32];
  for (var i = 0; i < threads.length; i++) {
    document.getElementById('radio'+i).addEventListener('click',
        postThreadFunc(threads[i]));
  }
  document.getElementById('zoomRange').addEventListener('input',
    function() {
      var value = parseFloat(document.getElementById('zoomRange').value);
      common.naclModule.postMessage({'message' : 'set_zoom',
                                     'value' : value});
    });
  document.getElementById('lightRange').addEventListener('input',
    function() {
      var value = parseFloat(document.getElementById('lightRange').value);
      common.naclModule.postMessage({'message' : 'set_light',
                                     'value' : value});
    });
}

// Load a texture and send pixel data down to NaCl module.
function loadTexture(name) {
  // Load image from jpg, decompress into canvas.
  var img = new Image();
  img.onload = function() {
    var graph = document.createElement('canvas');
    graph.width = img.width;
    graph.height = img.height;
    var context = graph.getContext('2d');
    context.drawImage(img, 0, 0);
    var imageData = context.getImageData(0, 0, img.width, img.height);
    // Send NaCl module the raw image data obtained from canvas.
    common.naclModule.postMessage({'message' : 'texture',
                                   'name' : name,
                                   'width' : img.width,
                                   'height' : img.height,
                                   'data' : imageData.data.buffer});
  }
  img.src = name;
}

// Handle a message coming from the NaCl module.
function handleMessage(message_event) {
  if (message_event.data['message'] == 'benchmark_result') {
    // benchmark result
    var result = message_event.data['value'];
    console.log('Benchmark result:' + result);
    result = (Math.round(result * 1000) / 1000).toFixed(3);
    document.getElementById('result').textContent =
      'Result: ' + result + ' seconds';
    common.updateStatus('SUCCESS');
  } else if (message_event.data['message'] == 'set_zoom') {
    // zoom slider
    var zoom = message_event.data['value'];
    document.getElementById('zoomRange').value = zoom;
  } else if (message_event.data['message'] == 'set_light') {
    // light slider
    var light = message_event.data['value'];
    document.getElementById('lightRange').value = light;
  } else if (message_event.data['message'] == 'request_textures') {
    // NaCl module is requesting a set of textures.
    var names = message_event.data['names'];
    for (var i = 0; i < names.length; i++)
      loadTexture(names[i]);
  }
}

