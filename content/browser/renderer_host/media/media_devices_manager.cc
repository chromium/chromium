// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/base/media_switches.h"
#include "media/capture/capture_switches.h"
#include "media/capture/mojom/video_capture_types.mojom-shared.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/device_notifications.mojom.h"
#include "services/video_capture/public/cpp/features.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/browser_main_loop.h"
#include "media/device_monitors/device_monitor_mac.h"
#endif

namespace content {

namespace {
using media::mojom::DeviceEnumerationResult;

// Resolutions used if the source doesn't support capability enumeration.
struct {
  uint16_t width;
  uint16_t height;
} const kFallbackVideoResolutions[] = {{1920, 1080}, {1280, 720}, {640, 480}};

// Frame rates for sources with no support for capability enumeration.
const uint16_t kFallbackVideoFrameRates[] = {30, 60};

void SendLogMessage(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog("MDM::" + message);
}

const char* DeviceTypeToString(MediaDeviceType device_type) {
  switch (device_type) {
    case MediaDeviceType::kMediaAudioInput:
      return "AUDIO_INPUT";
    case MediaDeviceType::kMediaVideoInput:
      return "VIDEO_INPUT";
    case MediaDeviceType::kMediaAudioOutput:
      return "AUDIO_OUTPUT";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "UNKNOWN";
}

std::string GetDevicesEnumeratedLogString(
    MediaDeviceType device_type,
    const blink::WebMediaDeviceInfoArray& device_infos) {
  std::string str = base::StringPrintf("DevicesEnumerated({type=%s}, ",
                                       DeviceTypeToString(device_type));
  base::StringAppendF(&str, "{labels=[");
  for (const auto& device_info : device_infos)
    base::StringAppendF(&str, "%s, ", device_info.label.c_str());
  if (!str.empty()) {
    str.erase(str.end() - 2, str.end());
  }
  str += "])";
  return str;
}

blink::WebMediaDeviceInfoArray GetFakeAudioDevices(bool is_input) {
  blink::WebMediaDeviceInfoArray result;
  if (is_input) {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Input",
                        "fake_group_audio_input_default");
    result.emplace_back("fake_audio_input_1", "Fake Audio Input 1",
                        "fake_group_audio_input_1");
    result.emplace_back("fake_audio_input_2", "Fake Audio Input 2",
                        "fake_group_audio_input_2");
  } else {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Output",
                        "fake_group_audio_output_default");
    result.emplace_back("fake_audio_output_1", "Fake Audio Output 1",
                        "fake_group_audio_output_1");
    result.emplace_back("fake_audio_output_2", "Fake Audio Output 2",
                        "fake_group_audio_output_2");
  }

  return result;
}

std::string VideoLabelWithoutModelID(const std::string& label) {
  if (label.rfind(")") != label.size() - 1)
    return label;

  auto idx = label.rfind(" (");
  if (idx == std::string::npos)
    return label;

  return label.substr(0, idx - 1);
}

bool LabelHasUSBModel(const std::string& label) {
  return label.size() >= 11 && label[label.size() - 11] == '(' &&
         label[label.size() - 6] == ':' && label[label.size() - 1] == ')';
}

std::string GetUSBModelFromLabel(const std::string& label) {
  DCHECK(LabelHasUSBModel(label));
  return label.substr(label.size() - 10, 9);
}

bool IsRealAudioDeviceID(const std::string& device_id) {
  return !media::AudioDeviceDescription::IsDefaultDevice(device_id) &&
         !media::AudioDeviceDescription::IsCommunicationsDevice(device_id);
}

bool EqualDeviceExcludingGroupID(const blink::WebMediaDeviceInfo& lhs,
                                 const blink::WebMediaDeviceInfo& rhs) {
  return lhs.device_id == rhs.device_id && lhs.label == rhs.label &&
         lhs.video_facing == rhs.video_facing &&
         lhs.availability == rhs.availability;
}

bool EqualDeviceIncludingGroupID(const blink::WebMediaDeviceInfo& lhs,
                                 const blink::WebMediaDeviceInfo& rhs) {
  return EqualDeviceExcludingGroupID(lhs, rhs) && lhs.group_id == rhs.group_id;
}

void ReplaceInvalidFrameRatesWithFallback(media::VideoCaptureFormats* formats) {
  for (auto& format : *formats) {
    if (format.frame_rate <= 0)
      format.frame_rate = kFallbackVideoFrameRates[0];
  }
}

void BindDeviceNotifierFromUIThread(
    mojo::PendingReceiver<audio::mojom::DeviceNotifier> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetAudioService().BindDeviceNotifier(std::move(receiver));
}

void ReportVideoEnumerationStart() {
  base::UmaHistogramBoolean(
      "Media.MediaDevicesManager.VideoDeviceEnumeration.Start", true);
}

void ReportVideoEnumerationResult(DeviceEnumerationResult result_code) {
  base::UmaHistogramEnumeration(
      "Media.MediaDevicesManager.VideoDeviceEnumeration.Result", result_code);
}

BrowserContext* GetBrowserContextOnUIThread(
    GlobalRenderFrameHostId render_frame_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* rfh = RenderFrameHostImpl::FromID(render_frame_host_id);
  if (!rfh) {
    return nullptr;
  }
  return rfh->GetBrowserContext();
}

// Sort the devices according to user pref. If the pref is unset or
// the render frame host doesn't have a `BrowserContext` the ordering will be
// unmodified.
void RankDevices(
    GlobalRenderFrameHostId render_frame_host_id,
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    base::OnceCallback<void(const MediaDeviceEnumeration&)> callback,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* browser_context = GetBrowserContextOnUIThread(render_frame_host_id);
  if (!browser_context) {
    std::move(callback).Run(enumeration);
    return;
  }

  ContentBrowserClient* content_client_browser = GetContentClient()->browser();
  auto ranked_enumeration = enumeration;

  if (requested_types[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)]) {
    content_client_browser->PreferenceRankAudioDeviceInfos(
        browser_context, ranked_enumeration[static_cast<size_t>(
                             MediaDeviceType::kMediaAudioInput)]);
  }

