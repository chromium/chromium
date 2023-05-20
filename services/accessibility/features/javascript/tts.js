// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Massages the tts mojo into the chrome.tts extension API surface used
// by a11y component extensions.
// TODO(b:277221897): Compile and type-check this.
// TODO(b:266767235): Convert to typescript.
class AtpTts {
  constructor() {
    const TtsApi = ax.mojom.Tts;
    this.remote_ = TtsApi.getRemote();
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
          eventTypes.push(AtpTts.eventTypeToString_(eventType));
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
   * @return {chrome.tts.EventType}
   * @private
   */
  static eventTypeToString_(eventType) {
    switch (eventType) {
      case ax.mojom.TtsEventType.kStart:
        return 'start';
      case ax.mojom.TtsEventType.kEnd:
        return 'end';
      case ax.mojom.TtsEventType.kWord:
        return 'word';
      case ax.mojom.TtsEventType.kSentence:
        return 'sentence';
      case ax.mojom.TtsEventType.kMarker:
        return 'marker';
      case ax.mojom.TtsEventType.kInterrupted:
        return 'interrupted';
      case ax.mojom.TtsEventType.kCancelled:
        return 'cancelled';
      case ax.mojom.TtsEventType.kError:
        return 'error';
      case ax.mojom.TtsEventType.kPause:
        return 'pause';
      case ax.mojom.TtsEventType.kResume:
        return 'resume';
    }
  }
}

// Shim the TTS api onto the Chrome object to mimic chrome.tts in extensions.
chrome.tts = new AtpTts();
