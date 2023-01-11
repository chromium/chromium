// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_dispatcher_host.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

SpeechRecognitionDispatcherHost::SpeechRecognitionDispatcherHost(
    int render_process_id,
    int render_frame_id)
    : render_process_id_(render_process_id), render_frame_id_(render_frame_id) {
  // Do not add any non-trivial initialization here, instead do it lazily when
  // required (e.g. see the method |SpeechRecognitionManager::GetInstance()|) or
  // add an Init() method.
}

// static
void SpeechRecognitionDispatcherHost::Create(
    int render_process_id,
    int render_frame_id,
    mojo::PendingReceiver<blink::mojom::SpeechRecognizer> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<SpeechRecognitionDispatcherHost>(
                                  render_process_id, render_frame_id),
                              std::move(receiver));
}

SpeechRecognitionDispatcherHost::~SpeechRecognitionDispatcherHost() {}

base::WeakPtr<SpeechRecognitionDispatcherHost>
SpeechRecognitionDispatcherHost::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// -------- blink::mojom::SpeechRecognizer interface implementation ------------

void SpeechRecognitionDispatcherHost::Start(
    blink::mojom::StartSpeechRecognitionRequestParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionDispatcherHost::StartRequestOnUI,
                     AsWeakPtr(), render_process_id_, render_frame_id_,
                     std::move(params)));
}

// static
void SpeechRecognitionDispatcherHost::StartRequestOnUI(
    base::WeakPtr<SpeechRecognitionDispatcherHost>
        speech_recognition_dispatcher_host,
    int render_process_id,
    int render_frame_id,
    blink::mojom::StartSpeechRecognitionRequestParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  int embedder_render_process_id = 0;
  int embedder_render_frame_id = MSG_ROUTING_NONE;

  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!rfh) {
    DLOG(ERROR) << "SRDH::OnStartRequest, invalid frame";
    return;
  }
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(rfh));

  // Disable BackForwardCache when using the SpeechRecognition feature, because
  // currently we do not handle speech recognition after placing the page in
  // BackForwardCache.
  // TODO(sreejakshetty): Make SpeechRecognition compatible with
  // BackForwardCache.
  rfh->OnBackForwardCacheDisablingStickyFeatureUsed(
      blink::scheduler::WebSchedulerTrackedFeature::kSpeechRecognizer);

  // If the speech API request was from an inner WebContents or a guest, save
  // the context of the outer WebContents or the embedder since we will use it
  // to decide permission.
  WebContents* outer_web_contents = web_contents->GetOuterWebContents();
  if (outer_web_contents) {
    RenderFrameHost* embedder_frame = nullptr;

    FrameTreeNode* embedder_frame_node = web_contents->GetPrimaryMainFrame()
                                             ->frame_tree_node()
                                             ->render_manager()
                                             ->GetOuterDelegateNode();
    if (embedder_frame_node) {
      embedder_frame = embedder_frame_node->current_frame_host();
    } else {
      // The outer web contents is embedded using the browser plugin. Fall back
      // to a simple lookup of the main frame. TODO(avi): When the browser
      // plugin is retired, remove this code.
      embedder_frame = outer_web_contents->GetPrimaryMainFrame();
    }

    embedder_render_process_id = embedder_frame->GetProcess()->GetID();
    DCHECK_NE(embedder_render_process_id, 0);
    embedder_render_frame_id = embedder_frame->GetRoutingID();
    DCHECK_NE(embedder_render_frame_id, MSG_ROUTING_NONE);
  }

  bool filter_profanities =
      SpeechRecognitionManagerImpl::GetInstance() &&
      SpeechRecognitionManagerImpl::GetInstance()->delegate() &&
      SpeechRecognitionManagerImpl::GetInstance()
          ->delegate()
          ->FilterProfanities(embedder_render_process_id);

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  StoragePartition* storage_partition =
      browser_context->GetStoragePartition(web_contents->GetSiteInstance());

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SpeechRecognitionDispatcherHost::StartSessionOnIO,
          speech_recognition_dispatcher_host, std::move(params),
          embedder_render_process_id, embedder_render_frame_id,
          rfh->GetLastCommittedOrigin(), filter_profanities,
          storage_partition->GetURLLoaderFactoryForBrowserProcessIOThread(),
          GetContentClient()->browser()->GetAcceptLangs(browser_context)));
}