  if (requested_types[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)]) {
    content_client_browser->PreferenceRankVideoDeviceInfos(
        browser_context, ranked_enumeration[static_cast<size_t>(
                             MediaDeviceType::kMediaVideoInput)]);
  }

  std::move(callback).Run(ranked_enumeration);
}

}  // namespace

std::string GuessVideoGroupID(const blink::WebMediaDeviceInfoArray& audio_infos,
                              const blink::WebMediaDeviceInfo& video_info) {
  const std::string video_label = VideoLabelWithoutModelID(video_info.label);

  // If |video_label| is very small, do not guess in order to avoid false
  // positives.
  if (video_label.size() <= 3)
    return video_info.device_id;

  base::RepeatingCallback<bool(const blink::WebMediaDeviceInfo&)>
      video_label_is_included_in_audio_label = base::BindRepeating(
          [](const std::string& video_label,
             const blink::WebMediaDeviceInfo& audio_info) {
            return audio_info.label.find(video_label) != std::string::npos;
          },
          std::cref(video_label));

  const bool video_has_usb_model = LabelHasUSBModel(video_info.label);
  std::string video_usb_model = video_has_usb_model
                                    ? GetUSBModelFromLabel(video_info.label)
                                    : std::string();
  base::RepeatingCallback<bool(const blink::WebMediaDeviceInfo&)>
      usb_model_matches = base::BindRepeating(
          [](bool video_has_usb_model, const std::string& video_usb_model,
             const blink::WebMediaDeviceInfo& audio_info) {
            return video_has_usb_model && LabelHasUSBModel(audio_info.label)
                       ? video_usb_model ==
                             GetUSBModelFromLabel(audio_info.label)
                       : false;
          },
          video_has_usb_model, std::cref(video_usb_model));

  for (auto* callback :
       {&video_label_is_included_in_audio_label, &usb_model_matches}) {
    // The label for the default and communication audio devices may contain the
    // same label as the real devices, so they should be ignored when trying to
    // find unique matches.
    auto real_device_matches =
        [callback](const blink::WebMediaDeviceInfo& audio_info) {
          return IsRealAudioDeviceID(audio_info.device_id) &&
                 (*callback).Run(audio_info);
        };
    auto it_first = base::ranges::find_if(audio_infos, real_device_matches);
    if (it_first == audio_infos.end())
      continue;

    auto it = it_first;
    bool duplicate_found = false;
    while ((it = std::find_if(it + 1, audio_infos.end(),
                              real_device_matches)) != audio_infos.end()) {
      // If there is more than one match, it is impossible to know which group
      // ID is the correct one. This may occur if multiple devices of the same
      // model are installed.
      if (it->group_id != it_first->group_id) {
        duplicate_found = true;
        break;
      }
    }

    if (!duplicate_found)
      return it_first->group_id;
  }

  return video_info.device_id;
}

struct MediaDevicesManager::EnumerationRequest {
  EnumerationRequest(const BoolDeviceTypes& requested_types,
                     EnumerationCallback callback)
      : callback(std::move(callback)) {
    requested = requested_types;
  }

  BoolDeviceTypes requested;
  BoolDeviceTypes has_seen_result_for_request;
  EnumerationCallback callback;
};

// This class helps manage the consistency of cached enumeration results.
// It uses a sequence number for each invalidation and enumeration.
// A cache is considered valid if the sequence number for the last enumeration
// is greater than the sequence number for the last invalidation.
// The advantage of using invalidations over directly issuing enumerations upon
// each system notification is that some platforms issue multiple notifications
// on each device change. The cost of performing multiple redundant
// invalidations is significantly lower than the cost of issuing multiple
// redundant enumerations.
class MediaDevicesManager::CacheInfo {
 public:
  CacheInfo()
      : current_event_sequence_(0),
        seq_last_update_(0),
        seq_last_invalidation_(0),
        is_update_ongoing_(false) {}

  void InvalidateCache() {
    DCHECK(thread_checker_.CalledOnValidThread());
    seq_last_invalidation_ = NewEventSequence();
  }

  bool IsLastUpdateValid() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return seq_last_update_ > seq_last_invalidation_ && !is_update_ongoing_;
  }

  void UpdateStarted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_update_ongoing_);
    seq_last_update_ = NewEventSequence();
    is_update_ongoing_ = true;
  }

  void UpdateCompleted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(is_update_ongoing_);
    is_update_ongoing_ = false;
  }

  bool is_update_ongoing() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return is_update_ongoing_;
  }

 private:
  int64_t NewEventSequence() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return ++current_event_sequence_;
  }

  int64_t current_event_sequence_;
  int64_t seq_last_update_;
  int64_t seq_last_invalidation_;
  bool is_update_ongoing_;
  base::ThreadChecker thread_checker_;
};

MediaDevicesManager::SubscriptionRequest::SubscriptionRequest(
    GlobalRenderFrameHostId render_frame_host_id,
    const BoolDeviceTypes& subscribe_types,
    mojo::Remote<blink::mojom::MediaDevicesListener> listener)
    : render_frame_host_id(render_frame_host_id),
      subscribe_types(subscribe_types),
      listener_(std::move(listener)) {}

MediaDevicesManager::SubscriptionRequest::SubscriptionRequest(
    SubscriptionRequest&&) = default;

MediaDevicesManager::SubscriptionRequest::~SubscriptionRequest() = default;

MediaDevicesManager::SubscriptionRequest&
MediaDevicesManager::SubscriptionRequest::operator=(SubscriptionRequest&&) =
    default;

