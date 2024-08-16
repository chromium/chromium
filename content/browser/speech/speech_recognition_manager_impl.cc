// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_manager_impl.h"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/soda/soda_util.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/speech/network_speech_recognition_engine_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/speech/speech_recognizer_impl_android.h"
#elif !BUILDFLAG(IS_FUCHSIA)
#include "components/soda/constants.h"
#include "components/soda/soda_util.h"
#include "content/browser/speech/soda_speech_recognition_engine_impl.h"
#include "media/base/media_switches.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

SpeechRecognitionManager* SpeechRecognitionManager::manager_for_tests_;

namespace {

SpeechRecognitionManagerImpl* g_speech_recognition_manager_impl;

}  // namespace

int SpeechRecognitionManagerImpl::next_requester_id_ = 0;

class FrameSessionTracker
    : public content::DocumentUserData<FrameSessionTracker> {
 public:
  using FrameDeletedCallback =
      base::RepeatingCallback<void(int /* session_id */)>;

  ~FrameSessionTracker() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    for (auto session : sessions_) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(frame_deleted_callback_, session));
    }
  }

  static void CreateObserverForSession(int render_process_id,
                                       int render_frame_id,
                                       int session_id,
                                       FrameDeletedCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RenderFrameHost* render_frame_host =
        RenderFrameHost::FromID(render_process_id, render_frame_id);
    if (!render_frame_host)
      return;

    FrameSessionTracker* tracker =
        GetOrCreateForCurrentDocument(render_frame_host);

    // This will clobber any previously set callback but it will always
    // be the same binding.
    tracker->SetCallback(std::move(callback));
    tracker->AddSession(session_id);
  }

  static void RemoveObserverForSession(int render_process_id,
                                       int render_frame_id,
                                       int session_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RenderFrameHost* render_frame_host =
        RenderFrameHost::FromID(render_process_id, render_frame_id);
    if (!render_frame_host)
      return;

    FrameSessionTracker* tracker = GetForCurrentDocument(render_frame_host);
    if (!tracker)
      return;
    tracker->RemoveSession(session_id);
  }

 private:
  explicit FrameSessionTracker(content::RenderFrameHost* rfh)
      : DocumentUserData<FrameSessionTracker>(rfh) {}

  friend class content::DocumentUserData<FrameSessionTracker>;
  DOCUMENT_USER_DATA_KEY_DECL();

  void AddSession(int session_id) { sessions_.insert(session_id); }

  void RemoveSession(int session_id) { sessions_.erase(session_id); }

  void SetCallback(FrameDeletedCallback callback) {
    frame_deleted_callback_ = std::move(callback);
  }

  FrameDeletedCallback frame_deleted_callback_;
  std::set<int> sessions_;
};

DOCUMENT_USER_DATA_KEY_IMPL(FrameSessionTracker);

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

bool SpeechRecognitionManagerImpl::IsOnDeviceSpeechRecognitionAvailable(
    const SpeechRecognitionSessionConfig& config) {
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  return speech::IsOnDeviceSpeechRecognitionAvailable(config.language);
#else
  return false;
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
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
}

SpeechRecognitionManagerImpl::~SpeechRecognitionManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(g_speech_recognition_manager_impl);

  g_speech_recognition_manager_impl = nullptr;
}

int SpeechRecognitionManagerImpl::CreateSession(
    const SpeechRecognitionSessionConfig& config) {
  return CreateSession(std::move(config), mojo::NullReceiver(),
                       mojo::NullRemote(), std::nullopt);
}

