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
#include "content/browser/speech/speech_recognition_session.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/constants.mojom.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/soda/soda_util.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {

// static
std::string SpeechRecognitionDispatcherHost::GetAcceptedLanguages(
    const std::string& language,
    const std::string& accept_language) {
  std::string langs = language;
  if (langs.empty() && !accept_language.empty()) {
    // If no language is provided then we use the first from the accepted
    // language list. If this list is empty then it defaults to "en-US".
    // Example of the contents of this list: "es,en-GB;q=0.8", ""
    size_t separator = accept_language.find_first_of(",;");
    if (separator != std::string::npos) {
      langs = accept_language.substr(0, separator);
    } else {
      langs = accept_language;
    }
  }
  if (langs.empty()) {
    langs = "en-US";
  }
  return langs;
}

SpeechRecognitionDispatcherHost::SpeechRecognitionDispatcherHost(
    GlobalRenderFrameHostId global_id)
    : global_id_(global_id) {
  // Do not add any non-trivial initialization here, instead do it lazily when
  // required (e.g. see the method |SpeechRecognitionManager::GetInstance()|) or
  // add an Init() method.
}

// static
void SpeechRecognitionDispatcherHost::Create(
    GlobalRenderFrameHostId global_id,
    mojo::PendingReceiver<media::mojom::SpeechRecognizer> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SpeechRecognitionDispatcherHost>(global_id),
      std::move(receiver));
}

SpeechRecognitionDispatcherHost::~SpeechRecognitionDispatcherHost() {}

base::WeakPtr<SpeechRecognitionDispatcherHost>
SpeechRecognitionDispatcherHost::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// -------- media::mojom::SpeechRecognizer interface implementation ------------

void SpeechRecognitionDispatcherHost::Start(
    media::mojom::StartSpeechRecognitionRequestParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (params->audio_forwarder.is_valid()) {
#if BUILDFLAG(IS_ANDROID)
    mojo::Remote<media::mojom::SpeechRecognitionSessionClient> client(
        std::move(params->client));
    client->ErrorOccurred(media::mojom::SpeechRecognitionError::New(
        media::mojom::SpeechRecognitionErrorCode::kNotAllowed,
        media::mojom::SpeechAudioErrorDetails::kNone));
    return;
#else
    if (params->channel_count <= 0) {
      mojo::ReportBadMessage("Channel count must be positive.");
      return;
    }
    if (params->sample_rate <= 0) {
      mojo::ReportBadMessage("Sample rate must be positive.");
      return;
    }
#endif
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionDispatcherHost::StartRequestOnUI,
                     AsWeakPtr(), global_id_, std::move(params)));
}

// static
void SpeechRecognitionDispatcherHost::StartRequestOnUI(
    base::WeakPtr<SpeechRecognitionDispatcherHost>
        speech_recognition_dispatcher_host,
    GlobalRenderFrameHostId global_id,
    media::mojom::StartSpeechRecognitionRequestParamsPtr params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GlobalRenderFrameHostId embedder_global_id;

  RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(global_id);
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

    embedder_global_id = embedder_frame->GetGlobalId();
    DCHECK(embedder_global_id.child_id);
    DCHECK(embedder_global_id);
  }

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  content::RenderFrameHost* main_frame = rfh->GetMainFrame();
  StoragePartition* storage_partition = main_frame->GetStoragePartition();

  bool is_valid_storage_context =
      !main_frame->GetSiteInstance()->GetSecurityPrincipal().IsGuest() &&
      (storage_partition == browser_context->GetDefaultStoragePartition() ||
       !main_frame->GetLastCommittedURL().SchemeIsHTTPOrHTTPS());
  bool is_policy_enabled = rfh->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kOnDeviceSpeechRecognition);

  bool can_render_frame_use_on_device =
      is_valid_storage_context && is_policy_enabled;
  const std::string& language =
      SpeechRecognitionDispatcherHost::GetAcceptedLanguages(
          params->language,
          GetContentClient()->browser()->GetAcceptLangs(browser_context));

#if !BUILDFLAG(IS_ANDROID)
  bool on_device_available =
      GetContentClient()
          ->browser()
          ->GetOnDeviceSpeechRecognitionAvailabilityStatus(
              browser_context, language, params->quality) ==
      media::mojom::AvailabilityStatus::kAvailable;
#else
  bool on_device_available = false;
