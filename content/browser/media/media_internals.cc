// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/media/media_internals.h"

#include <stddef.h>

#include <list>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_features.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_log_record.h"
#include "media/base/media_switches.h"
#include "media/webrtc/webrtc_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/sandbox_type.h"

#if !BUILDFLAG(IS_ANDROID)
#include "media/filters/decrypting_video_decoder.h"
#endif

namespace content {

namespace {

std::u16string SerializeUpdate(std::string_view function,
                               const base::ValueView value) {
  base::ValueView args[] = {value};
  return content::WebUI::GetJavascriptCall(function, args);
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
      {media::AudioParameters::HOTWORD, "HOTWORD"},
  };

  std::string ret;
  for (size_t i = 0; i < std::size(flags); ++i) {
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
    case media::AudioParameters::AUDIO_BITSTREAM_DTS:
      return "dts";
    case media::AudioParameters::AUDIO_BITSTREAM_DTS_HD:
      return "dtshd";
    case media::AudioParameters::AUDIO_BITSTREAM_DTSX_P2:
      return "dtsxp2";
    case media::AudioParameters::AUDIO_BITSTREAM_IEC61937:
      return "iec61937";
    case media::AudioParameters::AUDIO_BITSTREAM_DTS_HD_MA:
      return "dtshd_ma";
    case media::AudioParameters::AUDIO_FAKE:
      return "fake";
  }
}

const char kAudioLogStatusKey[] = "status";
const char kAudioLogUpdateFunction[] = "media.updateAudioComponent";

}  // namespace

// This class works as a receiver of logs of events occurring in the
// media pipeline. Media logs send by the renderer process to the
// browser process is handled by the below implementation in the
// browser side.
class MediaInternals::MediaInternalLogRecordsImpl
    : public content::mojom::MediaInternalLogRecords {
 public:
  MediaInternalLogRecordsImpl(content::MediaInternals* media_internals,
                              int render_process_id);

  MediaInternalLogRecordsImpl(const MediaInternalLogRecordsImpl&) = delete;
  MediaInternalLogRecordsImpl& operator=(const MediaInternalLogRecordsImpl&) =
      delete;

  ~MediaInternalLogRecordsImpl() override = default;
  void Log(const std::vector<::media::MediaLogRecord>& arr) override;

 private:
  const raw_ptr<content::MediaInternals> media_internals_;
  const int render_process_id_;
};

MediaInternals::MediaInternalLogRecordsImpl::MediaInternalLogRecordsImpl(
    content::MediaInternals* media_internals,
    int render_process_id)
    : media_internals_(media_internals),
      render_process_id_(render_process_id) {}

void MediaInternals::MediaInternalLogRecordsImpl::Log(
    const std::vector<::media::MediaLogRecord>& events) {
  media_internals_->OnMediaEvents(render_process_id_, events);
}

class MediaInternals::AudioLogImpl : public media::mojom::AudioLog,
                                     public media::AudioLog {
 public:
  AudioLogImpl(int owner_id,
               media::AudioLogFactory::AudioComponent component,
               content::MediaInternals* media_internals,
               int component_id,
               int render_process_id,
               int render_frame_id);

  AudioLogImpl(const AudioLogImpl&) = delete;
  AudioLogImpl& operator=(const AudioLogImpl&) = delete;

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

  void StoreComponentMetadata(base::Value::Dict* dict);
  std::string FormatCacheKey();

  static void SendWebContentsTitleHelper(const std::string& cache_key,
                                         base::Value::Dict dict,
                                         int render_process_id,
                                         int render_frame_id);

  const int owner_id_;
  const media::AudioLogFactory::AudioComponent component_;
  const raw_ptr<content::MediaInternals> media_internals_;
  const int component_id_;
  const int render_process_id_;
  const int render_frame_id_;
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
  base::Value::Dict dict;
  StoreComponentMetadata(&dict);

  dict.Set(kAudioLogStatusKey, "created");
  dict.Set("device_id", device_id);
  dict.Set("device_type", FormatToString(params.format()));
  dict.Set("frames_per_buffer", params.frames_per_buffer());
  dict.Set("sample_rate", params.sample_rate());
  dict.Set("channels", params.channels());
  dict.Set("channel_layout", ChannelLayoutToString(params.channel_layout()));
  dict.Set("effects", EffectsToString(params.effects()));

  media_internals_->UpdateAudioLog(MediaInternals::CREATE, FormatCacheKey(),
                                   kAudioLogUpdateFunction, dict);
  SetWebContentsTitle();
}