class MediaDevicesManager::AudioServiceDeviceListener
    : public audio::mojom::DeviceListener {
 public:
  AudioServiceDeviceListener(base::RepeatingClosure disconnect_cb)
      : disconnect_cb_(std::move(disconnect_cb)) {
    ConnectToService();
  }

  AudioServiceDeviceListener(const AudioServiceDeviceListener&) = delete;
  AudioServiceDeviceListener& operator=(const AudioServiceDeviceListener&) =
      delete;

  ~AudioServiceDeviceListener() override = default;

  void DevicesChanged() override {
    auto* system_monitor = base::SystemMonitor::Get();
    if (system_monitor)
      system_monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  }

 private:
  void ConnectToService() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!mojo_audio_device_notifier_);
    DCHECK(!receiver_.is_bound());
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BindDeviceNotifierFromUIThread,
            mojo_audio_device_notifier_.BindNewPipeAndPassReceiver()));
    mojo_audio_device_notifier_.set_disconnect_handler(base::BindOnce(
        &MediaDevicesManager::AudioServiceDeviceListener::OnConnectionError,
        weak_factory_.GetWeakPtr()));
    mojo_audio_device_notifier_->RegisterListener(
        receiver_.BindNewPipeAndPassRemote());
  }

  void OnConnectionError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(disconnect_cb_);
    mojo_audio_device_notifier_.reset();
    receiver_.reset();
    disconnect_cb_.Run();

    // Resetting the error handler in a posted task since doing it synchronously
    // results in a browser crash. See https://crbug.com/845142.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AudioServiceDeviceListener::ConnectToService,
                                  weak_factory_.GetWeakPtr()));
  }

  // |disconnect_cb_| is a callback used to invalidate the cache and do a
  // fresh enumeration to avoid losing out on the changes that might happen
  // when the audio service is not active.
  const base::RepeatingClosure disconnect_cb_;
  mojo::Receiver<audio::mojom::DeviceListener> receiver_{this};
  mojo::Remote<audio::mojom::DeviceNotifier> mojo_audio_device_notifier_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AudioServiceDeviceListener> weak_factory_{this};
};

MediaDevicesManager::MediaDevicesManager(
    media::AudioSystem* audio_system,
    const scoped_refptr<VideoCaptureManager>& video_capture_manager,
    StopRemovedInputDeviceCallback stop_removed_input_device_cb,
    UIInputDeviceChangeCallback ui_input_device_change_cb)
    : use_fake_devices_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)),
      audio_system_(audio_system),
      video_capture_manager_(video_capture_manager),
      stop_removed_input_device_cb_(std::move(stop_removed_input_device_cb)),
      ui_input_device_change_cb_(std::move(ui_input_device_change_cb)),
      permission_checker_(std::make_unique<MediaDevicesPermissionChecker>()),
      cache_infos_(static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes)),
      monitoring_started_(false),
      get_salt_and_origin_cb_(
          base::BindRepeating(&GetMediaDeviceSaltAndOrigin)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(audio_system_);
  DCHECK(video_capture_manager_.get());
  DCHECK(!stop_removed_input_device_cb_.is_null());
  DCHECK(!ui_input_device_change_cb_.is_null());
  SendLogMessage("MediaDevicesManager()");
  cache_policies_.fill(CachePolicy::NO_CACHE);
}

MediaDevicesManager::~MediaDevicesManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaDevicesManager::EnumerateDevices(
    const BoolDeviceTypes& requested_types,
    EnumerationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartMonitoring();

  requests_.emplace_back(requested_types, std::move(callback));
  bool all_results_cached = true;
  for (size_t i = 0;
       i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
    if (requested_types[i] && cache_policies_[i] == CachePolicy::NO_CACHE) {
      all_results_cached = false;
      DoEnumerateDevices(static_cast<MediaDeviceType>(i));
    }
  }

  if (all_results_cached)
    ProcessRequests();
}

void MediaDevicesManager::EnumerateAndRankDevices(
    GlobalRenderFrameHostId render_frame_host_id,
    const BoolDeviceTypes& requested_types,
    EnumerationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  EnumerateDevices(
      requested_types,
      base::BindPostTask(
          GetUIThreadTaskRunner(),
          base::BindOnce(
              &RankDevices, render_frame_host_id, requested_types,
              base::BindPostTaskToCurrentDefault(std::move(callback)))));
}

void MediaDevicesManager::EnumerateAndRankDevices(
    GlobalRenderFrameHostId render_frame_host_id,
    const BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_video_input_capabilities &&
             requested_types[static_cast<size_t>(
                 MediaDeviceType::kMediaVideoInput)] ||
         !request_video_input_capabilities);
  DCHECK(request_audio_input_capabilities &&
             requested_types[static_cast<size_t>(
                 MediaDeviceType::kMediaAudioInput)] ||
         !request_audio_input_capabilities);
  SendLogMessage(base::StringPrintf(
      "EnumerateDevices({render_process_id=%d}, {render_frame_id=%d}, "
      "{request_audio=%s}, {request_video=%s})",
      render_frame_host_id.child_id, render_frame_host_id.frame_routing_id,
      request_audio_input_capabilities ? "true" : "false",
      request_video_input_capabilities ? "true" : "false"));

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          get_salt_and_origin_cb_, render_frame_host_id,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &MediaDevicesManager::CheckPermissionsForEnumerateDevices,
              weak_factory_.GetWeakPtr(), render_frame_host_id, requested_types,
              request_video_input_capabilities,
              request_audio_input_capabilities, std::move(callback)))));
}

uint32_t MediaDevicesManager::SubscribeDeviceChangeNotifications(
    GlobalRenderFrameHostId render_frame_host_id,
    const BoolDeviceTypes& subscribe_types,
    mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartMonitoring();
  uint32_t subscription_id = ++last_subscription_id_;
  mojo::Remote<blink::mojom::MediaDevicesListener> media_devices_listener;
  media_devices_listener.Bind(std::move(listener));
  media_devices_listener.set_disconnect_handler(
      base::BindOnce(&MediaDevicesManager::UnsubscribeDeviceChangeNotifications,
                     weak_factory_.GetWeakPtr(), subscription_id));
  subscriptions_.emplace(
      subscription_id,
      SubscriptionRequest(render_frame_host_id, subscribe_types,
                          std::move(media_devices_listener)));

  // Fetch the first device_id_salt for this subscriber's frame, to be able to
  // later detect changes.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          get_salt_and_origin_cb_, render_frame_host_id,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &MediaDevicesManager::SetSubscriptionLastSeenDeviceIdSalt,
              weak_factory_.GetWeakPtr(), subscription_id))));

  return subscription_id;
}

