// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-forward.h"

namespace media {
class AudioSystem;
}

namespace content {
class BrowserMainLoop;
class MediaStreamManager;
class MediaStreamUIProxy;
class SpeechRecognitionManagerDelegate;
class SpeechRecognizer;

// This is the manager for speech recognition. It is a single instance in
// the browser process and can serve several requests. Each recognition request
// corresponds to a session, initiated via |CreateSession|.
//
// In any moment, the manager has a single session known as the primary session,
// |primary_session_id_|.
// This is the session that is capturing audio, waiting for user permission,
// etc. There may also be other, non-primary, sessions living in parallel that
// are waiting for results but not recording audio.
//
// The SpeechRecognitionManager has the following responsibilities:
//  - Handles requests received from various render frames and makes sure only
//    one of them accesses the audio device at any given time.
//  - Handles the instantiation of NetworkSpeechRecognitionEngineImpl objects
//    when requested by SpeechRecognitionSessions.
//  - Relays recognition results/status/error events of each session to the
//    corresponding listener (demuxing on the base of their session_id).
//  - Relays also recognition results/status/error events of every session to
//    the catch-all snoop listener (optionally) provided by the delegate.
class CONTENT_EXPORT SpeechRecognitionManagerImpl
    : public SpeechRecognitionManager,
      public SpeechRecognitionEventListener {
 public:
  // Returns the current SpeechRecognitionManagerImpl or NULL if the call is
  // issued when it is not created yet or destroyed (by BrowserMainLoop).
  static SpeechRecognitionManagerImpl* GetInstance();

  static bool IsOnDeviceSpeechRecognitionAvailable(
      const SpeechRecognitionSessionConfig& config);

  // SpeechRecognitionManager implementation.
  int CreateSession(const SpeechRecognitionSessionConfig& config) override;
  int CreateSession(
      const SpeechRecognitionSessionConfig& config,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          client_remote,
      std::optional<SpeechRecognitionAudioForwarderConfig>
          audio_forwarder_config) override;
  void StartSession(int session_id) override;
  void AbortSession(int session_id) override;
  void AbortAllSessionsForRenderFrame(int render_process_id,
                                      int render_frame_id) override;
  void StopAudioCaptureForSession(int session_id) override;
  const SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) override;
  SpeechRecognitionSessionContext GetSessionContext(int session_id) override;
  bool UseOnDeviceSpeechRecognition(
      const SpeechRecognitionSessionConfig& config) override;

  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& result)
      override;
  void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

  SpeechRecognitionManagerDelegate* delegate() const { return delegate_.get(); }

 protected:
  // BrowserMainLoop is the only one allowed to instantiate this class.
  friend class BrowserMainLoop;

  // Needed for deletion on the IO thread.
  friend std::default_delete<SpeechRecognitionManagerImpl>;
  friend class base::DeleteHelper<content::SpeechRecognitionManagerImpl>;

  SpeechRecognitionManagerImpl(media::AudioSystem* audio_system,
                               MediaStreamManager* media_stream_manager);
  ~SpeechRecognitionManagerImpl() override;

 private:

  // Data types for the internal Finite State Machine (FSM).
  enum FSMState {
    SESSION_STATE_IDLE = 0,
    SESSION_STATE_CAPTURING_AUDIO,
    SESSION_STATE_WAITING_FOR_RESULT,
    SESSION_STATE_MAX_VALUE = SESSION_STATE_WAITING_FOR_RESULT
  };

  enum FSMEvent {
    EVENT_ABORT = 0,
    EVENT_START,
    EVENT_STOP_CAPTURE,
    EVENT_AUDIO_ENDED,
    EVENT_RECOGNITION_ENDED,
    EVENT_MAX_VALUE = EVENT_RECOGNITION_ENDED
  };

  struct Session {
    Session();
    ~Session();

    int id;
    bool abort_requested;
    SpeechRecognitionSessionConfig config;
    SpeechRecognitionSessionContext context;
    scoped_refptr<SpeechRecognizer> recognizer;
    std::unique_ptr<MediaStreamUIProxy> ui;
    bool use_microphone;
  };

  void AbortSessionImpl(int session_id);

  // Callback issued by the SpeechRecognitionManagerDelegate for reporting
  // asynchronously the result of the CheckRecognitionIsAllowed call.
  void RecognitionAllowedCallback(int session_id,
                                  bool ask_user,
                                  bool is_allowed);

  // Callback to get back the result of a media request. |devices| is an array
  // of devices approved to be used for the request, |devices| is empty if the
  // users deny the request.
  void MediaRequestPermissionCallback(
      int session_id,
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      std::unique_ptr<MediaStreamUIProxy> stream_ui);

  // Entry point for pushing any external event into the session handling FSM.
  void DispatchEvent(int session_id, FSMEvent event);

  // Defines the behavior of the session handling FSM, selecting the appropriate
  // transition according to the session, its current state and the event.
  void ExecuteTransitionAndGetNextState(Session* session,
                                        FSMState session_state,
                                        FSMEvent event);

  // Retrieves the state of the session, enquiring directly the recognizer.
  FSMState GetSessionState(int session_id) const;

  // The methods below handle transitions of the session handling FSM.
  void SessionStart(const Session& session);
  void SessionAbort(const Session& session);
  void SessionStopAudioCapture(const Session& session);
  void ResetCapturingSessionId(const Session& session);
  void SessionDelete(Session* session);
  void NotFeasible(const Session& session, FSMEvent event);

  bool SessionExists(int session_id) const;
  Session* GetSession(int session_id) const;
  SpeechRecognitionEventListener* GetListener(int session_id) const;
  SpeechRecognitionEventListener* GetDelegateListener() const;
  int GetNextSessionID();

  static int next_requester_id_;

  raw_ptr<media::AudioSystem> audio_system_;
  raw_ptr<MediaStreamManager> media_stream_manager_;
  base::flat_map<int, std::unique_ptr<Session>> sessions_;
  int primary_session_id_;
  int last_session_id_;
  bool is_dispatching_event_;
  std::unique_ptr<SpeechRecognitionManagerDelegate> delegate_;
  const int requester_id_;

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;

  // Used for posting asynchronous tasks (on the IO thread) without worrying
  // about this class being destroyed in the meanwhile (due to browser shutdown)
  // since tasks pending on a destroyed WeakPtr are automatically discarded.
  base::WeakPtrFactory<SpeechRecognitionManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_MANAGER_IMPL_H_
