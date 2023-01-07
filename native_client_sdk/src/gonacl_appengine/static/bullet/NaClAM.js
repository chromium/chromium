// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NaClAMMessage() {
  this.header = {};
  this.frames = new Array();
}

NaClAMMessage.prototype.reset = function() {
  this.header = {};
  this.frames = new Array();
}

function NaClAM(embedId) {
  this.embedId = embedId;
  this.requestId = 0;
  this.message = new NaClAMMessage();
  this.state = 0;
  this.framesLeft = 0;
  this.listeners_ = Object.create(null);
  this.handleMesssage_ = this.handleMesssage_.bind(this);
}

NaClAM.prototype.enable = function() {
  window.addEventListener('message', this.handleMesssage_, true);
}

NaClAM.prototype.disable = function() {
  window.removeEventListener('message', this.handleMesssage_, true);
}

NaClAM.prototype.log_ = function(msg) {
  console.log(msg);
}

NaClAM.prototype.handleMesssage_ = function(event) {
  var STATE_WAITING_FOR_HEADER = 0;
  var STATE_COLLECTING_FRAMES = 1;
  if (this.state == STATE_WAITING_FOR_HEADER) {
    var header;
    try {
      header = JSON.parse(String(event.data));
    } catch (e) {
      console.log(e);
      console.log(event.data);
      return;
    }
    // Special case our log print command
    if (header['cmd'] == 'NaClAMPrint') {
      this.log_(header['print'])
        return;
    }
    if (typeof(header['request']) != "number") {
      console.log('Header message requestId is not a number.');
      return;
    }
    if (typeof(header['frames']) != "number") {
      console.log('Header message frames is not a number.');
      return;
    }
    this.framesLeft = header['frames'];
    this.state = STATE_COLLECTING_FRAMES;
    this.message.header = header;
  } else if (this.state == STATE_COLLECTING_FRAMES) {
    this.framesLeft--;
    this.message.frames.push(event.data);
  }
  if (this.state == STATE_COLLECTING_FRAMES && this.framesLeft == 0) {
    this.dispatchEvent(this.message);
    this.message.reset();
    this.state = STATE_WAITING_FOR_HEADER;
  }
}

NaClAM.prototype.messageHeaderIsValid_ = function(header) {
  if (header['cmd'] == undefined) {
    console.log('NaClAM: Message header does not contain cmd.');
    return false;
  }
  if (typeof(header['cmd']) != "string") {
    console.log('NaClAm: Message cmd is not a string.');
    return false;
  }
  if (header['frames'] == undefined) {
    console.log('NaClAM: Message header does not contain frames.');
    return false;
  }
  if (typeof(header['frames']) != "number") {
    console.log('NaClAm: Message frames is not a number.');
    return false;
  }
  if (header['request'] == undefined) {
    console.log('NaClAM: Message header does not contain request.');
    return false;
  }
  if (typeof(header['request']) != "number") {
    console.log('NaClAm: Message request is not a number.');
    return false;
  }
  return true;
}

NaClAM.prototype.framesIsValid_ = function(frames) {
  var i;
  if (!frames) {
    // No frames.
    return true;
  }
  if (Array.isArray(frames) == false) {
    console.log('NaClAM: Frames must be an array.');
    return false;
  }
  for (i = 0; i < frames.length; i++) {
    var e = frames[i];
    if (typeof(e) == "string") {
      continue;
    }
    if ((e instanceof ArrayBuffer) == false) {
      console.log('NaClAM: Frame is not a string or ArrayBuffer');
      return false;
    }
  }
  return true;
}

NaClAM.prototype.framesLength_ = function(frames) {
  if (!frames) {
    // No frames.
    return 0;
  }
  return frames.length;
}

NaClAM.prototype.sendMessage = function(cmdName, arguments, frames) {
  if (this.framesIsValid_(frames) == false) {
    console.log('NaClAM: Not sending message because frames is invalid.');
    return undefined;
  }
  var numFrames = this.framesLength_(frames);
  this.requestId++;
  var msgHeader = {
    cmd: cmdName,
    frames: numFrames,
    request: this.requestId,
    args: arguments
  };
  if (this.messageHeaderIsValid_(msgHeader) == false) {
    console.log('NaClAM: Not sending message because header is invalid.');
    return undefined;
  }
  var AM = document.getElementById(this.embedId);
  if (!AM) {
    console.log('NaClAM: Not sending message because Acceleration Module is not there.');
    return undefined;
  }
  AM.postMessage(JSON.stringify(msgHeader));
  var i;
  for (i = 0; i < numFrames; i++) {
    AM.postMessage(frames[i]);
  }
  return this.requestId;
}

/**
 * Adds an event listener to this Acceleration Module.
 * @param {string} type The name of the command.
 * @param handler The handler for the cmomand. This is called whenever the command is received.
 */
NaClAM.prototype.addEventListener = function(type, handler) {
  if (!this.listeners_) {
    this.listeners_ = Object.create(null);
  }
  if (!(type in this.listeners_)) {
    this.listeners_[type] = [handler];
  } else {
    var handlers = this.listeners_[type];
    if (handlers.indexOf(handler) < 0) {
      handlers.push(handler);
    }
  }
}

/**
 * Removes an event listener from this Acceleration Module.
 * @param {string} type The name of the command.
 * @param handler The handler for the cmomand. This is called whenever the command is received.
 */
NaClAM.prototype.removeEventListener = function(type, handler) {
  if (!this.listeners_) {
    // No listeners
    return;
  }
  if (type in this.listeners_) {
    var handlers = this.listeners_[type];
    var index = handlers.indexOf(handler);
    if (index >= 0) {
      if (handlers.length == 1) {
        // Listeners list would be empty, delete it
        delete this.listeners_[type];
      } else {
        // Remove the handler
        handlers.splice(index, 1);
      }
    }
  }
}

/**
 *
 */
NaClAM.prototype.dispatchEvent = function(event) {
  if (!this.listeners_) {
    return true;
  }
  var type = event.header.cmd;
  if (type in this.listeners_) {
    // Make a copy to walk over
    var handlers = this.listeners_[type].concat();
    for (var i = 0, handler; handler = handlers[i]; i++) {
      handler.call(this, event);
    }
  }
}