void MediaDevicesManager::SetSubscriptionLastSeenDeviceIdSalt(
    uint32_t subscription_id,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  auto it = subscriptions_.find(subscription_id);

  if (it == subscriptions_.end())
    return;
  SubscriptionRequest& request = it->second;

  request.last_seen_device_id_salt_ = salt_and_origin.device_id_salt();
}

void MediaDevicesManager::UnsubscribeDeviceChangeNotifications(
    uint32_t subscription_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  subscriptions_.erase(subscription_id);
}

void MediaDevicesManager::SetCachePolicy(MediaDeviceType type,
                                         CachePolicy policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsValidMediaDeviceType(type));
  if (cache_policies_[static_cast<size_t>(type)] == policy)
    return;

  cache_policies_[static_cast<size_t>(type)] = policy;
  // If the new policy is SYSTEM_MONITOR, issue an enumeration to populate the
  // cache.
  if (policy == CachePolicy::SYSTEM_MONITOR) {
    cache_infos_[static_cast<size_t>(type)].InvalidateCache();
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::StartMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (monitoring_started_)
    return;

  if (!base::SystemMonitor::Get())
    return;

#if BUILDFLAG(IS_MAC)
  if (!base::FeatureList::IsEnabled(features::kDeviceMonitorMac))
    return;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess)) {
    DCHECK(!audio_service_device_listener_);

    // base::Unretained(this) is safe here because |this| owns
    // |audio_service_device_listener_|.
    audio_service_device_listener_ =
        std::make_unique<AudioServiceDeviceListener>(
            /*disconnect_cb=*/base::BindRepeating(
                &MediaDevicesManager::HandleDevicesChanged,
                base::Unretained(this), MediaDeviceType::kMediaAudioInput));
  }
#endif
  SendLogMessage("StartMonitoring()");
  monitoring_started_ = true;
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  if (base::FeatureList::IsEnabled(features::kMediaDevicesSystemMonitorCache)) {
    for (size_t i = 0;
         i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
      DCHECK(cache_policies_[i] != CachePolicy::SYSTEM_MONITOR);
      SetCachePolicy(static_cast<MediaDeviceType>(i),
                     CachePolicy::SYSTEM_MONITOR);
    }
  }

#if BUILDFLAG(IS_MAC)
    RegisterVideoCaptureDevicesChangedObserver();
#endif
#if BUILDFLAG(IS_WIN)
    if ((base::FeatureList::IsEnabled(
             video_capture::features::
                 kWinCameraMonitoringInVideoCaptureService) ||
         switches::IsMediaFoundationCameraUsageMonitoringEnabled()) &&
        !base::FeatureList::IsEnabled(
            features::kRunVideoCaptureServiceInBrowserProcess)) {
      RegisterVideoCaptureDevicesChangedObserver();
    }
#endif
}

void MediaDevicesManager::StopMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!monitoring_started_)
    return;
  SendLogMessage(base::StringPrintf("StopMonitoring([this=%p])", this));
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
  audio_service_device_listener_.reset();
  monitoring_started_ = false;
  for (size_t i = 0;
       i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
    SetCachePolicy(static_cast<MediaDeviceType>(i), CachePolicy::NO_CACHE);
  }
}

void MediaDevicesManager::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO:
      HandleDevicesChanged(MediaDeviceType::kMediaAudioInput);
      HandleDevicesChanged(MediaDeviceType::kMediaAudioOutput);
      break;
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      HandleDevicesChanged(MediaDeviceType::kMediaVideoInput);
      break;
    default:
      break;  // Uninteresting device change.
  }
}

media::VideoCaptureFormats MediaDevicesManager::GetVideoInputFormats(
    const std::string& device_id,
    bool try_in_use_first) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  media::VideoCaptureFormats formats;

  if (try_in_use_first) {
    std::optional<media::VideoCaptureFormat> format =
        video_capture_manager_->GetDeviceFormatInUse(
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, device_id);
    if (format.has_value()) {
      formats.push_back(format.value());
      ReplaceInvalidFrameRatesWithFallback(&formats);
      return formats;
    }
  }

  video_capture_manager_->GetDeviceSupportedFormats(device_id, &formats);
  ReplaceInvalidFrameRatesWithFallback(&formats);
  // Remove formats that have zero resolution.
  std::erase_if(formats, [](const media::VideoCaptureFormat& format) {
    return format.frame_size.GetArea() <= 0;
  });

  // If the device does not report any valid format, use a fallback list of
  // standard formats.
  if (formats.empty()) {
    for (const auto& resolution : kFallbackVideoResolutions) {
      for (const auto frame_rate : kFallbackVideoFrameRates) {
        formats.push_back(media::VideoCaptureFormat(
            gfx::Size(resolution.width, resolution.height), frame_rate,
            media::PIXEL_FORMAT_I420));
      }
    }
  }

  return formats;
}

blink::WebMediaDeviceInfoArray MediaDevicesManager::GetCachedDeviceInfo(
    MediaDeviceType type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return current_snapshot_[static_cast<size_t>(type)];
}

void MediaDevicesManager::RegisterDispatcherHost(
    std::unique_ptr<blink::mojom::MediaDevicesDispatcherHost> dispatcher_host,
    mojo::PendingReceiver<blink::mojom::MediaDevicesDispatcherHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  dispatcher_hosts_.Add(std::move(dispatcher_host), std::move(receiver));
}

void MediaDevicesManager::SetPermissionChecker(
    std::unique_ptr<MediaDevicesPermissionChecker> permission_checker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(permission_checker);
  permission_checker_ = std::move(permission_checker);
}