void MediaInternals::AudioLogImpl::OnStarted() {
  SendSingleStringUpdate(kAudioLogStatusKey, "started");
}

void MediaInternals::AudioLogImpl::OnStopped() {
  SendSingleStringUpdate(kAudioLogStatusKey, "stopped");
}

void MediaInternals::AudioLogImpl::OnClosed() {
  base::Value::Dict dict;
  StoreComponentMetadata(&dict);
  dict.Set(kAudioLogStatusKey, "closed");
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_AND_DELETE,
                                   FormatCacheKey(), kAudioLogUpdateFunction,
                                   dict);
}

void MediaInternals::AudioLogImpl::OnError() {
  SendSingleStringUpdate("error_occurred", "true");
}

void MediaInternals::AudioLogImpl::OnSetVolume(double volume) {
  base::Value::Dict dict;
  StoreComponentMetadata(&dict);
  dict.Set("volume", volume);
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_IF_EXISTS,
                                   FormatCacheKey(), kAudioLogUpdateFunction,
                                   dict);
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
  base::Value::Dict dict;
  StoreComponentMetadata(&dict);
  SendWebContentsTitleHelper(FormatCacheKey(), std::move(dict),
                             render_process_id_, render_frame_id_);
}

std::string MediaInternals::AudioLogImpl::FormatCacheKey() {
  return base::StringPrintf("%d:%d:%d", owner_id_,
                            base::to_underlying(component_), component_id_);
}

// static
void MediaInternals::AudioLogImpl::SendWebContentsTitleHelper(
    const std::string& cache_key,
    base::Value::Dict dict,
    int render_process_id,
    int render_frame_id) {
  // Page title information can only be retrieved from the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
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
  dict.Set("render_process_id", render_process_id);
  dict.Set("web_contents_title", web_contents->GetTitle());
  MediaInternals::GetInstance()->UpdateAudioLog(
      MediaInternals::UPDATE_IF_EXISTS, cache_key, kAudioLogUpdateFunction,
      dict);
}

void MediaInternals::AudioLogImpl::SendSingleStringUpdate(
    const std::string& key,
    const std::string& value) {
  base::Value::Dict dict;
  StoreComponentMetadata(&dict);
  dict.Set(key, value);
  media_internals_->UpdateAudioLog(MediaInternals::UPDATE_IF_EXISTS,
                                   FormatCacheKey(), kAudioLogUpdateFunction,
                                   dict);
}

void MediaInternals::AudioLogImpl::StoreComponentMetadata(
    base::Value::Dict* dict) {
  dict->Set("owner_id", owner_id_);
  dict->Set("component_id", component_id_);
  dict->Set("component_type", base::to_underlying(component_));
}

MediaInternals* MediaInternals::GetInstance() {
  static content::MediaInternals* internals = new content::MediaInternals();
  return internals;
}

MediaInternals::MediaInternals() = default;

MediaInternals::~MediaInternals() {}

void MediaInternals::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  if (!host_observation_.IsObservingSource(host)) {
    host_observation_.AddObservation(host);
  }
}

void MediaInternals::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  EraseSavedEvents(host);
  host_observation_.RemoveObservation(host);
}

void MediaInternals::RenderProcessHostDestroyed(RenderProcessHost* host) {
  // TODO(sandersd): Is there ever a relevant case where
  // RenderProcessHostDestroyed is called without RenderProcessExited also being
  // called?
  EraseSavedEvents(host);
  host_observation_.RemoveObservation(host);
}

// Converts the |event| to a |update|. Returns whether the conversion succeeded.
static bool ConvertEventToUpdate(int render_process_id,
                                 const media::MediaLogRecord& event,
                                 std::u16string* update) {
  DCHECK(update);

  base::Value::Dict dict;
  dict.Set("renderer", render_process_id);
  dict.Set("player", event.id);

  // TODO(dalecurtis): This is technically not correct.  TimeTicks "can't" be
  // converted to to a human readable time format.  See base/time/time.h.
  const double ticks = event.time.ToInternalValue();
  const double ticks_millis = ticks / base::Time::kMicrosecondsPerMillisecond;
  dict.Set("ticksMillis", ticks_millis);

  base::Value::Dict cloned_params = event.params.Clone();
  switch (event.type) {
    case media::MediaLogRecord::Type::kMessage:
      dict.Set("type", "MEDIA_LOG_ENTRY");
      break;
    case media::MediaLogRecord::Type::kMediaPropertyChange:
      dict.Set("type", "PROPERTY_CHANGE");
      break;
    case media::MediaLogRecord::Type::kMediaEventTriggered: {
      // Delete the "event" param so that it won't spam the log.
      std::optional<base::Value> exists = cloned_params.Extract("event");
      DCHECK(exists.has_value());
      dict.Set("type", std::move(exists.value()));
      break;
    }
    case media::MediaLogRecord::Type::kMediaStatus:
      dict.Set("type", "PIPELINE_ERROR");
      base::Value::Dict wrapped_parameters;
      wrapped_parameters.Set("error", std::move(cloned_params));
      cloned_params = std::move(wrapped_parameters);
      break;
  }

  dict.Set("params", std::move(cloned_params));

  *update = SerializeUpdate("media.onMediaEvent", dict);
  return true;
}

