// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals.h"

#include <stddef.h>

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_manager_connection.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_log_event.h"
#include "media/webrtc/webrtc_switches.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/sandbox/features.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if !defined(OS_ANDROID)
#include "media/filters/decrypting_video_decoder.h"
#endif

namespace {

base::string16 SerializeUpdate(const std::string& function,
                               const base::Value* value) {
  return content::WebUI::GetJavascriptCall(
      function, std::vector<const base::Value*>(1, value));
}

std::string EffectsToString(int effects) {
  if (effects == media::AudioParameters::NO_EFFECTS)
    return "NO_EFFECTS";

  struct {
    int flag;
    const char* name;
  } flags[] = {
      {media::AudioParameters::ECHO_CANCELLER, "ECHO_CANCELLER"},
      {media::AudioParameters::DUCKING, "DUCKING"},
      {media::AudioParameters::KEYBOARD_MIC, "KEYBOARD_MIC"},
      {media::AudioParameters::HOTWORD, "HOTWORD"},
  };

  std::string ret;
  for (size_t i = 0; i < base::size(flags); ++i) {
    if (effects & flags[i].flag) {
      if (!ret.empty())
        ret += " | ";
      ret += flags[i].name;
      effects &= ~flags[i].flag;
    }
  }

  if (effects) {
    if (!ret.empty())
      ret += " | ";
    ret += base::NumberToString(effects);
  }

  return ret;
}

std::string FormatToString(media::AudioParameters::Format format) {
  switch (format) {
    case media::AudioParameters::AUDIO_PCM_LINEAR:
      return "pcm_linear";
    case media::AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return "pcm_low_latency";
    case media::AudioParameters::AUDIO_BITSTREAM_AC3:
      return "ac3";
    case media::AudioParameters::AUDIO_BITSTREAM_EAC3:
      return "eac3";
    case media::AudioParameters::AUDIO_FAKE:
      return "fake";
  }

  NOTREACHED();
  return "unknown";
}

const char kAudioLogStatusKey[] = "status";
const char kAudioLogUpdateFunction[] = "media.updateAudioComponent";

}  // namespace

namespace content {

class MediaInternals::AudioLogImpl : public media::mojom::AudioLog,
                                     public media::AudioLog {
 public:
  AudioLogImpl(int owner_id,
               media::AudioLogFactory::AudioComponent component,
               content::MediaInternals* media_internals,
               int component_id,
               int render_process_id,
               int render_frame_id);
  ~AudioLogImpl() override;

  void OnCreated(const media::AudioParameters& params,
                 const std::string& device_id) override;
  void OnStarted() override;
  void OnStopped() override;
  void OnClosed() override;
  void OnError() override;
  void OnSetVolume(double volume) override;
  void OnLogMessage(const std::string& message) override;
  void OnProcessingStateChanged(const std::string& message) override;

 private:
  // If possible, i.e. a WebContents exists for the given RenderFrameHostID,
  // tells an existing AudioLogEntry the WebContents title for easier
  // differentiation on the UI. Note that the log entry must be created (by
  // calling OnCreated() before calling this method.
  void SetWebContentsTitle();

  void SendSingleStringUpdate(const std::string& key, const std::string& value);
  void StoreComponentMetadata(base::DictionaryValue* dict);
  std::string FormatCacheKey();

  static void SendWebContentsTitleHelper(
      const std::string& cache_key,
      std::unique_ptr<base::DictionaryValue> dict,
      int render_process_id,
      int render_frame_id);

  const int owner_id_;
  const media::AudioLogFactory::AudioComponent component_;
  content::MediaInternals* const media_internals_;
  const int component_id_;
  const int render_process_id_;
  const int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(AudioLogImpl);
};

MediaInternals::AudioLogImpl::AudioLogImpl(
    int owner_id,
    media::AudioLogFactory::AudioComponent component,
    content::MediaInternals* media_internals,
    int component_id,
    int render_process_id,
    int render_frame_id)
    : owner_id_(owner_id),
      component_(component),
      media_internals_(media_internals),
      component_id_(component_id),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}