void MediaDevicesManager::CheckPermissionsForEnumerateDevices(
    GlobalRenderFrameHostId render_frame_host_id,
    const BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_->CheckPermissions(
      requested_types, render_frame_host_id.child_id,
      render_frame_host_id.frame_routing_id,
      base::BindOnce(&MediaDevicesManager::OnPermissionsCheckDone,
                     weak_factory_.GetWeakPtr(), render_frame_host_id,
                     requested_types, request_video_input_capabilities,
                     request_audio_input_capabilities, std::move(callback),
                     std::move(salt_and_origin)));
}

void MediaDevicesManager::OnPermissionsCheckDone(
    GlobalRenderFrameHostId render_frame_host_id,
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // The video-capture subsystem currently does not support group IDs.
  // If video input devices are requested, also request audio input devices in
  // order to be able to use an heuristic that guesses group IDs for video
  // devices by finding matches in audio input devices.
  // TODO(crbug.com/41263713): Remove |internal_requested_types| and use
  // |requested_types| directly when video capture supports group IDs.
  BoolDeviceTypes internal_requested_types;
  internal_requested_types[static_cast<size_t>(
      MediaDeviceType::kMediaAudioInput)] =
      requested_types[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] ||
      requested_types[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)];
  internal_requested_types[static_cast<size_t>(
      MediaDeviceType::kMediaVideoInput)] =
      requested_types[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)];
  internal_requested_types[static_cast<size_t>(
      MediaDeviceType::kMediaAudioOutput)] =
      requested_types[static_cast<size_t>(MediaDeviceType::kMediaAudioOutput)];

  EnumerateAndRankDevices(
      render_frame_host_id, internal_requested_types,
      base::BindOnce(&MediaDevicesManager::OnDevicesEnumerated,
                     weak_factory_.GetWeakPtr(), requested_types,
                     request_video_input_capabilities,
                     request_audio_input_capabilities, std::move(callback),
                     std::move(salt_and_origin), has_permissions));
}

void MediaDevicesManager::OnDevicesEnumerated(
    const MediaDevicesManager::BoolDeviceTypes& requested_types,
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    const MediaDevicesManager::BoolDeviceTypes& has_permissions,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const bool video_input_capabilities_requested =
      has_permissions[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)] &&
      request_video_input_capabilities;
  const bool audio_input_capabilities_requested =
      has_permissions[static_cast<size_t>(MediaDeviceType::kMediaAudioInput)] &&
      request_audio_input_capabilities;

  std::vector<blink::WebMediaDeviceInfoArray> translation(
      static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes));
  for (size_t i = 0;
       i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
    if (!requested_types[i])
      continue;

    for (const auto& device_info : enumeration[i]) {
      if (!has_permissions[i] && !translation[i].empty()) {
        break;
      }

      translation[i].push_back(TranslateMediaDeviceInfo(
          has_permissions[i], salt_and_origin, device_info));
    }
  }

  GetAudioInputCapabilities(video_input_capabilities_requested,
                            audio_input_capabilities_requested,
                            std::move(callback), enumeration, translation);
}

void MediaDevicesManager::GetAudioInputCapabilities(
    bool request_video_input_capabilities,
    bool request_audio_input_capabilities,
    EnumerateDevicesCallback callback,
    const MediaDeviceEnumeration& raw_enumeration_results,
    const std::vector<blink::WebMediaDeviceInfoArray>&
        hashed_enumeration_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  EnumerationState state;
  size_t state_id = next_enumeration_state_id_++;
  state.video_input_capabilities_requested = request_video_input_capabilities;
  state.audio_input_capabilities_requested = request_audio_input_capabilities;
  state.completion_cb = std::move(callback);
  state.raw_enumeration_results = raw_enumeration_results;
  state.hashed_enumeration_results = hashed_enumeration_results;
  state.num_pending_audio_input_capabilities =
      hashed_enumeration_results[static_cast<size_t>(
                                     MediaDeviceType::kMediaAudioInput)]
          .size();

  if (!state.audio_input_capabilities_requested ||
      state.num_pending_audio_input_capabilities == 0) {
    FinalizeDevicesEnumerated(std::move(state));
    return;
  }

  enumeration_states_[state_id] = std::move(state);
  DCHECK_EQ(raw_enumeration_results[static_cast<size_t>(
                                        MediaDeviceType::kMediaAudioInput)]
                .size(),
            hashed_enumeration_results[static_cast<size_t>(
                                           MediaDeviceType::kMediaAudioInput)]
                .size());
  std::size_t num_audio_input_devices =
      raw_enumeration_results[static_cast<size_t>(
                                  MediaDeviceType::kMediaAudioInput)]
          .size();
  for (std::size_t i = 0; i < num_audio_input_devices; i++) {
    auto raw_device_info = raw_enumeration_results[static_cast<size_t>(
        MediaDeviceType::kMediaAudioInput)][i];
    auto hashed_device_info = hashed_enumeration_results[static_cast<size_t>(
        MediaDeviceType::kMediaAudioInput)][i];

    AudioInputDeviceCapabilitiesPtr capabilities =
        blink::mojom::AudioInputDeviceCapabilities::New();
    capabilities->device_id = hashed_device_info.device_id;
    capabilities->parameters =
        media::AudioParameters::UnavailableDeviceParams();
    enumeration_states_[state_id].audio_capabilities.push_back(
        std::move(capabilities));
    size_t capabilities_index =
        enumeration_states_[state_id].audio_capabilities.size() - 1;
    if (use_fake_devices_) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&MediaDevicesManager::GotAudioInputCapabilities,
                         weak_factory_.GetWeakPtr(), state_id,
                         capabilities_index,
                         media::AudioParameters::UnavailableDeviceParams()));
    } else {
      audio_system_->GetInputStreamParameters(
          raw_device_info.device_id,
          base::BindOnce(&MediaDevicesManager::GotAudioInputCapabilities,
                         weak_factory_.GetWeakPtr(), state_id,
                         capabilities_index));
    }
  }
}

