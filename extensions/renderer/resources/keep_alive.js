// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
loadScript('extensions/common/mojom/keep_alive.mojom');

/**
 * An object that keeps the background page alive until closed.
 * @constructor
 * @alias module:keep_alive~KeepAlive
 */
function KeepAlive() {
  var pipe = Mojo.createMessagePipe();
  /**
   * The handle to the keep-alive object in the browser.
   * @type {!MojoHandle}
   * @private
   */
  this.handle_ = pipe.handle0;
  Mojo.bindInterface(extensions.KeepAlive.name, pipe.handle1);
}

/**
 * Removes this keep-alive.
 */
KeepAlive.prototype.close = function() {
  this.handle_.close();
};

/**
 * Creates a keep-alive.
 * @return {!module:keep_alive~KeepAlive} A new keep-alive.
 */
exports.$set('createKeepAlive', function() { return new KeepAlive(); });