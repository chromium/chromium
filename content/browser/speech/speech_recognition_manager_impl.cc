// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_manager_impl.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "media/audio/audio_device_description.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "content/browser/speech/speech_recognizer_impl_android.h"
#endif

using base::Callback;

namespace content {

SpeechRecognitionManager* SpeechRecognitionManager::manager_for_tests_;

namespace {

SpeechRecognitionManagerImpl* g_speech_recognition_manager_impl;

}  // namespace

int SpeechRecognitionManagerImpl::next_requester_id_ = 0;

class SpeechRecognitionManagerImpl::FrameDeletionObserver {
 public:
  using FrameDeletedCallback =
      base::RepeatingCallback<void(int /* session_id */)>;
  explicit FrameDeletionObserver(FrameDeletedCallback frame_deleted_callback);

  void CreateObserverForSession(int render_process_id,
                                int render_frame_id,
                                int session_id);

  void RemoveObserverForSession(int render_process_id,
                                int render_frame_id,
                                int session_id);

 private:
  class ContentsObserver;
  friend class ContentsObserver;

  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<FrameDeletionObserver>;

  ~FrameDeletionObserver();

  FrameDeletedCallback frame_deleted_callback_;
  std::map<WebContents*, std::unique_ptr<ContentsObserver>> contents_observers_;
};

class SpeechRecognitionManagerImpl::FrameDeletionObserver::ContentsObserver
    : public WebContentsObserver {
 public:
  ContentsObserver(WebContents* web_contents,
                   FrameDeletionObserver* parent_observer)
      : WebContentsObserver(web_contents), parent_observer_(parent_observer) {}

  void AddObservedFrame(RenderFrameHost* render_frame_host, int session_id);
  void RemoveObservedFrame(RenderFrameHost* render_frame_host, int session_id);

  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

 private:
  FrameDeletionObserver* parent_observer_;

  // A multimap from the frame to the session_ids started by that frame.
  // Although a rare case, theoretically a frame can start multiple sessions.
  std::multimap<RenderFrameHost*, int> observed_frames_;
};

SpeechRecognitionManagerImpl::FrameDeletionObserver::FrameDeletionObserver(
    FrameDeletedCallback frame_deleted_callback)
    : frame_deleted_callback_(std::move(frame_deleted_callback)) {}

void SpeechRecognitionManagerImpl::FrameDeletionObserver::
    CreateObserverForSession(int render_process_id,
                             int render_frame_id,
                             int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  auto& observer = contents_observers_[web_contents];
  if (!observer)
    observer = std::make_unique<ContentsObserver>(web_contents, this);

  observer->AddObservedFrame(render_frame_host, session_id);
}

void SpeechRecognitionManagerImpl::FrameDeletionObserver::
    RemoveObserverForSession(int render_process_id,
                             int render_frame_id,
                             int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  auto it = contents_observers_.find(web_contents);
  if (it == contents_observers_.end())
    return;

  it->second->RemoveObservedFrame(render_frame_host, session_id);
}

SpeechRecognitionManagerImpl::FrameDeletionObserver::~FrameDeletionObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_EQ(0u, contents_observers_.size());
}

void SpeechRecognitionManagerImpl::FrameDeletionObserver::ContentsObserver::
    AddObservedFrame(RenderFrameHost* render_frame_host, int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  observed_frames_.emplace(render_frame_host, session_id);
}

void SpeechRecognitionManagerImpl::FrameDeletionObserver::ContentsObserver::
    RemoveObservedFrame(RenderFrameHost* render_frame_host, int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iters = observed_frames_.equal_range(render_frame_host);
  auto it = std::find_if(iters.first, iters.second,
                         [render_frame_host, session_id](
                             std::pair<RenderFrameHost* const, int>& map_pair) {
                           return map_pair.first == render_frame_host &&
                                  map_pair.second == session_id;
                         });

  if (it == iters.second)
    return;

  observed_frames_.erase(it);
  if (!observed_frames_.size())
    parent_observer_->contents_observers_.erase(web_contents());

  // |this| may be deleted.
}

void SpeechRecognitionManagerImpl::FrameDeletionObserver::ContentsObserver::
    RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  auto iters = observed_frames_.equal_range(render_frame_host);
  for (auto it = iters.first; it != iters.second; ++it) {
    base::CreateSingleThreadTaskRunner({BrowserThread::IO})
        ->PostTask(FROM_HERE,
                   base::BindOnce(parent_observer_->frame_deleted_callback_,
                                  it->second));
  }

  observed_frames_.erase(iters.first, iters.second);
  if (!observed_frames_.size())
    parent_observer_->contents_observers_.erase(web_contents());

  // |this| is likely deleted.
}