void MediaDevicesManager::GotAudioInputCapabilities(
    size_t state_id,
    size_t capabilities_index,
    const std::optional<media::AudioParameters>& parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::Contains(enumeration_states_, state_id));

  auto& enumeration_state = enumeration_states_[state_id];
  DCHECK_GT(enumeration_state.num_pending_audio_input_capabilities, 0);

  AudioInputDeviceCapabilitiesPtr& capabilities =
      enumeration_state.audio_capabilities[capabilities_index];
  if (parameters) {
    capabilities->parameters = *parameters;
    // Data from the |parameters| field is duplicated in the |channels|,
    // |sample_rate| and |latency| fields due to the lack of availability
    // of the media::AudioParameters native mojo mapping in blink.
    // TODO(crbug.com/40550966): Remove redundant fields when |parameters|
    // is accessible from Blink.
    capabilities->is_valid = parameters->IsValid();
    capabilities->channels = parameters->channels();
    capabilities->sample_rate = parameters->sample_rate();
    capabilities->latency = parameters->GetBufferDuration();
  }
  DCHECK(capabilities->parameters.IsValid());

  if (--enumeration_state.num_pending_audio_input_capabilities == 0) {
    FinalizeDevicesEnumerated(std::move(enumeration_state));
    enumeration_states_.erase(state_id);
  }
}

void MediaDevicesManager::FinalizeDevicesEnumerated(
    EnumerationState enumeration_state) {
  std::move(enumeration_state.completion_cb)
      .Run(std::move(enumeration_state.hashed_enumeration_results),
           enumeration_state.video_input_capabilities_requested
               ? ComputeVideoInputCapabilities(
                     enumeration_state
                         .raw_enumeration_results[static_cast<size_t>(
                             MediaDeviceType::kMediaVideoInput)],
                     enumeration_state
                         .hashed_enumeration_results[static_cast<size_t>(
                             MediaDeviceType::kMediaVideoInput)])
               : std::vector<VideoInputDeviceCapabilitiesPtr>(),
           std::move(enumeration_state.audio_capabilities));
}

std::vector<VideoInputDeviceCapabilitiesPtr>
MediaDevicesManager::ComputeVideoInputCapabilities(
    const blink::WebMediaDeviceInfoArray& raw_device_infos,
    const blink::WebMediaDeviceInfoArray& translated_device_infos) {
  DCHECK_EQ(raw_device_infos.size(), translated_device_infos.size());
  std::vector<VideoInputDeviceCapabilitiesPtr> video_input_capabilities;
  for (size_t i = 0; i < raw_device_infos.size(); ++i) {
    VideoInputDeviceCapabilitiesPtr capabilities =
        blink::mojom::VideoInputDeviceCapabilities::New();
    capabilities->device_id = translated_device_infos[i].device_id;
    capabilities->formats = GetVideoInputFormats(raw_device_infos[i].device_id,
                                                 false /* try_in_use_first */);
    capabilities->facing_mode = translated_device_infos[i].video_facing;
    if (translated_device_infos[i].availability) {
      capabilities->availability =
          static_cast<media::mojom::CameraAvailability>(
              *translated_device_infos[i].availability);
    }
    video_input_capabilities.push_back(std::move(capabilities));
  }
  return video_input_capabilities;
}

void MediaDevicesManager::DoEnumerateDevices(MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsValidMediaDeviceType(type));
  CacheInfo& cache_info = cache_infos_[static_cast<size_t>(type)];
  if (cache_info.is_update_ongoing())
    return;
  SendLogMessage(base::StringPrintf("DoEnumerateDevices({type=%s})",
                                    DeviceTypeToString(type)));

  cache_info.UpdateStarted();
  switch (type) {
    case MediaDeviceType::kMediaAudioInput:
      EnumerateAudioDevices(true /* is_input */);
      break;
    case MediaDeviceType::kMediaVideoInput:
      ReportVideoEnumerationStart();
      video_capture_manager_->EnumerateDevices(
          base::BindOnce(&MediaDevicesManager::VideoInputDevicesEnumerated,
                         weak_factory_.GetWeakPtr()));
      break;
    case MediaDeviceType::kMediaAudioOutput:
      EnumerateAudioDevices(false /* is_input */);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void MediaDevicesManager::EnumerateAudioDevices(bool is_input) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDeviceType type = is_input ? MediaDeviceType::kMediaAudioInput
                                  : MediaDeviceType::kMediaAudioOutput;
  if (use_fake_devices_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MediaDevicesManager::DevicesEnumerated,
                                  weak_factory_.GetWeakPtr(), type,
                                  GetFakeAudioDevices(is_input)));
    return;
  }

  audio_system_->GetDeviceDescriptions(
      is_input, base::BindOnce(&MediaDevicesManager::AudioDevicesEnumerated,
                               weak_factory_.GetWeakPtr(), type));
}

void MediaDevicesManager::VideoInputDevicesEnumerated(
    DeviceEnumerationResult result_code,
    const media::VideoCaptureDeviceDescriptors& descriptors) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ReportVideoEnumerationResult(result_code);

  if (result_code != DeviceEnumerationResult::kSuccess) {
    std::string log_message =
        base::StringPrintf("VideoInputDevicesEnumerated got error %d",
                           static_cast<int>(result_code));
    // Log to both WebRTC logs (for feedback reports) and text logs for
    // manually-collected chrome logs at customers.
    SendLogMessage(log_message);
    VLOG(1) << log_message;
    // TODO(crbug.com/40221155): Propagate this as an error response to the
    // page and expose in the JS API.
  }
  blink::WebMediaDeviceInfoArray snapshot;
  for (const auto& descriptor : descriptors) {
    snapshot.emplace_back(descriptor);
  }
  DevicesEnumerated(MediaDeviceType::kMediaVideoInput, snapshot);
}

void MediaDevicesManager::AudioDevicesEnumerated(
    MediaDeviceType type,
    media::AudioDeviceDescriptions device_descriptions) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  blink::WebMediaDeviceInfoArray snapshot;
  for (const media::AudioDeviceDescription& description : device_descriptions) {
    snapshot.emplace_back(
        description.unique_id, description.device_name, description.group_id,
        media::VideoCaptureControlSupport(), blink::mojom::FacingMode::kNone);
  }
  DevicesEnumerated(type, snapshot);
}

