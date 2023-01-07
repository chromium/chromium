/*
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

'use strict';

lib.rtdep('lib.f',
          'hterm');

// CSP means that we can't kick off the initialization from the html file,
// so we do it like this instead.
window.onload = function() {
  lib.init(function() {
    NaClTerm.init();
  });
};

/**
 * The hterm-powered terminal command.
 *
 * This class defines a command that can be run in an hterm.Terminal instance.
 *
 * @param {Object} argv The argument object passed in from the Terminal.
 */
function NaClTerm(argv) {
  this.io = argv.io.push();
  this.argv_ = argv;
};

var ansiCyan = '\x1b[36m';
var ansiReset = '\x1b[0m';

/**
 * Static initialier called from index.html.
 *
 * This constructs a new Terminal instance and instructs it to run the NaClTerm
 * command.
 */
NaClTerm.init = function() {
  var profileName = lib.f.parseQuery(document.location.search)['profile'];
  var terminal = new hterm.Terminal(profileName);
  terminal.decorate(document.querySelector('#terminal'));

  // Useful for console debugging.
  window.term_ = terminal;

  terminal.runCommandClass(NaClTerm, document.location.hash.substr(1));
  return true;
};

NaClTerm.prototype.updateStatus = function(message) {
  document.getElementById('statusField').textContent = message;
  this.io.print(message + '\n');
}

/**
 * Handle messages sent to us from NaCl.
 *
 * @private
 */
NaClTerm.prototype.handleMessage_ = function(e) {
  if (e.data.indexOf(NaClTerm.prefix) == 0) {
    var msg = e.data.substring(NaClTerm.prefix.length);
    if (!this.loaded) {
      this.bufferedOutput += msg;
    } else {
      this.io.print(msg);
    }
  } else if (e.data.indexOf('exited') == 0) {
    var exitCode = e.data.split(':', 2)[1]
    if (exitCode === undefined)
      exitCode = 0;
    this.exit(exitCode);
  } else {
    console.log('unexpected message: ' + e.data);
    return;
  }
}

/**
 * Handle load error event from NaCl.
 */
NaClTerm.prototype.handleLoadAbort_ = function(e) {
  this.updateStatus('Load aborted.');
}

/**
 * Handle load abort event from NaCl.
 */
NaClTerm.prototype.handleLoadError_ = function(e) {
  this.updateStatus(embed.lastError);
}

NaClTerm.prototype.doneLoadingUrl = function() {
  var width = this.io.terminal_.screenSize.width;
  this.io.print('\r' + Array(width+1).join(' '));
  var message = '\rLoaded ' + this.lastUrl;
  if (this.lastTotal) {
    var kbsize = Math.round(this.lastTotal/1024)
    message += ' ['+ kbsize + ' KiB]';
  }
  this.io.print(message.slice(0, width) + '\n')
}

/**
 * Handle load end event from NaCl.
 */
NaClTerm.prototype.handleLoad_ = function(e) {
  if (this.lastUrl)
    this.doneLoadingUrl();
  else
    this.io.print('Loaded.\n');
  delete this.lastUrl

  document.getElementById('loading-cover').style.display = 'none';

  this.io.print(ansiReset);

  // Now that have completed loading and displaying
  // loading messages we output any messages from the
  // NaCl module that were buffered up unto this point
  this.loaded = true;
  this.io.print(this.bufferedOutput);
  this.sendMessage(this.bufferedInput);
  this.bufferedOutput = ''
  this.bufferedInput = ''
}

/**
 * Handle load progress event from NaCl.
 */
NaClTerm.prototype.handleProgress_ = function(e) {
  var url = e.url.substring(e.url.lastIndexOf('/') + 1);

  if (this.lastUrl && this.lastUrl != url)
    this.doneLoadingUrl()

  if (!url)
    return;

  var percent = 10;
  var message = 'Loading ' + url;

  if (e.lengthComputable && e.total > 0) {
    percent = Math.round(e.loaded * 100 / e.total);
    var kbloaded = Math.round(e.loaded / 1024);
    var kbtotal = Math.round(e.total / 1024);
    message += ' [' + kbloaded + ' KiB/' + kbtotal + ' KiB ' + percent + '%]';
  }

  document.getElementById('progress-bar').style.width = percent + "%";

  var width = this.io.terminal_.screenSize.width;
  this.io.print('\r' + message.slice(-width));
  this.lastUrl = url;
  this.lastTotal = e.total;
}

/**
 * Handle crash event from NaCl.
 */
NaClTerm.prototype.handleCrash_ = function(e) {
 this.exit(this.embed.exitStatus);
}

/**
 * Exit the command.
 */
NaClTerm.prototype.exit = function(code) {
 this.io.print(ansiCyan)
 if (code == -1) {
   this.io.print('Program crashed (exit status -1)\n')
 } else {
   this.io.print('Program exited (status=' + code + ')\n');
 }
 this.loaded = false;
};