MediaInternals::AudioLogImpl::~AudioLogImpl() {
  // Ensure log is always closed. This covers the case of crashes in the audio
  // service utility process, in which case the log may not be closed
  // explicitly.
  OnClosed();
}

void MediaInternals::AudioLogImpl::OnCreated(
    const media::AudioParameters& params,
    const std::string& device_id) {
  base::DictionaryValue dict;
  StoreComponentMetadata(&dict);

  dict.SetString(kAudioLogStatusKey, "created");
  dict.SetString("device_id", device_id);
  dict.SetString("device_type", FormatToString(params.format()));
  dict.SetInteger("frames_per_buffer", params.frames_per_buffer());
  dict.SetInteger("sample_rate", params.sample_rate());
  dict.SetInteger("channels", params.channels());
  dict.SetString("channel_layout",
                 ChannelLayoutToString(params.channel_layout()));
  dict.SetString("effects", EffectsToString(params.effects()));

  media_internals_->UpdateAudioLog(MediaInternals::CREATE, FormatCacheKey(),
                                   kAudioLogUpdateFunction, &dict);
  SetWebContentsTitle();
}

void MediaInternals::AudioLogImpl::OnStarted() {
  SendSingleStringUpdate(kAudioLogStatusKey, "started");
}

void MediaInternals::AudioLogImpl::OnStopped() {
  SendSingleStringUpdate(kAudioLogStatusKey, "stopped");
}

void MediaInternals::AudioLogImpl::OnClosed() {
  base::DictionaryValue dict;
  StoreComponentMetadata(&dict);
  dict.SetString(kAudioLogStatusKey, "closed");
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_AND_DELETE,
                                   FormatCacheKey(), kAudioLogUpdateFunction,
                                   &dict);
}

void MediaInternals::AudioLogImpl::OnError() {
  SendSingleStringUpdate("error_occurred", "true");
}

void MediaInternals::AudioLogImpl::OnSetVolume(double volume) {
  base::DictionaryValue dict;
  StoreComponentMetadata(&dict);
  dict.SetDouble("volume", volume);
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_IF_EXISTS,
                                   FormatCacheKey(), kAudioLogUpdateFunction,
                                   &dict);
}

void MediaInternals::AudioLogImpl::OnProcessingStateChanged(
    const std::string& message) {
  SendSingleStringUpdate("processing state", message);
}

void MediaInternals::AudioLogImpl::OnLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog(message);
}

void MediaInternals::AudioLogImpl::SetWebContentsTitle() {
  if (render_process_id_ < 0 || render_frame_id_ < 0)
    return;
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  StoreComponentMetadata(dict.get());
  SendWebContentsTitleHelper(FormatCacheKey(), std::move(dict),
                             render_process_id_, render_frame_id_);
}

std::string MediaInternals::AudioLogImpl::FormatCacheKey() {
  return base::StringPrintf("%d:%d:%d", owner_id_, component_, component_id_);
}

// static
void MediaInternals::AudioLogImpl::SendWebContentsTitleHelper(
    const std::string& cache_key,
    std::unique_ptr<base::DictionaryValue> dict,
    int render_process_id,
    int render_frame_id) {
  // Page title information can only be retrieved from the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SendWebContentsTitleHelper, cache_key, std::move(dict),
                       render_process_id, render_frame_id));
    return;
  }

  WebContents* web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_process_id, render_frame_id));
  if (!web_contents)
    return;

  // Note: by this point the given audio log entry could have been destroyed, so
  // we use UPDATE_IF_EXISTS to discard such instances.
  dict->SetInteger("render_process_id", render_process_id);
  dict->SetString("web_contents_title", web_contents->GetTitle());
  MediaInternals::GetInstance()->UpdateAudioLog(
      MediaInternals::UPDATE_IF_EXISTS, cache_key, kAudioLogUpdateFunction,
      dict.get());
}

