// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function moduleDidLoad() {
  common.hideModule();
}

function $(id) {
  return document.getElementById(id);
}

// Called by the common.js module.
function domContentLoaded(name, tc, config, width, height) {
  navigator.webkitPersistentStorage.requestQuota(5 * 1024 * 1024,
      function(bytes) {
        common.updateStatus(
            'Allocated ' + bytes + ' bytes of persistent storage.');
        common.attachDefaultListeners();
        common.createNaClModule(name, tc, config, width, height);
      },
      function(e) { alert('Failed to allocate space') });
}

// Called by the common.js module.
function attachListeners() {
  var radioEls = document.querySelectorAll('input[type="radio"]');
  for (var i = 0; i < radioEls.length; ++i) {
    radioEls[i].addEventListener('click', onRadioClicked);
  }

  // Wire up the 'click' event for each function's button.
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var functionEl = functionEls[i];
    var id = functionEl.getAttribute('id');
    var buttonEl = functionEl.querySelector('button');

    // The function name matches the element id.
    var func = window[id];
    buttonEl.addEventListener('click', func);
  }

  $('pipe_input_box').addEventListener('keypress', onPipeInput)
  $('pipe_output').disabled = true;

  $('pipe_name').addEventListener('change',
                                  function() { $('pipe_output').value = ''; })
}

// Called with keypress events on the pipe input box
function onPipeInput(e) {
  // Create an arraybuffer containing the 16-bit char code
  // from the keypress event.
  var buffer = new ArrayBuffer(1*2);
  var bufferView = new Uint16Array(buffer);
  bufferView[0] = e.charCode;

  // Pass the buffer in a dictionary over the NaCl module
  var pipeSelect = $('pipe_name');
  var pipeName = pipeSelect[pipeSelect.selectedIndex].value;
  var message = {
    pipe: pipeName,
    operation: 'write',
    payload: buffer,
  };
  nacl_module.postMessage(message);
  e.preventDefault();
  return false;
}

function onRadioClicked(e) {
  var divId = this.id.slice(5);  // skip "radio"
  var functionEls = document.querySelectorAll('.function');
  for (var i = 0; i < functionEls.length; ++i) {
    var visible = functionEls[i].id === divId;
    if (functionEls[i].id === divId)
      functionEls[i].removeAttribute('hidden');
    else
      functionEls[i].setAttribute('hidden', '');
  }
}

function addNameToSelectElements(cssClass, handle, name) {
  var text = '[' + handle + '] ' + name;
  var selectEls = document.querySelectorAll(cssClass);
  for (var i = 0; i < selectEls.length; ++i) {
    var optionEl = document.createElement('option');
    optionEl.setAttribute('value', handle);
    optionEl.appendChild(document.createTextNode(text));
    selectEls[i].appendChild(optionEl);
  }
}

function removeNameFromSelectElements(cssClass, handle) {
  var optionEls = document.querySelectorAll(cssClass + ' > option');
  for (var i = 0; i < optionEls.length; ++i) {
    var optionEl = optionEls[i];
    if (optionEl.value == handle) {
      var selectEl = optionEl.parentNode;
      selectEl.removeChild(optionEl);
    }
  }
}

var filehandle_map = {};
var dirhandle_map = {};

function fopen(e) {
  var filename = $('fopenFilename').value;
  var access = $('fopenMode').value;
  postCall('fopen', filename, access, function(filename, filehandle) {
    filehandle_map[filehandle] = filename;

    addNameToSelectElements('.file-handle', filehandle, filename);
    common.logMessage('File ' + filename + ' opened successfully.');
  });
}

function fclose(e) {
  var filehandle = parseInt($('fcloseHandle').value, 10);
  postCall('fclose', filehandle, function(filehandle) {
    var filename = filehandle_map[filehandle];
    removeNameFromSelectElements('.file-handle', filehandle, filename);
    common.logMessage('File ' + filename + ' closed successfully.');
  });
}

function fread(e) {
  var filehandle = parseInt($('freadHandle').value, 10);
  var numBytes = parseInt($('freadBytes').value, 10);
  postCall('fread', filehandle, numBytes, function(filehandle, data) {
    var filename = filehandle_map[filehandle];
    common.logMessage('Read "' + data + '" from file ' + filename + '.');
  });
}

function fwrite(e) {
  var filehandle = parseInt($('fwriteHandle').value, 10);
  var data = $('fwriteData').value;
  postCall('fwrite', filehandle, data, function(filehandle, bytesWritten) {
    var filename = filehandle_map[filehandle];
    common.logMessage('Wrote ' + bytesWritten + ' bytes to file ' + filename +
        '.');
  });
}

function fseek(e) {
  var filehandle = parseInt($('fseekHandle').value, 10);
  var offset = parseInt($('fseekOffset').value, 10);
  var whence = parseInt($('fseekWhence').value, 10);
  postCall('fseek', filehandle, offset, whence, function(filehandle, filepos) {
    var filename = filehandle_map[filehandle];
    common.logMessage('Seeked to location ' + filepos + ' in file ' + filename +
        '.');
  });
}

function fflush(e) {
  var filehandle = parseInt($('fflushHandle').value, 10);
  postCall('fflush', filehandle, function(filehandle, filepos) {
    var filename = filehandle_map[filehandle];
    common.logMessage('flushed ' + filename + '.');
  });
}

function stat(e) {
  var filename = $('statFilename').value;
  postCall('stat', filename, function(filename, size) {
    common.logMessage('File ' + filename + ' has size ' + size + '.');
  });
}