#endif  // !BUILDFLAG(IS_ANDROID)

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SpeechRecognitionDispatcherHost::StartSessionOnIO,
          speech_recognition_dispatcher_host, std::move(params),
          embedder_global_id, rfh->GetLastCommittedOrigin(),
          storage_partition->GetURLLoaderFactoryForBrowserProcessIOThread(),
          language, can_render_frame_use_on_device, on_device_available));
}

void SpeechRecognitionDispatcherHost::StartSessionOnIO(
    media::mojom::StartSpeechRecognitionRequestParamsPtr params,
    GlobalRenderFrameHostId embedder_global_id,
    const url::Origin& origin,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_shared_url_loader_factory,
    const std::string& language,
    bool can_render_frame_use_on_device,
    bool on_device_available) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SpeechRecognitionSessionContext context;
  context.security_origin = origin;
  context.global_id = global_id_;
  context.embedder_global_id = embedder_global_id;

  SpeechRecognitionSessionConfig config;
  config.language = language;
  config.max_hypotheses = params->max_hypotheses;
  config.origin = origin;
  config.initial_context = context;
  config.shared_url_loader_factory = network::SharedURLLoaderFactory::Create(
      std::move(pending_shared_url_loader_factory));
  config.filter_profanities = false;
  config.continuous = params->continuous;
  config.interim_results = params->interim_results;
  config.unspoken_punctuation = params->unspoken_punctuation;
  config.on_device = params->on_device;
  config.on_device_available = on_device_available;
  config.allow_cloud_fallback = params->allow_cloud_fallback;
  config.quality = params->quality;
  config.recognition_context = params->recognition_context;

  for (media::mojom::SpeechRecognitionGrammarPtr& grammar_ptr :
       params->grammars) {
    config.grammars.push_back(*grammar_ptr);
  }

  if (SpeechRecognitionManager::GetInstance()->UseOnDeviceSpeechRecognition(
          config) &&
      params->audio_forwarder.is_valid() &&
      !SpeechRecognitionManagerImpl::IsOptimizationGuideSpeechModel(config)) {
    // Use on-device speech recognition, bypassing the browser process. The
    // speech recognition session will live in the speech recognition service
    // process.
    CreateSession(config, std::move(params->session_receiver),
                  std::move(params->client),
                  std::make_optional<SpeechRecognitionAudioForwarderConfig>(
                      std::move(params->audio_forwarder), params->channel_count,
                      params->sample_rate),
                  can_render_frame_use_on_device);
  } else {
    // Create the speech recognition session in the browser if cloud-based
    // speech recognition is used or if microphone audio input is used.
    auto session =
        std::make_unique<SpeechRecognitionSession>(std::move(params->client));
    config.event_listener = session->AsWeakPtr();

    int session_id = CreateSession(
        config, mojo::NullReceiver(), mojo::NullRemote(),
        params->audio_forwarder.is_valid()
            ? std::make_optional<SpeechRecognitionAudioForwarderConfig>(
                  std::move(params->audio_forwarder), params->channel_count,
                  params->sample_rate)
            : std::nullopt,
        can_render_frame_use_on_device);
    DCHECK_NE(session_id, SpeechRecognitionManager::kSessionIDInvalid);
    session->SetSessionId(session_id);
    mojo::MakeSelfOwnedReceiver(std::move(session),
                                std::move(params->session_receiver));

    SpeechRecognitionManager::GetInstance()->StartSession(session_id);
  }
}
int SpeechRecognitionDispatcherHost::CreateSession(
    const SpeechRecognitionSessionConfig& config,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        client_remote,
    std::optional<SpeechRecognitionAudioForwarderConfig> audio_forwarder_config,
    bool can_render_frame_use_on_device) {
  bool use_fake_manager = SpeechRecognitionManager::GetInstance() !=
                          SpeechRecognitionManagerImpl::GetInstance();
  if (use_fake_manager) {
    return SpeechRecognitionManager::GetInstance()->CreateSession(
        config, std::move(session_receiver), std::move(client_remote),
        audio_forwarder_config.has_value()
            ? std::make_optional<SpeechRecognitionAudioForwarderConfig>(
                  audio_forwarder_config.value())
            : std::nullopt);
  }

  return SpeechRecognitionManagerImpl::GetInstance()->CreateSession(
      config, std::move(session_receiver), std::move(client_remote),
      audio_forwarder_config.has_value()
          ? std::make_optional<SpeechRecognitionAudioForwarderConfig>(
                audio_forwarder_config.value())
          : std::nullopt,
      can_render_frame_use_on_device);
}

}  // namespace content
