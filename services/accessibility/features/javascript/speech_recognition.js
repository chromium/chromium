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
    /**
     * @private {!Map<
     *    !ax.mojom.AssistiveTechnologyType, !AtpSpeechRecognitionEventObserver
     * >}
     */
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
      const observerOrError = result.info.observerOrError;
      const error = observerOrError.error;
      if (error) {
        this.runCallbackWithError_(error, callback, type);
        return;
      }

      const observer = new AtpSpeechRecognitionEventObserver(
        /*pendingReceiver=*/observerOrError.observer,
        /*onStopCallback=*/() => {
          this.handleOnStop_();
        },
        /*onResultCallback=*/(event) => {
          this.handleOnResult_(event);
        },
        /*onErrorCallback=*/(event) => {
          this.handleOnError_(event);
        });
      this.observers_.set(mojoOptions.type, observer);
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
    this.remote_.stop(mojoOptions).then((result) => {
      const error = result.error;
      if (error) {
        this.runCallbackWithError_(error, callback);
        return;
      }

      this.observers_.delete(mojoOptions.type);
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
   * TODO(b/304305202): Move this function to a separate file (runtime.js).
   * @param {string} error
   * @param {!Function} callback
   * @private
   */
  runCallbackWithError_(error, callback, ...args) {
    // To mirror the behavior of extension APIs, we set
    // chrome.runtime.lastError for the duration of the callback and reset
    // it after it finishes execution.
    chrome.runtime.lastError = {message: error};
    callback(...args);
    chrome.runtime.lastError = undefined;
  }

  /**
   * @param {!chrome.speechRecognitionPrivate.StartOptions} source
   * @return {!ax.mojom.StartOptions}
   * @private
   */
  static convertStartOptions_(source) {
    const options = new ax.mojom.StartOptions();
    options.type = AtpSpeechRecognition.clientIdToAssistiveTechnologyType_(
        source.clientId);
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
    options.type = AtpSpeechRecognition.clientIdToAssistiveTechnologyType_(
        source.clientId);
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

  /**
   * @param {number|undefined} clientId
   * @return {!ax.mojom.AssistiveTechnologyType}
   * @private
   */
  static clientIdToAssistiveTechnologyType_(clientId) {
    // Use Dictation as the type since it's the only accessibility feature that
    // uses speech recognition.
    return ax.mojom.AssistiveTechnologyType.kDictation;
  }
}

chrome.speechRecognitionPrivate = new AtpSpeechRecognition();