void MediaInternals::OnMediaEvents(
    int render_process_id,
    const std::vector<media::MediaLogRecord>& events) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Notify observers that |event| has occurred.
  for (const auto& event : events) {
    if (CanUpdate()) {
      std::u16string update;
      if (ConvertEventToUpdate(render_process_id, event, &update))
        SendUpdate(update);
    }
    SaveEvent(render_process_id, event);
  }
}

void MediaInternals::AddUpdateCallback(UpdateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  update_callbacks_.push_back(std::move(callback));

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
      std::u16string update;
      if (ConvertEventToUpdate(saved_events.first, event, &update))
        SendUpdate(update);
    }
  }
  // Do not clear the map/list here so that refreshing the UI or opening a
  // second UI still works nicely!
}

void MediaInternals::SendGeneralAudioInformation() {
  base::Value::Dict audio_info_data;

  // Audio feature information.
  auto set_feature_data = [&audio_info_data](auto& feature) {
    audio_info_data.Set(
        feature.name,
        base::Value(base::FeatureList::IsEnabled(feature) ? "Enabled"
                                                          : "Disabled"));
  };

  auto set_explicit_feature_data = [&audio_info_data](auto& feature,
                                                      bool feature_value) {
    audio_info_data.Set(feature.name,
                        base::Value(feature_value ? "Enabled" : "Disabled"));
  };

  set_feature_data(features::kAudioServiceOutOfProcess);

  set_feature_data(features::kAudioServiceLaunchOnStartup);
  set_explicit_feature_data(
      features::kAudioServiceSandbox,
      GetContentClient()->browser()->ShouldSandboxAudioService());
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  std::string chrome_wide_echo_cancellation_value_string =
      media::IsChromeWideEchoCancellationEnabled()
          ? base::StrCat(
                {"Enabled, minimize_resampling = ",
                 media::kChromeWideEchoCancellationMinimizeResampling.Get()
                     ? "true"
                     : "false",
                 ", allow_all_sample_rates = ",
                 media::kChromeWideEchoCancellationAllowAllSampleRates.Get()
                     ? "true"
                     : "false"})
          : "Disabled";
  audio_info_data.Set(media::kChromeWideEchoCancellation.name,
                      base::Value(chrome_wide_echo_cancellation_value_string));

  std::string decrease_processing_audio_fifo_size_value_string =
      base::FeatureList::IsEnabled(media::kDecreaseProcessingAudioFifoSize)
          ? base::StrCat(
                {"Enabled, fifo_size = ",
                 base::NumberToString(media::GetProcessingAudioFifoSize())})
          : "Disabled";
  audio_info_data.Set(
      media::kDecreaseProcessingAudioFifoSize.name,
      base::Value(decrease_processing_audio_fifo_size_value_string));
#endif
  std::u16string audio_info_update =
      SerializeUpdate("media.updateGeneralAudioInformation", audio_info_data);
  SendUpdate(audio_info_update);
}

void MediaInternals::SendAudioStreamData() {
  std::u16string audio_stream_update;
  {
    base::AutoLock auto_lock(lock_);
    audio_stream_update = SerializeUpdate("media.onReceiveAudioStreamData",
                                          audio_streams_cached_data_);
  }
  SendUpdate(audio_stream_update);
}

void MediaInternals::SendVideoCaptureDeviceCapabilities() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!CanUpdate())
    return;

  SendUpdate(SerializeUpdate("media.onReceiveVideoCaptureCapabilities",
                             video_capture_capabilities_cached_data_));
}

void MediaInternals::SendAudioFocusState() {
  audio_focus_helper_.SendAudioFocusState();
}