void SpeechRecognitionDispatcherHost::StartSessionOnIO(
    blink::mojom::StartSpeechRecognitionRequestParamsPtr params,
    int embedder_render_process_id,
    int embedder_render_frame_id,
    const url::Origin& origin,
    bool filter_profanities,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_shared_url_loader_factory,
    const std::string& accept_language) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SpeechRecognitionSessionContext context;
  context.security_origin = origin;
  context.render_process_id = render_process_id_;
  context.render_frame_id = render_frame_id_;
  context.embedder_render_process_id = embedder_render_process_id;
  context.embedder_render_frame_id = embedder_render_frame_id;

  auto session =
      std::make_unique<SpeechRecognitionSession>(std::move(params->client));

  SpeechRecognitionSessionConfig config;
  config.language = params->language;
  config.accept_language = accept_language;
  config.max_hypotheses = params->max_hypotheses;
  config.origin = origin;
  config.initial_context = context;
  config.shared_url_loader_factory = network::SharedURLLoaderFactory::Create(
      std::move(pending_shared_url_loader_factory));
  config.filter_profanities = filter_profanities;
  config.continuous = params->continuous;
  config.interim_results = params->interim_results;
  config.event_listener = session->AsWeakPtr();

  for (blink::mojom::SpeechRecognitionGrammarPtr& grammar_ptr :
       params->grammars) {
    config.grammars.push_back(*grammar_ptr);
  }

  int session_id =
      SpeechRecognitionManager::GetInstance()->CreateSession(config);
  DCHECK_NE(session_id, SpeechRecognitionManager::kSessionIDInvalid);
  session->SetSessionId(session_id);
  mojo::MakeSelfOwnedReceiver(std::move(session),
                              std::move(params->session_receiver));

  SpeechRecognitionManager::GetInstance()->StartSession(session_id);
}

// ---------------------- SpeechRecognizerSession -----------------------------

SpeechRecognitionSession::SpeechRecognitionSession(
    mojo::PendingRemote<blink::mojom::SpeechRecognitionSessionClient> client)
    : session_id_(SpeechRecognitionManager::kSessionIDInvalid),
      client_(std::move(client)),
      stopped_(false) {
  client_.set_disconnect_handler(
      base::BindOnce(&SpeechRecognitionSession::ConnectionErrorHandler,
                     base::Unretained(this)));
}

SpeechRecognitionSession::~SpeechRecognitionSession() {
  // If a connection error happens and the session hasn't been stopped yet,
  // abort it.
  if (!stopped_)
    Abort();
}

base::WeakPtr<SpeechRecognitionSession> SpeechRecognitionSession::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SpeechRecognitionSession::Abort() {
  SpeechRecognitionManager::GetInstance()->AbortSession(session_id_);
  stopped_ = true;
}

void SpeechRecognitionSession::StopCapture() {
  SpeechRecognitionManager::GetInstance()->StopAudioCaptureForSession(
      session_id_);
  stopped_ = true;
}

// -------- SpeechRecognitionEventListener interface implementation -----------

void SpeechRecognitionSession::OnRecognitionStart(int session_id) {
  client_->Started();
}

void SpeechRecognitionSession::OnAudioStart(int session_id) {
  client_->AudioStarted();
}

void SpeechRecognitionSession::OnSoundStart(int session_id) {
  client_->SoundStarted();
}

void SpeechRecognitionSession::OnSoundEnd(int session_id) {
  client_->SoundEnded();
}

void SpeechRecognitionSession::OnAudioEnd(int session_id) {
  client_->AudioEnded();
}

void SpeechRecognitionSession::OnRecognitionEnd(int session_id) {
  client_->Ended();
  stopped_ = true;
  client_.reset();
}

void SpeechRecognitionSession::OnRecognitionResults(
    int session_id,
    const std::vector<blink::mojom::SpeechRecognitionResultPtr>& results) {
  client_->ResultRetrieved(mojo::Clone(results));
}

void SpeechRecognitionSession::OnRecognitionError(
    int session_id,
    const blink::mojom::SpeechRecognitionError& error) {
  if (!client_.is_bound())
    return;
  client_->ErrorOccurred(blink::mojom::SpeechRecognitionError::New(error));
}

// The events below are currently not used by speech JS APIs implementation.
void SpeechRecognitionSession::OnAudioLevelsChange(int session_id,
                                                   float volume,
                                                   float noise_volume) {}

void SpeechRecognitionSession::OnEnvironmentEstimationComplete(int session_id) {
}

void SpeechRecognitionSession::ConnectionErrorHandler() {
  if (!stopped_)
    Abort();
}

}  // namespace content