void MediaDevicesManager::DevicesEnumerated(
    MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsValidMediaDeviceType(type));
  UpdateSnapshot(type, snapshot);
  cache_infos_[static_cast<size_t>(type)].UpdateCompleted();
  cache_is_populated_[static_cast<size_t>(type)] = true;
  SendLogMessage(GetDevicesEnumeratedLogString(type, snapshot));

  if (cache_policies_[static_cast<size_t>(type)] == CachePolicy::NO_CACHE) {
    for (auto& request : requests_)
      request.has_seen_result_for_request[static_cast<size_t>(type)] = true;
  }

  // Note that IsLastUpdateValid is always true when policy is NO_CACHE.
  if (cache_infos_[static_cast<size_t>(type)].IsLastUpdateValid()) {
    ProcessRequests();
  } else {
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::UpdateSnapshot(
    MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& new_snapshot,
    bool ignore_group_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsValidMediaDeviceType(type));

  bool need_update_device_change_subscribers = false;
  blink::WebMediaDeviceInfoArray& old_snapshot =
      current_snapshot_[static_cast<size_t>(type)];

  if (type == MediaDeviceType::kMediaAudioInput ||
      type == MediaDeviceType::kMediaVideoInput) {
    MaybeStopRemovedInputDevices(type, new_snapshot);
  }

  // Update the cached snapshot and send notifications only if the device list
  // has changed.
  if (!base::ranges::equal(new_snapshot, old_snapshot,
                           ignore_group_id ? EqualDeviceExcludingGroupID
                                           : EqualDeviceIncludingGroupID)) {
    // Prevent sending notifications until group IDs are updated using
    // a heuristic in ProcessRequests().
    // TODO(crbug.com/41263713): Remove |is_video_with_group_ids| and the
    // corresponding checks when the video-capture subsystem supports
    // group IDs.
    bool is_video_with_good_group_ids =
        type == MediaDeviceType::kMediaVideoInput &&
        (new_snapshot.size() == 0 || !new_snapshot[0].group_id.empty());
    if (type == MediaDeviceType::kMediaAudioInput ||
        is_video_with_good_group_ids) {
      ui_input_device_change_cb_.Run(type, new_snapshot);
    }

    // Do not notify device-change subscribers after the first enumeration
    // result, since it is not due to an actual device change.
    need_update_device_change_subscribers =
        cache_is_populated_[static_cast<size_t>(type)] &&
        (old_snapshot.size() != 0 || new_snapshot.size() != 0) &&
        (type != MediaDeviceType::kMediaVideoInput ||
         is_video_with_good_group_ids);
    current_snapshot_[static_cast<size_t>(type)] = new_snapshot;
  }

  // Generate salts for each subscriber even if the device list hasn't changed,
  // as we may need to notify them anyway.
  for (const auto& subscription : subscriptions_) {
    const SubscriptionRequest& request = subscription.second;
    if (request.subscribe_types[static_cast<size_t>(type)]) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              get_salt_and_origin_cb_, request.render_frame_host_id,
              base::BindPostTaskToCurrentDefault(base::BindOnce(
                  &MediaDevicesManager::OnSaltAndOriginForSubscription,
                  weak_factory_.GetWeakPtr(), subscription.first,
                  request.render_frame_host_id, type, new_snapshot,
                  need_update_device_change_subscribers))));
    }
  }
}

void MediaDevicesManager::ProcessRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Populate the group ID field for video devices using a heuristic that looks
  // for device coincidences with audio input devices.
  // TODO(crbug.com/41263713): Remove this once the video-capture subsystem
  // supports group IDs.
  if (cache_is_populated_[static_cast<size_t>(
          MediaDeviceType::kMediaVideoInput)]) {
    blink::WebMediaDeviceInfoArray video_devices =
        current_snapshot_[static_cast<size_t>(
            MediaDeviceType::kMediaVideoInput)];
    for (auto& video_device_info : video_devices) {
      video_device_info.group_id =
          GuessVideoGroupID(current_snapshot_[static_cast<size_t>(
                                MediaDeviceType::kMediaAudioInput)],
                            video_device_info);
    }
    UpdateSnapshot(MediaDeviceType::kMediaVideoInput, video_devices,
                   false /* ignore_group_id */);
  }

  std::erase_if(requests_, [this](EnumerationRequest& request) {
    if (IsEnumerationRequestReady(request)) {
      std::move(request.callback).Run(current_snapshot_);
      return true;
    }
    return false;
  });
}