SpeechRecognitionManager* SpeechRecognitionManager::GetInstance() {
  if (manager_for_tests_)
    return manager_for_tests_;
  return SpeechRecognitionManagerImpl::GetInstance();
}

void SpeechRecognitionManager::SetManagerForTesting(
    SpeechRecognitionManager* manager) {
  manager_for_tests_ = manager;
}

SpeechRecognitionManagerImpl* SpeechRecognitionManagerImpl::GetInstance() {
  return g_speech_recognition_manager_impl;
}

SpeechRecognitionManagerImpl::SpeechRecognitionManagerImpl(
    media::AudioSystem* audio_system,
    MediaStreamManager* media_stream_manager)
    : audio_system_(audio_system),
      media_stream_manager_(media_stream_manager),
      primary_session_id_(kSessionIDInvalid),
      last_session_id_(kSessionIDInvalid),
      is_dispatching_event_(false),
      delegate_(GetContentClient()
                    ->browser()
                    ->CreateSpeechRecognitionManagerDelegate()),
      requester_id_(next_requester_id_++) {
  DCHECK(!g_speech_recognition_manager_impl);
  g_speech_recognition_manager_impl = this;

  frame_deletion_observer_.reset(new FrameDeletionObserver(
      base::BindRepeating(&SpeechRecognitionManagerImpl::AbortSessionImpl,
                          weak_factory_.GetWeakPtr())));
}

SpeechRecognitionManagerImpl::~SpeechRecognitionManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(g_speech_recognition_manager_impl);

  g_speech_recognition_manager_impl = nullptr;
}

int SpeechRecognitionManagerImpl::CreateSession(
    const SpeechRecognitionSessionConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id = GetNextSessionID();
  DCHECK(!SessionExists(session_id));

  // Set-up the new session.
  auto session = std::make_unique<Session>();
  session->id = session_id;
  session->config = config;
  session->context = config.initial_context;

#if !defined(OS_ANDROID)
  // A SpeechRecognitionEngine (and corresponding Config) is required only
  // when using SpeechRecognizerImpl, which performs the audio capture and
  // endpointing in the browser. This is not the case of Android where, not
  // only the speech recognition, but also the audio capture and endpointing
  // activities performed outside of the browser (delegated via JNI to the
  // Android API implementation).

  SpeechRecognitionEngine::Config remote_engine_config;
  remote_engine_config.language = config.language;
  remote_engine_config.grammars = config.grammars;
  remote_engine_config.audio_sample_rate =
      SpeechRecognizerImpl::kAudioSampleRate;
  remote_engine_config.audio_num_bits_per_sample =
      SpeechRecognizerImpl::kNumBitsPerAudioSample;
  remote_engine_config.filter_profanities = config.filter_profanities;
  remote_engine_config.continuous = config.continuous;
  remote_engine_config.interim_results = config.interim_results;
  remote_engine_config.max_hypotheses = config.max_hypotheses;
  remote_engine_config.origin_url = config.origin.Serialize();
  remote_engine_config.auth_token = config.auth_token;
  remote_engine_config.auth_scope = config.auth_scope;
  remote_engine_config.preamble = config.preamble;

  SpeechRecognitionEngine* google_remote_engine = new SpeechRecognitionEngine(
      config.shared_url_loader_factory, config.accept_language);
  google_remote_engine->SetConfig(remote_engine_config);

  session->recognizer = new SpeechRecognizerImpl(
      this, audio_system_, session_id, config.continuous,
      config.interim_results, google_remote_engine);
#else
  session->recognizer = new SpeechRecognizerImplAndroid(this, session_id);
#endif

  sessions_[session_id] = std::move(session);

  // The deletion observer is owned by this class, so it's safe to use
  // Unretained.
  base::CreateSingleThreadTaskRunner({BrowserThread::UI})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&SpeechRecognitionManagerImpl::FrameDeletionObserver::
                             CreateObserverForSession,
                         base::Unretained(frame_deletion_observer_.get()),
                         config.initial_context.render_process_id,
                         config.initial_context.render_frame_id, session_id));

  return session_id;
}