void MediaInternals::AudioLogImpl::SendSingleStringUpdate(
    const std::string& key,
    const std::string& value) {
  base::DictionaryValue dict;
  StoreComponentMetadata(&dict);
  dict.SetString(key, value);
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_IF_EXISTS,
                                   FormatCacheKey(), kAudioLogUpdateFunction,
                                   &dict);
}

void MediaInternals::AudioLogImpl::StoreComponentMetadata(
    base::DictionaryValue* dict) {
  dict->SetInteger("owner_id", owner_id_);
  dict->SetInteger("component_id", component_id_);
  dict->SetInteger("component_type", component_);
}

MediaInternals* MediaInternals::GetInstance() {
  static content::MediaInternals* internals = new content::MediaInternals();
  return internals;
}

MediaInternals::MediaInternals() : can_update_(false), owner_ids_() {
  // TODO(sandersd): Is there ever a relevant case where TERMINATED is sent
  // without CLOSED also being sent?
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
}

MediaInternals::~MediaInternals() {}

void MediaInternals::Observe(int type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* process = Source<RenderProcessHost>(source).ptr();
  // TODO(sandersd): Send a termination event before clearing the log.
  saved_events_by_process_.erase(process->GetID());
}

// Converts the |event| to a |update|. Returns whether the conversion succeeded.
static bool ConvertEventToUpdate(int render_process_id,
                                 const media::MediaLogEvent& event,
                                 base::string16* update) {
  DCHECK(update);

  base::DictionaryValue dict;
  dict.SetInteger("renderer", render_process_id);
  dict.SetInteger("player", event.id);
  dict.SetString("type", media::MediaLog::EventTypeToString(event.type));

  // TODO(dalecurtis): This is technically not correct.  TimeTicks "can't" be
  // converted to to a human readable time format.  See base/time/time.h.
  const double ticks = event.time.ToInternalValue();
  const double ticks_millis = ticks / base::Time::kMicrosecondsPerMillisecond;
  dict.SetDouble("ticksMillis", ticks_millis);

  // Convert PipelineStatus to human readable string
  if (event.type == media::MediaLogEvent::PIPELINE_ERROR) {
    int status;
    if (!event.params.GetInteger("pipeline_error", &status) ||
        status < static_cast<int>(media::PIPELINE_OK) ||
        status > static_cast<int>(media::PIPELINE_STATUS_MAX)) {
      return false;
    }
    media::PipelineStatus error = static_cast<media::PipelineStatus>(status);
    dict.SetString("params.pipeline_error",
                   media::PipelineStatusToString(error));
  } else {
    dict.SetKey("params", event.params.Clone());
  }

  *update = SerializeUpdate("media.onMediaEvent", &dict);
  return true;
}

void MediaInternals::OnMediaEvents(
    int render_process_id,
    const std::vector<media::MediaLogEvent>& events) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Notify observers that |event| has occurred.
  for (const auto& event : events) {
    if (CanUpdate()) {
      base::string16 update;
      if (ConvertEventToUpdate(render_process_id, event, &update))
        SendUpdate(update);
    }
    SaveEvent(render_process_id, event);
  }
}

void MediaInternals::AddUpdateCallback(const UpdateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  update_callbacks_.push_back(callback);

  base::AutoLock auto_lock(lock_);
  can_update_ = true;
  audio_focus_helper_.SetEnabled(true);
}

void MediaInternals::RemoveUpdateCallback(const UpdateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (size_t i = 0; i < update_callbacks_.size(); ++i) {
    if (update_callbacks_[i] == callback) {
      update_callbacks_.erase(update_callbacks_.begin() + i);
      break;
    }
  }

  base::AutoLock auto_lock(lock_);
  can_update_ = !update_callbacks_.empty();
  audio_focus_helper_.SetEnabled(can_update_);
}

bool MediaInternals::CanUpdate() {
  base::AutoLock auto_lock(lock_);
  return can_update_;
}

