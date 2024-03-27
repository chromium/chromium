// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Class that can be used by ATP ChromeEvents. Consumers can set
// up any required state in the `listenerAddedCallback`, which is
// run the first time a listener is added to this ChromeEvent.
class ChromeEvent {
  /**
   * @param {?function<>} listenerAddedCallback Callback to run the first
   *     time a listener is added.
   */
  constructor(listenerAddedCallback) {
    /** @type {!Set<!Function>} */
    this.listeners_ = new Set();
    this.listenerAddedCallback_ = listenerAddedCallback;
  }

  /**
   * Adds a listener to this event.
   * @param {!Function} listener
   */
  addListener(listener) {
    this.listeners_.add(listener);
    if (this.listenerAddedCallback_) {
      this.listenerAddedCallback_();
      this.listenerAddedCallback_ = null;
    }
  }

  /**
   * Removes a listener from this event.
   * @param {Function} listener
   */
  removeListener(listener) {
    this.listeners_.delete(listener);
  }

  /**
   * Calls all listeners of this event with the given args.
   * @param  {...any} args Args to pass to listeners.
   */
  callListeners(...args) {
    try {
      this.listeners_.forEach(listener => {
        listener(...args);
      });
    } catch (err) {
      console.error(err);
    }
  }
}