bool MediaDevicesManager::IsEnumerationRequestReady(
    const EnumerationRequest& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (size_t i = 0;
       i < static_cast<size_t>(MediaDeviceType::kNumMediaDeviceTypes); ++i) {
    if (!request_info.requested[i]) {
      continue;
    }
    switch (cache_policies_[i]) {
      case CachePolicy::SYSTEM_MONITOR:
        if (!cache_is_populated_[i]) {
          return false;
        }
        break;
      case CachePolicy::NO_CACHE:
        if (!request_info.has_seen_result_for_request[i]) {
          return false;
        }
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
  return true;
}

void MediaDevicesManager::HandleDevicesChanged(MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsValidMediaDeviceType(type));
  if (!cache_infos_[static_cast<size_t>(type)].is_update_ongoing()) {
    SendLogMessage(base::StringPrintf("HandleDevicesChanged({type=%s}",
                                      DeviceTypeToString(type)));
  }
  cache_infos_[static_cast<size_t>(type)].InvalidateCache();
  DoEnumerateDevices(type);
}

void MediaDevicesManager::MaybeStopRemovedInputDevices(
    MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& new_snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MediaDeviceType::kMediaAudioInput ||
         type == MediaDeviceType::kMediaVideoInput);

  std::vector<blink::WebMediaDeviceInfo> removed_audio_devices;
  for (const auto& old_device_info :
       current_snapshot_[static_cast<size_t>(type)]) {
    // If a device was removed, notify the MediaStreamManager to stop all
    // streams using that device.
    if (!base::Contains(new_snapshot, old_device_info.device_id,
                        &blink::WebMediaDeviceInfo::device_id)) {
      stop_removed_input_device_cb_.Run(type, old_device_info);

      if (type == MediaDeviceType::kMediaAudioInput) {
        removed_audio_devices.push_back(old_device_info);
      }
    }
  }

  // "default" and "communications" audio devices that have been removed,
  // require an extra notification. In fact, such audio devices have associated
  // virtual audio devices in the snapshot with the special "default" or
  // "communications" IDs. The code below implements an heuristic, such that to
  // identify if an audio device was default, it checks whether the old
  // snapshot contained an audio device with the same group ID and device ID
  // matching either "default" or "communications".
  // NOTE: ChromeOS is able to seamlessly redirect streams to the new default
  // device, hence the event should not be triggered.
#if !BUILDFLAG(IS_CHROMEOS)
  for (const auto& removed_audio_device : removed_audio_devices) {
    for (const auto& old_device_info :
         current_snapshot_[static_cast<size_t>(type)]) {
      if (removed_audio_device.group_id == old_device_info.group_id &&
          (old_device_info.device_id ==
               media::AudioDeviceDescription::kDefaultDeviceId ||
           old_device_info.device_id ==
               media::AudioDeviceDescription::kCommunicationsDeviceId)) {
        stop_removed_input_device_cb_.Run(type, old_device_info);
      }
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

void MediaDevicesManager::OnSaltAndOriginForSubscription(
    uint32_t subscription_id,
    GlobalRenderFrameHostId render_frame_host_id,
    MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& device_infos,
    bool devices_changed,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = subscriptions_.find(subscription_id);
  if (it == subscriptions_.end())
    return;
  SubscriptionRequest& request = it->second;

  // Continue to propagate a change notification if either the actual device
  // list has changed, or the device_id_salt has changed.
  bool salt_reset =
      request.last_seen_device_id_salt_ &&
      salt_and_origin.device_id_salt() != request.last_seen_device_id_salt_;

  if (devices_changed || salt_reset) {
    MediaDevicesManager::CheckPermissionForDeviceChange(
        subscription_id, render_frame_host_id, type, device_infos,
        salt_and_origin);
  }
  request.last_seen_device_id_salt_ = salt_and_origin.device_id_salt();
}

void MediaDevicesManager::CheckPermissionForDeviceChange(
    uint32_t subscription_id,
    GlobalRenderFrameHostId render_frame_host_id,
    MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& device_infos,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  permission_checker_->CheckPermission(
      type, render_frame_host_id.child_id,
      render_frame_host_id.frame_routing_id,
      base::BindOnce(&MediaDevicesManager::OnCheckedPermissionForDeviceChange,
                     weak_factory_.GetWeakPtr(), subscription_id,
                     render_frame_host_id, type, device_infos,
                     salt_and_origin));
}

void MediaDevicesManager::OnCheckedPermissionForDeviceChange(
    uint32_t subscription_id,
    GlobalRenderFrameHostId render_frame_host_id,
    MediaDeviceType type,
    const blink::WebMediaDeviceInfoArray& device_infos,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    bool has_permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDevicesManager::BoolDeviceTypes requested_types;
  requested_types[static_cast<size_t>(type)] = true;
  MediaDeviceEnumeration enumeration;
  enumeration[static_cast<size_t>(type)] = device_infos;

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RankDevices, render_frame_host_id, requested_types,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &MediaDevicesManager::NotifyDeviceChange,
                         weak_factory_.GetWeakPtr(), subscription_id, type,
                         salt_and_origin, has_permission)),
                     enumeration));
}

void MediaDevicesManager::NotifyDeviceChange(
    uint32_t subscription_id,
    MediaDeviceType type,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    bool has_permission,
    const MediaDeviceEnumeration& enumeration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::IsValidMediaDeviceType(type));
  auto it = subscriptions_.find(subscription_id);
  if (it == subscriptions_.end())
    return;
  SendLogMessage(
      base::StringPrintf("NotifyDeviceChange({subscription_id=%u}, {type=%s}",
                         subscription_id, DeviceTypeToString(type)));

  const SubscriptionRequest& request = it->second;
  request.listener_->OnDevicesChanged(
      type,
      TranslateMediaDeviceInfoArray(has_permission, salt_and_origin,
                                    enumeration[static_cast<size_t>(type)]));
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
void MediaDevicesManager::RegisterVideoCaptureDevicesChangedObserver() {
  CHECK(!video_capture_service_device_changed_observer_);
  if (base::FeatureList::IsEnabled(
          features::kRunVideoCaptureServiceInBrowserProcess)) {
    // Do not create a mojo connection when the video capture service is running
    // in the browser process as the device monitor will send device change
    // notifications directly to the system monitor in the browser process.
    return;
  }
  // base::Unretained(this) is safe here because |this| owns
  // |video_capture_service_device_changed_observer_|.
  video_capture_service_device_changed_observer_ =
      std::make_unique<VideoCaptureDevicesChangedObserver>(
          /*disconnect_cb=*/base::BindRepeating(
              &MediaDevicesManager::HandleDevicesChanged,
              base::Unretained(this), MediaDeviceType::kMediaVideoInput),
          /*listener_cb=*/base::BindRepeating([] {
            if (auto* monitor = base::SystemMonitor::Get()) {
              monitor->ProcessDevicesChanged(
                  base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
            }
          }));
  video_capture_service_device_changed_observer_->ConnectToService();
}
#endif

MediaDevicesManager::EnumerationState::EnumerationState() = default;
MediaDevicesManager::EnumerationState::EnumerationState(
    EnumerationState&& other) = default;
MediaDevicesManager::EnumerationState::~EnumerationState() = default;
MediaDevicesManager::EnumerationState& MediaDevicesManager::EnumerationState::
operator=(EnumerationState&& other) = default;

}  // namespace content