void MediaInternals::SendHistoricalMediaEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& saved_events : saved_events_by_process_) {
    for (const auto& event : saved_events.second) {
      base::string16 update;
      if (ConvertEventToUpdate(saved_events.first, event, &update))
        SendUpdate(update);
    }
  }
  // Do not clear the map/list here so that refreshing the UI or opening a
  // second UI still works nicely!
}

void MediaInternals::SendGeneralAudioInformation() {
  base::DictionaryValue audio_info_data;

  // Audio feature information.
  auto set_feature_data = [&](auto& feature) {
    audio_info_data.SetKey(
        feature.name,
        base::Value(base::FeatureList::IsEnabled(feature) ? "Enabled"
                                                          : "Disabled"));
  };

  auto set_explicit_feature_data = [&](auto& feature, bool feature_value) {
    audio_info_data.SetKey(feature.name,
                           base::Value(feature_value ? "Enabled" : "Disabled"));
  };

  set_feature_data(features::kAudioServiceOutOfProcess);

  std::string feature_value_string;
  if (base::FeatureList::IsEnabled(
          features::kAudioServiceOutOfProcessKillAtHang)) {
    std::string timeout_value = base::GetFieldTrialParamValueByFeature(
        features::kAudioServiceOutOfProcessKillAtHang, "timeout_seconds");
    if (timeout_value.empty())
      timeout_value = "<undefined>";
    feature_value_string =
        base::StrCat({"Enabled, timeout = ", timeout_value, " seconds"});
  } else {
    feature_value_string = "Disabled";
  }
  audio_info_data.SetKey(features::kAudioServiceOutOfProcessKillAtHang.name,
                         base::Value(feature_value_string));

  set_feature_data(features::kAudioServiceLaunchOnStartup);
  set_explicit_feature_data(service_manager::features::kAudioServiceSandbox,
                            service_manager::IsAudioSandboxEnabled());
  set_explicit_feature_data(features::kWebRtcApmInAudioService,
                            media::IsWebRtcApmInAudioServiceEnabled());

  base::string16 audio_info_update =
      SerializeUpdate("media.updateGeneralAudioInformation", &audio_info_data);
  SendUpdate(audio_info_update);
}

void MediaInternals::SendAudioStreamData() {
  base::string16 audio_stream_update;
  {
    base::AutoLock auto_lock(lock_);
    audio_stream_update = SerializeUpdate("media.onReceiveAudioStreamData",
                                          &audio_streams_cached_data_);
  }
  SendUpdate(audio_stream_update);
}

void MediaInternals::SendVideoCaptureDeviceCapabilities() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!CanUpdate())
    return;

  SendUpdate(SerializeUpdate("media.onReceiveVideoCaptureCapabilities",
                             &video_capture_capabilities_cached_data_));
}

void MediaInternals::SendAudioFocusState() {
  audio_focus_helper_.SendAudioFocusState();
}

void MediaInternals::UpdateVideoCaptureDeviceCapabilities(
    const std::vector<std::tuple<media::VideoCaptureDeviceDescriptor,
                                 media::VideoCaptureFormats>>&
        descriptors_and_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  video_capture_capabilities_cached_data_.Clear();

  for (const auto& device_format_pair : descriptors_and_formats) {
    auto format_list = std::make_unique<base::ListValue>();
    // TODO(nisse): Representing format information as a string, to be
    // parsed by the javascript handler, is brittle. Consider passing
    // a list of mappings instead.

    const media::VideoCaptureDeviceDescriptor& descriptor =
        std::get<0>(device_format_pair);
    const media::VideoCaptureFormats& supported_formats =
        std::get<1>(device_format_pair);
    for (const auto& format : supported_formats)
      format_list->AppendString(media::VideoCaptureFormat::ToString(format));

    std::unique_ptr<base::DictionaryValue> device_dict(
        new base::DictionaryValue());
    device_dict->SetString("id", descriptor.device_id);
    device_dict->SetString("name", descriptor.GetNameAndModel());
    device_dict->Set("formats", std::move(format_list));
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
    device_dict->SetString("captureApi", descriptor.GetCaptureApiTypeString());
#endif
    video_capture_capabilities_cached_data_.Append(std::move(device_dict));
  }

  SendVideoCaptureDeviceCapabilities();
}

