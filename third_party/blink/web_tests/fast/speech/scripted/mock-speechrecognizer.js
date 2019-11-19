'use strict';

// MockSpeechRecognizer is a mock implementation of mojom::SpeechRecognizer and
// the browser speech recognition service. Mock results can be set using
// addMockSpeechRecognitionResult, and setMockSpeechRecognitionError can be used
// to simulate an error state. If no mock results are set, a NoMatch error is
// sent to the client.
class MockSpeechRecognizer {
  constructor() {
    this.results_ = [];
    this.session_ = null;
    this.session_client_ = null;
    this.error_ = null;
    this.lastSetTimeout_ = null;

    this.task_queue_ = [];

    this.binding_ = new mojo.Binding(blink.mojom.SpeechRecognizer, this);
    this.interceptor_ = new MojoInterfaceInterceptor(blink.mojom.SpeechRecognizer.name, "context", true);
    this.interceptor_.oninterfacerequest = e => {
      this.binding_.bind(e.handle);
      this.binding_.setConnectionErrorHandler(() => {
        this.reset();
      });
    };
    this.interceptor_.start();
  }

  reset() {
    this.binding_.close();
    this.results_ = [];
    this.session_ = null;
    this.session_client_ = null;
    this.error_ = null;
  }

  addMockSpeechRecognitionResult(transcript, confidence) {
    let toString16 = function(string) {
      let array = new Array(string.length);
      for (var i = 0; i < string.length; ++i) {
        array[i] = string.charCodeAt(i);
      }
      return { data: array };
    }
    var hypothesis = new blink.mojom.SpeechRecognitionHypothesis({
      utterance: toString16(transcript),
      confidence: confidence
    });
    var result = new blink.mojom.SpeechRecognitionResult({
      hypotheses: [hypothesis],
      is_provisional: false
    });
    this.results_.push(result);
  }

  setMockSpeechRecognitionError(errorCode) {
    this.error_ = new blink.mojom.SpeechRecognitionError({
      code: errorCode,
      details: blink.mojom.SpeechAudioErrorDetails.kNone
    });
  }

  dispatchError() {
    this.session_client_.errorOccurred(this.error_);
    this.error_ = null;
    this.session_client_.ended();
  }

  dispatchResult() {
    this.session_client_.started();
    this.session_client_.audioStarted();
    this.session_client_.soundStarted();

    if (this.results_.length) {
      this.results_.forEach(result => {
        this.session_client_.resultRetrieved([result]);
      });
      this.results_ = [];
    } else {
      var error = new blink.mojom.SpeechRecognitionError({
        code: blink.mojom.SpeechRecognitionErrorCode.kNoMatch,
        details: blink.mojom.SpeechAudioErrorDetails.kNone
      });
      this.session_client_.errorOccurred(error);
    }
    this.session_client_.soundEnded();
    this.session_client_.audioEnded();
    this.session_client_.ended();
  }

  start(params) {
    this.session_ = new MockSpeechRecognitionSession(params.sessionReceiver, this);
    this.session_client_ = params.client;

    // if setMockSpeechRecognitionError was called
    if (this.error_) {
      this.dispatchError();
      return;
    }

    this.dispatchResult();
  }
}

let mockSpeechRecognizer = new MockSpeechRecognizer();

class MockSpeechRecognitionSession {
  constructor(request, recognizer) {
    this.binding_ = new mojo.Binding(blink.mojom.SpeechRecognitionSession, this, request);
    this.binding_.setConnectionErrorHandler(() => {
      this.binding_.close();
    });
    this.recognizer_ = recognizer;
  }

  stop() {}

  abort() {}
}