void SpeechRecognitionManagerImpl::StartSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  // If there is another active session, abort that.
  if (primary_session_id_ != kSessionIDInvalid &&
      primary_session_id_ != session_id) {
    AbortSession(primary_session_id_);
  }

  primary_session_id_ = session_id;

  if (delegate_) {
    delegate_->CheckRecognitionIsAllowed(
        session_id,
        base::BindOnce(
            &SpeechRecognitionManagerImpl::RecognitionAllowedCallback,
            weak_factory_.GetWeakPtr(), session_id));
  }
}

void SpeechRecognitionManagerImpl::RecognitionAllowedCallback(int session_id,
                                                              bool ask_user,
                                                              bool is_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  Session* session = iter->second.get();

  if (session->abort_requested)
    return;

  if (ask_user) {
    SpeechRecognitionSessionContext& context = session->context;
    context.label = media_stream_manager_->MakeMediaAccessRequest(
        context.render_process_id, context.render_frame_id, requester_id_,
        session_id, blink::StreamControls(true, false), context.security_origin,
        base::BindOnce(
            &SpeechRecognitionManagerImpl::MediaRequestPermissionCallback,
            weak_factory_.GetWeakPtr(), session_id));
    return;
  }

  if (is_allowed) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                       weak_factory_.GetWeakPtr(), session_id, EVENT_START));
  } else {
    OnRecognitionError(
        session_id, blink::mojom::SpeechRecognitionError(
                        blink::mojom::SpeechRecognitionErrorCode::kNotAllowed,
                        blink::mojom::SpeechAudioErrorDetails::kNone));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                       weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
  }
}

void SpeechRecognitionManagerImpl::MediaRequestPermissionCallback(
    int session_id,
    const blink::MediaStreamDevices& devices,
    std::unique_ptr<MediaStreamUIProxy> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  bool is_allowed = !devices.empty();
  if (is_allowed) {
    // Copy the approved devices array to the context for UI indication.
    iter->second->context.devices = devices;

    // Save the UI object.
    iter->second->ui = std::move(stream_ui);
  }

  // Clear the label to indicate the request has been done.
  iter->second->context.label.clear();

  // Notify the recognition about the request result.
  RecognitionAllowedCallback(iter->first, false, is_allowed);
}

void SpeechRecognitionManagerImpl::AbortSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  // The deletion observer is owned by this class, so it's safe to use
  // Unretained.
  base::CreateSingleThreadTaskRunner({BrowserThread::UI})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&SpeechRecognitionManagerImpl::FrameDeletionObserver::
                             RemoveObserverForSession,
                         base::Unretained(frame_deletion_observer_.get()),
                         iter->second->config.initial_context.render_process_id,
                         iter->second->config.initial_context.render_frame_id,
                         session_id));

  AbortSessionImpl(session_id);
}

void SpeechRecognitionManagerImpl::AbortSessionImpl(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  iter->second->ui.reset();

  if (iter->second->abort_requested)
    return;

  iter->second->abort_requested = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                     weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
}

void SpeechRecognitionManagerImpl::StopAudioCaptureForSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  iter->second->ui.reset();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_STOP_CAPTURE));
}

// Here begins the SpeechRecognitionEventListener interface implementation,
// which will simply relay the events to the proper listener registered for the
// particular session and to the catch-all listener provided by the delegate
// (if any).