function opendir(e) {
  var dirname = $('opendirDirname').value;
  postCall('opendir', dirname, function(dirname, dirhandle) {
    dirhandle_map[dirhandle] = dirname;

    addNameToSelectElements('.dir-handle', dirhandle, dirname);
    common.logMessage('Directory ' + dirname + ' opened successfully.');
  });
}

function readdir(e) {
  var dirhandle = parseInt($('readdirHandle').value, 10);
  postCall('readdir', dirhandle, function(dirhandle, ino, name) {
    var dirname = dirhandle_map[dirhandle];
    if (ino === undefined) {
      common.logMessage('End of directory.');
    } else {
      common.logMessage('Read entry ("' + name + '", ino = ' + ino +
                        ') from directory ' + dirname + '.');
    }
  });
}

function closedir(e) {
  var dirhandle = parseInt($('closedirHandle').value, 10);
  postCall('closedir', dirhandle, function(dirhandle) {
    var dirname = dirhandle_map[dirhandle];
    delete dirhandle_map[dirhandle];

    removeNameFromSelectElements('.dir-handle', dirhandle, dirname);
    common.logMessage('Directory ' + dirname + ' closed successfully.');
  });
}

function mkdir(e) {
  var dirname = $('mkdirDirname').value;
  var mode = parseInt($('mkdirMode').value, 10);
  postCall('mkdir', dirname, mode, function(dirname) {
    common.logMessage('Directory ' + dirname + ' created successfully.');
  });
}

function rmdir(e) {
  var dirname = $('rmdirDirname').value;
  postCall('rmdir', dirname, function(dirname) {
    common.logMessage('Directory ' + dirname + ' removed successfully.');
  });
}

function chdir(e) {
  var dirname = $('chdirDirname').value;
  postCall('chdir', dirname, function(dirname) {
    common.logMessage('Changed directory to: ' + dirname + '.');
  });
}

function getcwd(e) {
  postCall('getcwd', function(dirname) {
    common.logMessage('getcwd: ' + dirname + '.');
  });
}

function getaddrinfo(e) {
  var name = $('getaddrinfoName').value;
  var family = $('getaddrinfoFamily').value;
  postCall('getaddrinfo', name, family, function(name, addrType) {
    common.logMessage('getaddrinfo returned successfully');
    common.logMessage('ai_cannonname = ' + name + '.');
    var count = 1;
    for (var i = 1; i < arguments.length; i+=2) {
      var msg = 'Address number ' + count + ' = ' + arguments[i] +
                ' (' + arguments[i+1] + ')';
      common.logMessage(msg);
      count += 1;
    }
  });
}

function gethostbyname(e) {
  var name = $('gethostbynameName').value;
  postCall('gethostbyname', name, function(name, addrType) {
    common.logMessage('gethostbyname returned successfully');
    common.logMessage('h_name = ' + name + '.');
    common.logMessage('h_addr_type = ' + addrType + '.');
    for (var i = 2; i < arguments.length; i++) {
      common.logMessage('Address number ' + (i-1) + ' = ' + arguments[i] + '.');
    }
  });
}

function connect(e) {
  var host = $('connectHost').value;
  var port = parseInt($('connectPort').value, 10);
  postCall('connect', host, port, function(sockhandle) {
    common.logMessage('connected');
    addNameToSelectElements('.sock-handle', sockhandle, '[socket]');
  });
}

function recv(e) {
  var handle = parseInt($('recvHandle').value, 10);
  var bufferSize = parseInt($('recvBufferSize').value, 10);
  postCall('recv', handle, bufferSize, function(messageLen, message) {
    common.logMessage("received " + messageLen + ' bytes: ' + message);
  });
}

function send(e) {
  var handle = parseInt($('sendHandle').value, 10);
  var message = $('sendMessage').value;
  postCall('send', handle, message, function(sentBytes) {
    common.logMessage("sent bytes: " + sentBytes);
  });
}

function close(e) {
  var handle = parseInt($('closeHandle').value, 10);
  postCall('close', handle, function(sock) {
    removeNameFromSelectElements('.sock-handle', sock, "[socket]");
    common.logMessage("closed socket: " + sock);
  });
}

var funcToCallback = {};

function postCall(func) {
  var callback = arguments[arguments.length - 1];
  funcToCallback[func] = callback;

  nacl_module.postMessage({
    cmd: func,
    args: Array.prototype.slice.call(arguments, 1, -1)
  });
}

function ArrayBufferToString(buf) {
  return String.fromCharCode.apply(null, new Uint16Array(buf));
}

// Called by the common.js module.
function handleMessage(message_event) {
  var data = message_event.data;
  if ((typeof(data) === 'string' || data instanceof String)) {
    common.logMessage(data);
  } else if (data instanceof Object) {
    var pipeName = data['pipe']
    if (pipeName !== undefined) {
      // Message for JavaScript I/O pipe
      var operation = data['operation'];
      if (operation == 'write') {
        $('pipe_output').value += ArrayBufferToString(data['payload']);
      } else if (operation == 'ack') {
        common.logMessage(pipeName + ": ack:" + data['payload']);
      } else {
        common.logMessage('Got unexpected pipe operation: ' + operation);
      }
    } else {
      // Result from a function call.
      var params = data.args;
      var funcName = data.cmd;
      var callback = funcToCallback[funcName];

      if (!callback) {
        common.logMessage('Error: Bad message ' + funcName +
                          ' received from NaCl module.');
        return;
      }

      delete funcToCallback[funcName];
      callback.apply(null, params);
    }
  } else {
    common.logMessage('Error: Unknow message `' + data +
                      '` received from NaCl module.');
  }
}
