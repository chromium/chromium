// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a concrete implementation of SpeechRecognitionEventObserver.
class AtpSpeechRecognitionEventObserver {
  /**
   * @param {!ax.mojom.SpeechRecognitionEventObserverPendingReceiver}
   *    pendingReceiver
   * @param {!function(): void} onStopCallback
   * @param {!function(!ax.mojom.SpeechRecognitionResultEvent): void}
   *    onResultCallback
   * @param {!function(): void} onErrorCallback
   */
  constructor(pendingReceiver, onStopCallback, onResultCallback,
        onErrorCallback) {
    this.receiver_ = new ax.mojom.SpeechRecognitionEventObserverReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);
    /** @private {!function(): void} */
    this.onStopCallback_ = onStopCallback;
    /** @private {!function(!ax.mojom.SpeechRecognitionResultEvent): void} */
    this.onResultCallback_ = onResultCallback;
    /** @private {!function(!ax.mojom.SpeechRecognitionErrorEvent): void} */
    this.onErrorCallback_ = onErrorCallback;
  }

  onStop() {
    this.onStopCallback_();
  }

  /** @param {!ax.mojom.SpeechRecognitionResultEvent} event */
  onResult(event) {
    this.onResultCallback_(event);
  }

  /** @param {ax.mojom.SpeechRecognitionErrorEvent} event */
  onError(event) {
    this.onErrorCallback_(event);
  }
}

// The ATP shim of the speech recognition private API.
class AtpSpeechRecognition {
  constructor() {
    const SpeechRecognitionApi = ax.mojom.SpeechRecognition;
    this.remote_ = SpeechRecognitionApi.getRemote();
    /** @private {!Map<number, !AtpSpeechRecognitionEventObserver>} */
    this.observers_ = new Map();
    /** @type {!ChromeEvent} */
    this.onStop = new ChromeEvent();
    /** @type {!ChromeEvent} */
    this.onResult = new ChromeEvent();
    /** @type {!ChromeEvent} */
    this.onError = new ChromeEvent();
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
        },
        /*onResultCallback=*/(event) => {
          this.handleOnResult_(event);
        },
        /*onErrorCallback=*/(event) => {
          this.handleOnError_(event);
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

  /** @private */
  handleOnStop_() {
    // TODO(b/304305202): Ensure we remove the relevant event observer.
    this.onStop.callListeners();
  }

  /**
   * @param {!ax.mojom.SpeechRecognitionResultEvent} event
   * @private
   */
  handleOnResult_(event) {
    this.onResult.callListeners(
      /**
       * @type {!chrome.speechRecognitionPrivate.SpeechRecognitionResultEvent}
       */ (event));
  }

  /**
   * @param {!ax.mojom.SpeechRecognitionErrorEvent} event
   * @private
   */
  handleOnError_(event) {
    this.onError.callListeners(
      /**
       * @type {!chrome.speechRecognitionPrivate.SpeechRecognitionErrorEvent}
       */ (event));
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