void SpeechRecognitionManagerImpl::OnRecognitionStart(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  auto iter = sessions_.find(session_id);
  if (iter->second->ui) {
    // Notify the UI that the devices are being used.
    iter->second->ui->OnStarted(base::OnceClosure(),
                                MediaStreamUI::SourceCallback(),
                                MediaStreamUIProxy::WindowIdCallback());
  }

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionStart(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioStart(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioStart(session_id);
}

void SpeechRecognitionManagerImpl::OnEnvironmentEstimationComplete(
    int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnEnvironmentEstimationComplete(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnEnvironmentEstimationComplete(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundStart(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  DCHECK_EQ(primary_session_id_, session_id);
  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnSoundStart(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnSoundStart(session_id);
}

void SpeechRecognitionManagerImpl::OnSoundEnd(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnSoundEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnSoundEnd(session_id);
}

void SpeechRecognitionManagerImpl::OnAudioEnd(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioEnd(session_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_AUDIO_ENDED));
}

void SpeechRecognitionManagerImpl::OnRecognitionResults(
    int session_id,
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& results) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionResults(session_id, results);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionResults(session_id, results);
}

void SpeechRecognitionManagerImpl::OnRecognitionError(
    int session_id,
    const blink::mojom::SpeechRecognitionError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionError(session_id, error);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionError(session_id, error);
}

void SpeechRecognitionManagerImpl::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnAudioLevelsChange(session_id, volume, noise_volume);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnAudioLevelsChange(session_id, volume, noise_volume);
}

void SpeechRecognitionManagerImpl::OnRecognitionEnd(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!SessionExists(session_id))
    return;

  if (SpeechRecognitionEventListener* delegate_listener = GetDelegateListener())
    delegate_listener->OnRecognitionEnd(session_id);
  if (SpeechRecognitionEventListener* listener = GetListener(session_id))
    listener->OnRecognitionEnd(session_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_RECOGNITION_ENDED));
}

SpeechRecognitionSessionContext SpeechRecognitionManagerImpl::GetSessionContext(
    int session_id) {
  return GetSession(session_id)->context;
}

void SpeechRecognitionManagerImpl::AbortAllSessionsForRenderFrame(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& session_pair : sessions_) {
    Session* session = session_pair.second.get();
    if (session->context.render_process_id == render_process_id &&
        session->context.render_frame_id == render_frame_id) {
      AbortSession(session->id);
    }
  }
}

// -----------------------  Core FSM implementation ---------------------------
void SpeechRecognitionManagerImpl::DispatchEvent(int session_id,
                                                 FSMEvent event) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // There are some corner cases in which the session might be deleted (due to
  // an EndRecognition event) between a request (e.g. Abort) and its dispatch.
  if (!SessionExists(session_id))
    return;

  Session* session = GetSession(session_id);
  FSMState session_state = GetSessionState(session_id);
  DCHECK_LE(session_state, SESSION_STATE_MAX_VALUE);
  DCHECK_LE(event, EVENT_MAX_VALUE);

  // Event dispatching must be sequential, otherwise it will break all the rules
  // and the assumptions of the finite state automata model.
  DCHECK(!is_dispatching_event_);
  is_dispatching_event_ = true;
  ExecuteTransitionAndGetNextState(session, session_state, event);
  is_dispatching_event_ = false;
}

// This FSM handles the evolution of each session, from the viewpoint of the
// interaction with the user (that may be either the browser end-user which
// interacts with UI bubbles, or JS developer interacting with JS methods).
// All the events received by the SpeechRecognizer instances (one for each
// session) are always routed to the SpeechRecognitionEventListener(s)
// regardless the choices taken in this FSM.
void SpeechRecognitionManagerImpl::ExecuteTransitionAndGetNextState(
    Session* session, FSMState session_state, FSMEvent event) {
  // Note: since we're not tracking the state of the recognizer object, rather
  // we're directly retrieving it (through GetSessionState), we see its events
  // (that are AUDIO_ENDED and RECOGNITION_ENDED) after its state evolution
  // (e.g., when we receive the AUDIO_ENDED event, the recognizer has just
  // completed the transition from CAPTURING_AUDIO to WAITING_FOR_RESULT, thus
  // we perceive the AUDIO_ENDED event in WAITING_FOR_RESULT).
  // This makes the code below a bit tricky but avoids a lot of code for
  // tracking and reconstructing asynchronously the state of the recognizer.
  switch (session_state) {
    case SESSION_STATE_IDLE:
      switch (event) {
        case EVENT_START:
          return SessionStart(*session);
        case EVENT_ABORT:
          return SessionAbort(*session);
        case EVENT_RECOGNITION_ENDED:
          return SessionDelete(session);
        case EVENT_STOP_CAPTURE:
          return SessionStopAudioCapture(*session);
        case EVENT_AUDIO_ENDED:
          return;
      }
      break;
    case SESSION_STATE_CAPTURING_AUDIO:
      switch (event) {
        case EVENT_STOP_CAPTURE:
          return SessionStopAudioCapture(*session);
        case EVENT_ABORT:
          return SessionAbort(*session);
        case EVENT_START:
          return;
        case EVENT_AUDIO_ENDED:
        case EVENT_RECOGNITION_ENDED:
          return NotFeasible(*session, event);
      }
      break;
    case SESSION_STATE_WAITING_FOR_RESULT:
      switch (event) {
        case EVENT_ABORT:
          return SessionAbort(*session);
        case EVENT_AUDIO_ENDED:
          return ResetCapturingSessionId(*session);
        case EVENT_START:
        case EVENT_STOP_CAPTURE:
          return;
        case EVENT_RECOGNITION_ENDED:
          return NotFeasible(*session, event);
      }
      break;
  }
  return NotFeasible(*session, event);
}

