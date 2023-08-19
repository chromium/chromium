// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * One TtsEventObserver is created for each utterance (for each call to
 * chrome.tts.Speak that didn't result in an error).
 * @implements {ax.mojom.TtsUtteranceClientInterface}
 */
class TtsEventObserver {
  constructor(pendingReceiver, callback) {
    this.receiver_ = new ax.mojom.TtsUtteranceClientReceiver(this);
    this.receiver_.$.bindHandle(pendingReceiver.handle);
    this.callback_ = callback;
  }

  /** @override */
  onEvent(ttsEvent) {
    if (this.callback_) {
      this.callback_(ttsEvent);
    }
  }
}

// Massages the tts mojo into the chrome.tts extension API surface used
// by a11y component extensions.
// TODO(b:277221897): Compile and type-check this.
// TODO(b:266767235): Convert to typescript.
class AtpTts {
  /**
   * @enum {string}
   */
  EventType = {
    START: 'start',
    END: 'end',
    WORD: 'word',
    SENTENCE: 'sentence',
    MARKER: 'marker',
    INTERRUPTED: 'interrupted',
    CANCELLED: 'cancelled',
    ERROR: 'error',
    PAUSE: 'pause',
    RESUME: 'resume',
  };

  constructor() {
    const TtsApi = ax.mojom.Tts;
    this.remote_ = TtsApi.getRemote();

    /** @private Set<!TtsEventObserver> */
    this.ttsEventObservers_ = new Set();
  }

  /**
   * Speaks the given `utterance`.
   * This is not async because the TTS extension does not use async calls,
   * so callers do not expect to be blocked awaiting speak to finish.
   * @param {string} utterance
   * @param {chrome.tts.TtsOptions} ttsOptions
   */
  speak(utterance, ttsOptions) {
    this.remote_.speak(utterance, AtpTts.createMojomOptions_(ttsOptions))
        .then(speakResult => {
          if (speakResult.result.error != ax.mojom.TtsError.kNoError) {
            console.error(
                'Error when trying to speak', utterance,
                speakResult.result.error);
            return;
          }
          if (!ttsOptions.onEvent) {
            // No utterance client will be returned, no need to make an
            // event observer.
            return;
          }
          if (!speakResult.result.utteranceClient) {
            console.error(
                'UtteranceClient was unexpectedly ' +
                'missing from TtsSpeakResult.');
            return;
          }
          const ttsEventObserver = new TtsEventObserver(
              speakResult.result.utteranceClient, (ttsEvent) => {
                if (ttsOptions.onEvent) {
                  let type = AtpTts.convertFromMojomEventType_(ttsEvent.type);
                  ttsOptions.onEvent({
                    type,
                    charIndex: ttsEvent.charIndex,
                    length: ttsEvent.length,
                    errorMessage: ttsEvent.errorMessage
                  });
                }
                if (ttsEvent.isFinal) {
                  // There will be no more events. Delete this observer from
                  // observers.
                  this.ttsEventObservers_.delete(ttsEventObserver);
                }
              });
          this.ttsEventObservers_.add(ttsEventObserver);
        });
  }

  /**
   * Stops any current speech and flushes the queue of any pending utterances.
   * In addition, if speech was paused, it will now be un-paused for the next
   * call to speak.
   */
  stop() {
    this.remote_.stop();
  }

  /**
   * Pauses speech synthesis, potentially in the middle of an utterance. A call
   * to resume or stop will un-pause speech.
   */
  pause() {
    this.remote_.pause();
  }

  /**
   * If speech was paused, resumes speaking where it left off.
   */
  resume() {
    this.remote_.resume();
  }

  /**
   * Checks whether the engine is currently speaking.
   * @param {function(boolean): void} callback
   */
  isSpeaking(callback) {
    this.remote_.isSpeaking().then(response => callback(response.speaking));
  }

  /**
   * Gets the currently available voices.
   * @param {function(!Array<!chrome.tts.TtsVoice>)} callback
   */
  getVoices(callback) {
    this.remote_.getVoices().then(voicesResult => {
      // Shape into chrome.tts API getvoices response for callback.
      let result = [];
      for (let voice of voicesResult.voices) {
        let eventTypes = [];
        for (let eventType of voice.eventTypes) {
          eventTypes.push(AtpTts.convertFromMojomEventType_(eventType));
        }
        result.push({
          voiceName: voice.voiceName,
          eventTypes,
          extensionId: voice.engineId,
          lang: voice.lang,
          remote: voice.remote
        });
      }
      callback(result);
    });
  }

  /**
   * Converts ax.mojom.TtsEventType into the string used by TTS extension API.
   * @param {ax.mojom.TtsEventType} eventType
   * @return {EventType}
   * @private
   */
  static convertFromMojomEventType_(eventType) {
    switch (eventType) {
      case ax.mojom.TtsEventType.kStart:
        return chrome.tts.EventType.START;
      case ax.mojom.TtsEventType.kEnd:
        return chrome.tts.EventType.END;
      case ax.mojom.TtsEventType.kWord:
        return chrome.tts.EventType.WORD;
      case ax.mojom.TtsEventType.kSentence:
        return chrome.tts.EventType.SENTENCE;
      case ax.mojom.TtsEventType.kMarker:
        return chrome.tts.EventType.MARKER;
      case ax.mojom.TtsEventType.kInterrupted:
        return chrome.tts.EventType.INTERRUPTED;
      case ax.mojom.TtsEventType.kCancelled:
        return chrome.tts.EventType.CANCELLED;
      case ax.mojom.TtsEventType.kError:
        return chrome.tts.EventType.ERROR;
      case ax.mojom.TtsEventType.kPause:
        return chrome.tts.EventType.PAUSE;
      case ax.mojom.TtsEventType.kResume:
        return chrome.tts.EventType.RESUME;
    }
  }

  /**
   * Constructs a Mojom options object from a TtsOptions.
   * @param {?chrome.tts.TtsOptions} ttsOptions
   * @return {ax.mojom.TtsOptions}
   * @private
   */
  static createMojomOptions_(ttsOptions) {
    let options = new ax.mojom.TtsOptions();
    if (ttsOptions === undefined) {
      // Use default options.
      return options;
    }

    if (ttsOptions.rate !== undefined) {
      options.rate = ttsOptions.rate;
    }
    if (ttsOptions.pitch !== undefined) {
      options.pitch = ttsOptions.pitch;
    }
    if (ttsOptions.volume !== undefined) {
      options.volume = ttsOptions.volume;
    }
    if (ttsOptions.enqueue !== undefined) {
      options.enqueue = ttsOptions.enqueue;
    }
    if (ttsOptions.voiceName !== undefined) {
      options.voiceName = ttsOptions.voiceName;
    }
    if (ttsOptions.engineId !== undefined) {
      options.engineId = ttsOptions.engineId;
    }
    if (ttsOptions.lang !== undefined) {
      options.lang = ttsOptions.lang;
    }
    if (ttsOptions.onEvent !== undefined) {
      options.onEvent = ttsOptions.onEvent !== undefined;
    }
    return options;
  }
}

// Shim the TTS api onto the Chrome object to mimic chrome.tts in extensions.
chrome.tts = new AtpTts();