void MediaInternals::GetRegisteredCdms() {
  cdm_helper_.GetRegisteredCdms();
}

void MediaInternals::UpdateVideoCaptureDeviceCapabilities(
    const std::vector<std::tuple<media::VideoCaptureDeviceDescriptor,
                                 media::VideoCaptureFormats>>&
        descriptors_and_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  video_capture_capabilities_cached_data_.clear();

  for (const auto& device_format_pair : descriptors_and_formats) {
    base::Value::List control_support;
    base::Value::List format_list;
    // TODO(nisse): Representing format information as a string, to be
    // parsed by the javascript handler, is brittle. Consider passing
    // a list of mappings instead.

    const media::VideoCaptureDeviceDescriptor& descriptor =
        std::get<0>(device_format_pair);
    const media::VideoCaptureFormats& supported_formats =
        std::get<1>(device_format_pair);
    if (descriptor.control_support().pan)
      control_support.Append("pan");
    if (descriptor.control_support().tilt)
      control_support.Append("tilt");
    if (descriptor.control_support().zoom)
      control_support.Append("zoom");
    for (const auto& format : supported_formats)
      format_list.Append(media::VideoCaptureFormat::ToString(format));

    base::Value::Dict device_dict;
    device_dict.Set("id", descriptor.device_id);
    device_dict.Set("name", descriptor.GetNameAndModel());
    device_dict.Set("controlSupport", std::move(control_support));
    device_dict.Set("formats", std::move(format_list));
    device_dict.Set("captureApi", descriptor.GetCaptureApiTypeString());
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

// static
void MediaInternals::CreateMediaLogRecords(
    int render_process_id,
    mojo::PendingReceiver<content::mojom::MediaInternalLogRecords> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MediaInternalLogRecordsImpl>(
          MediaInternals::GetInstance(), render_process_id),
      std::move(receiver));
}

std::unique_ptr<MediaInternals::AudioLogImpl>
MediaInternals::CreateAudioLogImpl(
    media::AudioLogFactory::AudioComponent component,
    int component_id,
    int render_process_id,
    int render_frame_id) {
  base::AutoLock auto_lock(lock_);
  return std::make_unique<AudioLogImpl>(
      owner_ids_[base::to_underlying(component)]++, component, this,
      component_id, render_process_id, render_frame_id);
}

void MediaInternals::SendUpdate(const std::u16string& update) {
  // SendUpdate() may be called from any thread, but must run on the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&MediaInternals::SendUpdate,
                                  base::Unretained(this), update));
    return;
  }

  for (size_t i = 0; i < update_callbacks_.size(); i++)
    update_callbacks_[i].Run(update);
}

void MediaInternals::SaveEvent(int process_id,
                               const media::MediaLogRecord& event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& saved_events = saved_events_by_process_[process_id];
  saved_events.push_back(event);
  if (saved_events.size() > media::MediaLog::kLogLimit) {
    // Remove all events for a given player as soon as we have to remove a
    // single event for that player to avoid showing incomplete players.
    const int id_to_remove = saved_events.front().id;
    std::erase_if(saved_events, [&](const media::MediaLogRecord& event) {
      return event.id == id_to_remove;
    });
  }
}

void MediaInternals::EraseSavedEvents(RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(sandersd): Send a termination event before clearing the log.
  saved_events_by_process_.erase(host->GetID());
}

void MediaInternals::UpdateAudioLog(AudioLogUpdateType type,
                                    std::string_view cache_key,
                                    std::string_view function,
                                    const base::Value::Dict& value) {
  {
    base::AutoLock auto_lock(lock_);
    const bool has_entry = audio_streams_cached_data_.Find(cache_key);
    if ((type == UPDATE_IF_EXISTS || type == UPDATE_AND_DELETE) && !has_entry) {
      return;
    } else if (!has_entry) {
      DCHECK_EQ(type, CREATE);
      audio_streams_cached_data_.Set(cache_key, value.Clone());
    } else if (type == UPDATE_AND_DELETE) {
      std::optional<base::Value> out_value =
          audio_streams_cached_data_.Extract(cache_key);
      CHECK(out_value.has_value());
    } else {
      base::Value::Dict* existing_dict =
          audio_streams_cached_data_.FindDict(cache_key);
      CHECK(existing_dict);
      existing_dict->Merge(value.Clone());
    }
  }

  if (CanUpdate())
    SendUpdate(SerializeUpdate(function, value));
}

}  // namespace content
