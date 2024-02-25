// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scripts to allow page console.log() etc. output to be seen on the console
// of the host application.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

declare type LogLevel = 'log' | 'debug' | 'info' | 'warn' | 'error';

function sendConsoleMessage(log_level: LogLevel, originalArgs: unknown[]) {
  let message, slicedArgs = Array.prototype.slice.call(originalArgs);
  try {
    message = slicedArgs.join(' ');
  } catch (err) {
  }
  sendWebKitMessage('ConsoleMessageHandler', {
    'sender_frame' : gCrWeb.message.getFrameId(),
    'log_level' : log_level,
    'message' : message,
    'url': document.location.href
  });
}

const originalConsoleLog = console.log;
console.log = function() {
  const originalArgs = arguments as unknown as unknown[];
  sendConsoleMessage('log', originalArgs);
  return originalConsoleLog.apply(this, originalArgs);
};

const originalConsoleDebug = console.debug;
console.debug = function() {
  const originalArgs = arguments as unknown as unknown[];
  sendConsoleMessage('debug', originalArgs);
  return originalConsoleDebug.apply(this, originalArgs);
};

const originalConsoleInfo = console.info;
console.info = function() {
  const originalArgs = arguments as unknown as unknown[];
  sendConsoleMessage('info', originalArgs);
  return originalConsoleInfo.apply(this, originalArgs);
};

const originalConsoleWarn = console.warn;
console.warn = function() {
  const originalArgs = arguments as unknown as unknown[];
  sendConsoleMessage('warn', originalArgs);
  return originalConsoleWarn.apply(this, originalArgs);
};

const originalConsoleError = console.error;
console.error = function() {
  const originalArgs = arguments as unknown as unknown[];
  sendConsoleMessage('error', originalArgs);
  return originalConsoleError.apply(this, originalArgs);
};
