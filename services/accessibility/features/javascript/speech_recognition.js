// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper class for the stop event.
// TODO(b:304305202): Inherit from ATP ChromeEvent.
class SpeechRecognitionStopEventManager {
  constructor() {
    /** @private {Set<!Function>} */
    this.listeners_ = new Set();
  }

  /** @param {!Function} listener */
  addListener(listener) {
    this.listeners_.add(listener);
  }

  /** @param {!Function} listener */
  removeListener(listener) {
    this.listeners_.delete(listener);
  }

  notify() {
    for (const listener of this.listeners_) {
      listener();
    }
  }
}

// Provides a concrete implementation of SpeechRecognitionEventObserver.
class AtpSpeechRecognitionEventObserver {
  /**
   * @param {!ax.mojom.SpeechRecognitionEventObserverPendingReceiver}
   *    pendingReceiver
   * @param {!Function} onStopCallback
   */
  constructor(pendingReceiver, onStopCallback) {
    this.receiver_ = new ax.mojom.SpeechRecognitionEventObserverReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);
    this.onStopCallback_ = onStopCallback;
  }

  onStop() {
    this.onStopCallback_();
  }
}

// The ATP shim of the speech recognition private API.
class AtpSpeechRecognition {
  constructor() {
    const SpeechRecognitionApi = ax.mojom.SpeechRecognition;
    this.remote_ = SpeechRecognitionApi.getRemote();
    /** @private {!Map<number, !AtpSpeechRecognitionEventObserver>} */
    this.observers_ = new Map();
    /** @type {!SpeechRecognitionEvent} */
    this.onStop = new SpeechRecognitionStopEventManager();
  }

  /**
   * @param {!chrome.speechRecognitionPrivate.StartOptions} options
   * @param {function(!chrome.speechRecognitionPrivate.SpeechRecognitionType):
   *    void} callback
   * Called when speech recognition has begun listening to the user's
   *     audio. The callback's parameter specifies which type of speech
   *     recognition is being used.
   */
  start(options, callback) {
    const mojoOptions = AtpSpeechRecognition.convertStartOptions_(options);
    this.remote_.start(mojoOptions).then(result => {
      const type = AtpSpeechRecognition.convertRecognitionType_(
          result.info.type);
      const observer = new AtpSpeechRecognitionEventObserver(
        /*pendingReceiver=*/result.info.observer,
        /*onStopCallback=*/() => {
          this.handleOnStop_();
        });
      // Default client ID is 0. It is possible for clients to pass an ID of 0,
      // but this will never happen in practice since we control the only
      // existing caller and potential new callers.
      const clientId = mojoOptions.clientId ?? 0;
      this.observers_.set(clientId, observer);
      callback(type);
    });
  }

  /**
   * @param {!chrome.speechRecognitionPrivate.StopOptions} options
   * @param {function(): void} callback
   * Called when speech recognition has stopped listening to the user's audio.
   */
  stop(options, callback) {
    const mojoOptions = AtpSpeechRecognition.convertStopOptions_(options);
    this.remote_.stop(mojoOptions).then(() => {
      const clientId = mojoOptions.clientId ?? 0;
      this.observers_.delete(clientId);
      callback();
    });
  }

  handleOnStop_() {
    // TODO(b/304305202): Ensure we remove the relevant event observer.
    this.onStop.notify();
  }

  /**
   * @param {!chrome.speechRecognitionPrivate.StartOptions} source
   * @return {!ax.mojom.StartOptions}
   * @private
   */
  static convertStartOptions_(source) {
    const options = new ax.mojom.StartOptions();
    if (source.clientId !== undefined) {
      options.clientId = source.clientId;
    }
    if (source.locale !== undefined) {
      options.locale = source.locale;
    }
    if (source.interimResults !== undefined) {
      options.interimResults = source.interimResults;
    }

    return options;
  }

  /**
   * @param {!chrome.speechRecognitionPrivate.StopOptions} source
   * @return {!ax.mojom.StopOptions}
   * @private
   */
  static convertStopOptions_(source) {
    const options = new ax.mojom.StopOptions();
    if (source.clientId !== undefined) {
      options.clientId = source.clientId;
    }

    return options;
  }

  /**
   * @param {!ax.mojom.SpeechRecognitionType} type
   * @return {string}
   * @private
   */
  static convertRecognitionType_(type) {
    if (type == ax.mojom.SpeechRecognitionType.kOnDevice) {
      return 'onDevice';
    }

    return 'network';
  }
}

chrome.speechRecognitionPrivate = new AtpSpeechRecognition();