SpeechRecognitionManagerImpl::FSMState
SpeechRecognitionManagerImpl::GetSessionState(int session_id) const {
  Session* session = GetSession(session_id);
  if (!session->recognizer.get() || !session->recognizer->IsActive())
    return SESSION_STATE_IDLE;
  if (session->recognizer->IsCapturingAudio())
    return SESSION_STATE_CAPTURING_AUDIO;
  return SESSION_STATE_WAITING_FOR_RESULT;
}

// ----------- Contract for all the FSM evolution functions below -------------
//  - Are guaranteed to be executed in the IO thread;
//  - Are guaranteed to be not reentrant (themselves and each other);

void SpeechRecognitionManagerImpl::SessionStart(const Session& session) {
  DCHECK_EQ(primary_session_id_, session.id);
  const blink::MediaStreamDevices& devices = session.context.devices;
  std::string device_id;
  if (devices.empty()) {
    // From the ask_user=false path, use the default device.
    // TODO(xians): Abort the session after we do not need to support this path
    // anymore.
    device_id = media::AudioDeviceDescription::kDefaultDeviceId;
  } else {
    // From the ask_user=true path, use the selected device.
    DCHECK_EQ(1u, devices.size());
    DCHECK_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              devices.front().type);
    device_id = devices.front().id;
  }

  session.recognizer->StartRecognition(device_id);
}

void SpeechRecognitionManagerImpl::SessionAbort(const Session& session) {
  if (primary_session_id_ == session.id)
    primary_session_id_ = kSessionIDInvalid;
  DCHECK(session.recognizer.get());
  session.recognizer->AbortRecognition();
}

void SpeechRecognitionManagerImpl::SessionStopAudioCapture(
    const Session& session) {
  DCHECK(session.recognizer.get());
  session.recognizer->StopAudioCapture();
}

void SpeechRecognitionManagerImpl::ResetCapturingSessionId(
    const Session& session) {
  DCHECK_EQ(primary_session_id_, session.id);
  primary_session_id_ = kSessionIDInvalid;
}

void SpeechRecognitionManagerImpl::SessionDelete(Session* session) {
  DCHECK(session->recognizer.get() == nullptr ||
         !session->recognizer->IsActive());
  if (primary_session_id_ == session->id)
    primary_session_id_ = kSessionIDInvalid;
  if (!session->context.label.empty())
    media_stream_manager_->CancelRequest(session->context.label);
  sessions_.erase(session->id);
}

void SpeechRecognitionManagerImpl::NotFeasible(const Session& session,
                                               FSMEvent event) {
  NOTREACHED() << "Unfeasible event " << event
               << " in state " << GetSessionState(session.id)
               << " for session " << session.id;
}

int SpeechRecognitionManagerImpl::GetNextSessionID() {
  ++last_session_id_;
  // Deal with wrapping of last_session_id_. (How civilized).
  if (last_session_id_ <= 0)
    last_session_id_ = 1;
  return last_session_id_;
}

bool SpeechRecognitionManagerImpl::SessionExists(int session_id) const {
  return sessions_.find(session_id) != sessions_.end();
}

SpeechRecognitionManagerImpl::Session*
SpeechRecognitionManagerImpl::GetSession(int session_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = sessions_.find(session_id);
  DCHECK(iter != sessions_.end());
  return iter->second.get();
}

SpeechRecognitionEventListener* SpeechRecognitionManagerImpl::GetListener(
    int session_id) const {
  Session* session = GetSession(session_id);
  if (session->config.event_listener)
    return session->config.event_listener.get();
  return nullptr;
}

SpeechRecognitionEventListener*
SpeechRecognitionManagerImpl::GetDelegateListener() const {
  return delegate_.get() ? delegate_->GetEventListener() : nullptr;
}

const SpeechRecognitionSessionConfig&
SpeechRecognitionManagerImpl::GetSessionConfig(int session_id) {
  return GetSession(session_id)->config;
}

SpeechRecognitionManagerImpl::Session::Session()
    : id(kSessionIDInvalid), abort_requested(false) {}

SpeechRecognitionManagerImpl::Session::~Session() {
}

}  // namespace content