NaClTerm.prototype.restartNaCl = function() {
  if (this.embed !== undefined) {
    document.getElementById("listener").removeChild(this.embed);
    delete this.embed;
  }
  this.io.terminal_.reset();
  this.startCommand();
  this.createEmbed(this.io.terminal_.screenSize.width, this.io.terminal_.screenSize.height);
}

/**
 * Create the NaCl embed element.
 * We delay this until the first terminal resize event so that we start
 * with the correct size.
 */
NaClTerm.prototype.createEmbed = function(width, height) {
  var mimetype = 'application/x-pnacl';
  if (navigator.mimeTypes[mimetype] === undefined) {
    if (mimetype.indexOf('pnacl') != -1)
      this.updateStatus('Browser does not support PNaCl or PNaCl is disabled');
    else
      this.updateStatus('Browser does not support NaCl or NaCl is disabled');
    return;
  }

  var embed = document.createElement('object');
  embed.width = 0;
  embed.height = 0;
  embed.data = NaClTerm.nmf;
  embed.type = mimetype;
  embed.addEventListener('message', this.handleMessage_.bind(this));
  embed.addEventListener('progress', this.handleProgress_.bind(this));
  embed.addEventListener('load', this.handleLoad_.bind(this));
  embed.addEventListener('error', this.handleLoadError_.bind(this));
  embed.addEventListener('abort', this.handleLoadAbort_.bind(this));
  embed.addEventListener('crash', this.handleCrash_.bind(this));

  function addParam(name, value) {
    var param = document.createElement('param');
    param.name = name;
    param.value = value;
    embed.appendChild(param);
  }

  addParam('PS_TTY_PREFIX', NaClTerm.prefix);
  addParam('PS_TTY_RESIZE', 'tty_resize');
  addParam('PS_TTY_COLS', width);
  addParam('PS_TTY_ROWS', height);
  addParam('PS_STDIN', '/dev/tty');
  addParam('PS_STDOUT', '/dev/tty');
  addParam('PS_STDERR', '/dev/tty');
  addParam('PS_VERBOSITY', '2');
  addParam('PS_EXIT_MESSAGE', 'exited');
  addParam('TERM', 'xterm-256color');
  addParam('LUA_DATA_URL', 'http://storage.googleapis.com/gonacl/demos/publish/234230_dev/lua');

  // Add ARGV arguments from query parameters.
  var args = lib.f.parseQuery(document.location.search);
  for (var argname in args) {
    addParam(argname, args[argname]);
  }

  // If the application has set NaClTerm.argv and there were
  // no arguments set in the query parameters then add the default
  // NaClTerm.argv arguments.
  if (args['arg1'] === undefined && NaClTerm.argv) {
    var argn = 1
    NaClTerm.argv.forEach(function(arg) {
      var argname = 'arg' + argn;
      addParam(argname, arg);
      argn = argn + 1
    })
  }

  this.updateStatus('Loading...');
  this.io.print('Loading NaCl module.\n')
  document.getElementById("listener").appendChild(embed);
  this.embed = embed;
}

NaClTerm.prototype.onTerminalResize_ = function(width, height) {
  if (this.embed === undefined)
    this.createEmbed(width, height);
  else
    this.embed.postMessage({'tty_resize': [ width, height ]});

  // Require at least 80 columns, otherwise some of the demos look
  // very wrong.
  var width = this.io.terminal_.scrollPort_.characterSize.width * 80;
  document.getElementById("terminal").style.minWidth = width + 'px';
}

NaClTerm.prototype.sendMessage = function(msg) {
  if (!this.loaded) {
    this.bufferedInput += msg;
    return;
  }
  var message = {};
  message[NaClTerm.prefix] = msg;
  this.embed.postMessage(message);
}

NaClTerm.prototype.onVTKeystroke_ = function(str) {
  this.sendMessage(str)
}

NaClTerm.prototype.startCommand = function() {
  // We don't properly support the hterm bell sound, so we need to disable it.
  this.io.terminal_.prefs_.definePreference('audible-bell-sound', '');
  this.io.terminal_.setAutoCarriageReturn(true);
  this.io.terminal_.setCursorPosition(0, 0);
  this.io.terminal_.setCursorVisible(true);

  this.bufferedOutput = '';
  this.bufferedInput = '';
  this.loaded = false;
  this.io.print(ansiCyan);
}

/*
 * This is invoked by the terminal as a result of terminal.runCommandClass().
 */
NaClTerm.prototype.run = function() {
  this.startCommand();
  this.io.onVTKeystroke = this.onVTKeystroke_.bind(this);
  this.io.onTerminalResize = this.onTerminalResize_.bind(this);
};