int SpeechRecognitionManagerImpl::CreateSession(
    const SpeechRecognitionSessionConfig& config,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        client_remote,
    std::optional<SpeechRecognitionAudioForwarderConfig>
        audio_forwarder_config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const int session_id = GetNextSessionID();
  DCHECK(!SessionExists(session_id));

  // If on-device speech recognition must be used but is not available, throw a
  // language-not-supported error and don't create the session.
  if (config.on_device && !config.allow_cloud_fallback &&
      !IsOnDeviceSpeechRecognitionAvailable(config)) {
    mojo::Remote<media::mojom::SpeechRecognitionSessionClient> client(
        std::move(client_remote));
    client->ErrorOccurred(media::mojom::SpeechRecognitionError::New(
        media::mojom::SpeechRecognitionErrorCode::kLanguageNotSupported,
        media::mojom::SpeechAudioErrorDetails::kNone));
    client->Ended();
    return session_id;
  }

  // Set-up the new session.
  auto session = std::make_unique<Session>();
  session->id = session_id;
  session->config = config;
  session->context = config.initial_context;
  session->use_microphone = !audio_forwarder_config.has_value();

#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_FUCHSIA)
  if (IsOnDeviceSpeechRecognitionAvailable(config) &&
      audio_forwarder_config.has_value()) {
    CHECK_GT(audio_forwarder_config.value().channel_count, 0);
    CHECK_GT(audio_forwarder_config.value().sample_rate, 0);
    // The speech recognition service process will create and manage the speech
    // recognition session instead of the browser. Raw audio will be passed
    // directly to the speech recognition process and speech recognition events
    // will be returned directly to the renderer, bypassing the browser
    // entirely.
    if (!speech_recognition_context_.is_bound()) {
      raw_ptr<SpeechRecognitionManagerDelegate>
          speech_recognition_mgr_delegate =
              SpeechRecognitionManagerImpl::GetInstance()
                  ? SpeechRecognitionManagerImpl::GetInstance()->delegate()
                  : nullptr;

      CHECK(speech_recognition_mgr_delegate);
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
          speech_recognition_context_receiver =
              speech_recognition_context_.BindNewPipeAndPassReceiver();
      speech_recognition_mgr_delegate->BindSpeechRecognitionContext(
          std::move(speech_recognition_context_receiver));
    }

    media::mojom::SpeechRecognitionOptionsPtr options =
        media::mojom::SpeechRecognitionOptions::New();
    options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
    options->enable_formatting = false;
    options->recognizer_client_type =
        media::mojom::RecognizerClientType::kLiveCaption;
    options->skip_continuously_empty_audio = true;

    speech_recognition_context_->BindWebSpeechRecognizer(
        std::move(session_receiver), std::move(client_remote),
        std::move(audio_forwarder_config.value().audio_forwarder),
        audio_forwarder_config.value().channel_count,
        audio_forwarder_config.value().sample_rate, std::move(options),
        config.continuous);

    // The session is managed by the speech recognition service directly thus
    // does not need to be associated with a session id in the browser.
    return 0;
  }
#endif  //! BUILDFLAG(IS_FUCHSIA)

  std::unique_ptr<SpeechRecognitionEngine> speech_recognition_engine;

#if !BUILDFLAG(IS_FUCHSIA)
  if (UseOnDeviceSpeechRecognition(config)) {
    std::unique_ptr<SodaSpeechRecognitionEngineImpl>
        soda_speech_recognition_engine =
            std::make_unique<SodaSpeechRecognitionEngineImpl>(config);
    if (soda_speech_recognition_engine->Initialize()) {
      speech_recognition_engine = std::move(soda_speech_recognition_engine);
    }
  }
#endif  //! BUILDFLAG(IS_FUCHSIA)

  if (!speech_recognition_engine) {
    // A NetworkSpeechRecognitionEngineImpl (and corresponding Config) is
    // required only when using SpeechRecognizerImpl, which performs the audio
    // capture and endpointing in the browser. This is not the case of Android
    // where, not only the speech recognition, but also the audio capture and
    // endpointing activities performed outside of the browser (delegated via
    // JNI to the Android API implementation).

    NetworkSpeechRecognitionEngineImpl::Config remote_engine_config;
    remote_engine_config.language = config.language;
    remote_engine_config.grammars = config.grammars;
    remote_engine_config.audio_sample_rate =
        audio_forwarder_config.has_value()
            ? audio_forwarder_config.value().sample_rate
            : SpeechRecognizerImpl::kAudioSampleRate;
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

    std::unique_ptr<NetworkSpeechRecognitionEngineImpl> google_remote_engine =
        std::make_unique<NetworkSpeechRecognitionEngineImpl>(
            config.shared_url_loader_factory, config.accept_language);
    google_remote_engine->SetConfig(remote_engine_config);
    speech_recognition_engine = std::move(google_remote_engine);
  }

  session->recognizer = new SpeechRecognizerImpl(
      this, audio_system_, session_id, config.continuous,
      config.interim_results, std::move(speech_recognition_engine),
      audio_forwarder_config.has_value()
          ? std::make_optional<SpeechRecognitionAudioForwarderConfig>(
                audio_forwarder_config.value())
          : std::nullopt);

#else
  session->recognizer = new SpeechRecognizerImplAndroid(this, session_id);
#endif  //! BUILDFLAG(IS_ANDROID)

  sessions_[session_id] = std::move(session);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FrameSessionTracker::CreateObserverForSession,
          config.initial_context.render_process_id,
          config.initial_context.render_frame_id, session_id,
          base::BindRepeating(&SpeechRecognitionManagerImpl::AbortSessionImpl,
                              weak_factory_.GetWeakPtr())));

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

  if (!sessions_[session_id]->use_microphone) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                       weak_factory_.GetWeakPtr(), session_id, EVENT_START));

  } else if (delegate_) {
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
        {context.render_process_id, context.render_frame_id}, requester_id_,
        session_id, blink::StreamControls(true, false), context.security_origin,
        base::BindOnce(
            &SpeechRecognitionManagerImpl::MediaRequestPermissionCallback,
            weak_factory_.GetWeakPtr(), session_id));
    return;
  }

  if (is_allowed) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                       weak_factory_.GetWeakPtr(), session_id, EVENT_START));
  } else {
    OnRecognitionError(
        session_id, media::mojom::SpeechRecognitionError(
                        media::mojom::SpeechRecognitionErrorCode::kNotAllowed,
                        media::mojom::SpeechAudioErrorDetails::kNone));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                       weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
  }
}