std::unique_ptr<media::AudioLog> MediaInternals::CreateAudioLog(
    AudioComponent component,
    int component_id) {
  return CreateAudioLogImpl(component, component_id, -1, MSG_ROUTING_NONE);
}

mojo::PendingRemote<media::mojom::AudioLog> MediaInternals::CreateMojoAudioLog(
    media::AudioLogFactory::AudioComponent component,
    int component_id,
    int render_process_id,
    int render_frame_id) {
  mojo::PendingRemote<media::mojom::AudioLog> audio_log;
  CreateMojoAudioLog(component, component_id,
                     audio_log.InitWithNewPipeAndPassReceiver(),
                     render_process_id, render_frame_id);
  return audio_log;
}

void MediaInternals::CreateMojoAudioLog(
    media::AudioLogFactory::AudioComponent component,
    int component_id,
    mojo::PendingReceiver<media::mojom::AudioLog> receiver,
    int render_process_id,
    int render_frame_id) {
  mojo::MakeSelfOwnedReceiver(
      CreateAudioLogImpl(component, component_id, render_process_id,
                         render_frame_id),
      std::move(receiver));
}

std::unique_ptr<MediaInternals::AudioLogImpl>
MediaInternals::CreateAudioLogImpl(
    media::AudioLogFactory::AudioComponent component,
    int component_id,
    int render_process_id,
    int render_frame_id) {
  base::AutoLock auto_lock(lock_);
  return std::make_unique<AudioLogImpl>(owner_ids_[component]++, component,
                                        this, component_id, render_process_id,
                                        render_frame_id);
}

void MediaInternals::SendUpdate(const base::string16& update) {
  // SendUpdate() may be called from any thread, but must run on the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&MediaInternals::SendUpdate,
                                  base::Unretained(this), update));
    return;
  }

  for (size_t i = 0; i < update_callbacks_.size(); i++)
    update_callbacks_[i].Run(update);
}

void MediaInternals::SaveEvent(int process_id,
                               const media::MediaLogEvent& event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

// Save the event and limit the total number per renderer. At the time of
// writing, 512 events of the kind: { "property": value } together consume
// ~88kb of memory on linux.
#if defined(OS_ANDROID)
  const size_t kEventLimit = 128;
#else
  const size_t kEventLimit = 512;
#endif

  auto& saved_events = saved_events_by_process_[process_id];
  saved_events.push_back(event);
  if (saved_events.size() > kEventLimit) {
    // Remove all events for a given player as soon as we have to remove a
    // single event for that player to avoid showing incomplete players.
    const int id_to_remove = saved_events.front().id;
    base::EraseIf(saved_events, [&](const media::MediaLogEvent& event) {
      return event.id == id_to_remove;
    });
  }
}

void MediaInternals::UpdateAudioLog(AudioLogUpdateType type,
                                    const std::string& cache_key,
                                    const std::string& function,
                                    const base::DictionaryValue* value) {
  {
    base::AutoLock auto_lock(lock_);
    const bool has_entry = audio_streams_cached_data_.HasKey(cache_key);
    if ((type == UPDATE_IF_EXISTS || type == UPDATE_AND_DELETE) && !has_entry) {
      return;
    } else if (!has_entry) {
      DCHECK_EQ(type, CREATE);
      audio_streams_cached_data_.Set(
          cache_key, std::make_unique<base::Value>(value->Clone()));
    } else if (type == UPDATE_AND_DELETE) {
      std::unique_ptr<base::Value> out_value;
      CHECK(audio_streams_cached_data_.Remove(cache_key, &out_value));
    } else {
      base::DictionaryValue* existing_dict = nullptr;
      CHECK(
          audio_streams_cached_data_.GetDictionary(cache_key, &existing_dict));
      existing_dict->MergeDictionary(value);
    }
  }

  if (CanUpdate())
    SendUpdate(SerializeUpdate(function, value));
}

}  // namespace content