void SpeechRecognitionManagerImpl::MediaRequestPermissionCallback(
    int session_id,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    std::unique_ptr<MediaStreamUIProxy> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  // The SpeechRecognictionManager is not used with multiple streams
  // which is only supported in combination with the getAllScreensMedia API.
  // The |stream_devices| vector can be empty e.g. if the permission
  // was denied.
  DCHECK_LE(stream_devices_set.stream_devices.size(), 1u);

  blink::MediaStreamDevices devices_list =
      blink::ToMediaStreamDevicesList(stream_devices_set);
  const bool is_allowed = !devices_list.empty();
  if (is_allowed) {
    // Copy the approved devices array to the context for UI indication.
    iter->second->context.devices = devices_list;

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

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FrameSessionTracker::RemoveObserverForSession,
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

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                     weak_factory_.GetWeakPtr(), session_id, EVENT_ABORT));
}

void SpeechRecognitionManagerImpl::StopAudioCaptureForSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto iter = sessions_.find(session_id);
  if (iter == sessions_.end())
    return;

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FrameSessionTracker::RemoveObserverForSession,
                     iter->second->config.initial_context.render_process_id,
                     iter->second->config.initial_context.render_frame_id,
                     session_id));

  iter->second->ui.reset();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    iter->second->ui->OnStarted(
        base::OnceClosure(), MediaStreamUI::SourceCallback(),
        MediaStreamUIProxy::WindowIdCallback(), /*label=*/std::string(),
        /*screen_capture_ids=*/{}, MediaStreamUI::StateChangeCallback());
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_AUDIO_ENDED));
}

void SpeechRecognitionManagerImpl::OnRecognitionResults(
    int session_id,
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results) {
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
    const media::mojom::SpeechRecognitionError& error) {
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SpeechRecognitionManagerImpl::DispatchEvent,
                                weak_factory_.GetWeakPtr(), session_id,
                                EVENT_RECOGNITION_ENDED));
}

SpeechRecognitionSessionContext SpeechRecognitionManagerImpl::GetSessionContext(
    int session_id) {
  return GetSession(session_id)->context;
}

bool SpeechRecognitionManagerImpl::UseOnDeviceSpeechRecognition(
    const SpeechRecognitionSessionConfig& config) {
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  return config.on_device &&
         (speech::IsOnDeviceSpeechRecognitionAvailable(config.language) ||
          !config.allow_cloud_fallback);
#else
  return false;
#endif
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
  NOTREACHED_IN_MIGRATION()
      << "Unfeasible event " << event << " in state "
      << GetSessionState(session.id) << " for session " << session.id;
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
  CHECK(iter != sessions_.end(), base::NotFatalUntil::M130);
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
