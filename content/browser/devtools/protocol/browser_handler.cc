// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/browser_handler.h"

#include <string.h>
#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "content/browser/devtools/browser_devtools_agent_host.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/protocol/devtools_download_manager_delegate.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "v8/include/v8-version-string.h"

using blink::PermissionType;

namespace content {
namespace protocol {

BrowserHandler::BrowserHandler(bool allow_set_download_behavior)
    : DevToolsDomainHandler(Browser::Metainfo::domainName),
      download_events_enabled_(false),
      allow_set_download_behavior_(allow_set_download_behavior) {}

BrowserHandler::~BrowserHandler() = default;

Response BrowserHandler::Disable() {
  // TODO: this leaks context ids for all contexts with overridden permissions.
  for (auto& browser_context_id : contexts_with_overridden_permissions_) {
    content::BrowserContext* browser_context = nullptr;
    std::string error;
    Maybe<std::string> context_id =
        browser_context_id == "" ? Maybe<std::string>()
                                 : Maybe<std::string>(browser_context_id);
    FindBrowserContext(context_id, &browser_context);
    if (browser_context) {
      PermissionControllerImpl* permission_controller =
          PermissionControllerImpl::FromBrowserContext(browser_context);
      permission_controller->ResetOverridesForDevTools();
    }
  }
  contexts_with_overridden_permissions_.clear();

  // TODO: this leaks context ids for all contexts with overridden downloads.
  for (auto& browser_context_id : contexts_with_overridden_downloads_) {
    content::BrowserContext* browser_context = nullptr;
    std::string error;
    Maybe<std::string> context_id =
        browser_context_id == "" ? Maybe<std::string>()
                                 : Maybe<std::string>(browser_context_id);
    FindBrowserContext(context_id, &browser_context);
    if (browser_context) {
      auto* delegate =
          DevToolsDownloadManagerDelegate::GetInstance(browser_context);
      if (delegate) {
        delegate->set_download_behavior(
            DevToolsDownloadManagerDelegate::DownloadBehavior::DEFAULT);
      }
    }
  }
  contexts_with_overridden_downloads_.clear();
  SetDownloadEventsEnabled(false);
  histograms_snapshots_.clear();

  return Response::Success();
}

void BrowserHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Browser::Frontend>(dispatcher->channel());
  Browser::Dispatcher::wire(dispatcher, this);
}

Response BrowserHandler::GetVersion(std::string* protocol_version,
                                    std::string* product,
                                    std::string* revision,
                                    std::string* user_agent,
                                    std::string* js_version) {
  *protocol_version = DevToolsAgentHost::GetProtocolVersion();
  *revision = GetChromiumGitRevision();
  *product = GetContentClient()->browser()->GetProduct();
  *user_agent = GetContentClient()->browser()->GetUserAgent();
  *js_version = V8_VERSION_STRING;
  return Response::Success();
}

namespace {
// Parses PermissionDescriptors (|descriptor|) into their appropriate
// PermissionType |permission_type| by duplicating the logic in the methods
// //third_party/blink/renderer/modules/permissions:permissions
// ::ParsePermission and
// //content/browser/permissions:permission_service_impl
// ::PermissionDescriptorToPermissionType, producing an error in
// |error_message| as necessary.
// TODO(crbug.com/40638575): De-duplicate this logic.
Response PermissionDescriptorToPermissionType(
    std::unique_ptr<protocol::Browser::PermissionDescriptor> descriptor,
    PermissionType* permission_type) {
  const std::string name = descriptor->GetName();

  if (name == "geolocation") {
    *permission_type = PermissionType::GEOLOCATION;
  } else if (name == "camera") {
    if (descriptor->GetPanTiltZoom(false))
      *permission_type = PermissionType::CAMERA_PAN_TILT_ZOOM;
    else
      *permission_type = PermissionType::VIDEO_CAPTURE;
  } else if (name == "microphone") {
    *permission_type = PermissionType::AUDIO_CAPTURE;
  } else if (name == "notifications") {
    *permission_type = PermissionType::NOTIFICATIONS;
  } else if (name == "persistent-storage") {
    *permission_type = PermissionType::DURABLE_STORAGE;
  } else if (name == "push") {
    if (!descriptor->GetUserVisibleOnly(false)) {
      return Response::InvalidParams(
          "Push Permission without userVisibleOnly:true isn't supported");
    }
    *permission_type = PermissionType::NOTIFICATIONS;
  } else if (name == "midi") {
    if (descriptor->GetSysex(false))
      *permission_type = PermissionType::MIDI_SYSEX;
    else
      *permission_type = PermissionType::MIDI;
  } else if (name == "background-sync") {
    *permission_type = PermissionType::BACKGROUND_SYNC;
  } else if (name == "ambient-light-sensor" || name == "accelerometer" ||
             name == "gyroscope" || name == "magnetometer") {
    *permission_type = PermissionType::SENSORS;
  } else if (name == "clipboard-read") {
    *permission_type = PermissionType::CLIPBOARD_READ_WRITE;
  } else if (name == "clipboard-write") {
    if (descriptor->GetAllowWithoutSanitization(false))
      *permission_type = PermissionType::CLIPBOARD_READ_WRITE;
    else
      *permission_type = PermissionType::CLIPBOARD_SANITIZED_WRITE;
  } else if (name == "payment-handler") {
    *permission_type = PermissionType::PAYMENT_HANDLER;
  } else if (name == "background-fetch") {
    *permission_type = PermissionType::BACKGROUND_FETCH;
  } else if (name == "idle-detection") {
    *permission_type = PermissionType::IDLE_DETECTION;
  } else if (name == "periodic-background-sync") {
    *permission_type = PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (name == "screen-wake-lock") {
    *permission_type = PermissionType::WAKE_LOCK_SCREEN;
  } else if (name == "system-wake-lock") {
    *permission_type = PermissionType::WAKE_LOCK_SYSTEM;
  } else if (name == "nfc") {
    *permission_type = PermissionType::NFC;
  } else if (name == "window-management") {
    *permission_type = PermissionType::WINDOW_MANAGEMENT;
  } else if (name == "local-fonts") {
    *permission_type = PermissionType::LOCAL_FONTS;
  } else if (name == "display-capture") {
    *permission_type = PermissionType::DISPLAY_CAPTURE;
  } else if (name == "storage-access") {
    *permission_type = PermissionType::STORAGE_ACCESS_GRANT;
  } else if (name == "top-level-storage-access") {
    *permission_type = PermissionType::TOP_LEVEL_STORAGE_ACCESS;
  } else if (name == "captured-surface-control") {
    *permission_type = PermissionType::CAPTURED_SURFACE_CONTROL;
  } else if (name == "speaker-selection") {
    *permission_type = PermissionType::SPEAKER_SELECTION;
  } else if (name == "keyboard-lock") {
    *permission_type = PermissionType::KEYBOARD_LOCK;
  } else if (name == "pointer-lock") {
    *permission_type = PermissionType::POINTER_LOCK;
  } else if (name == "fullscreen") {
    if (!descriptor->GetAllowWithoutGesture(false)) {
      // There is no PermissionType for fullscreen with user gesture.
      return Response::InvalidParams(
          "Fullscreen Permission only supports allowWithoutGesture:true");
    }
    *permission_type = PermissionType::AUTOMATIC_FULLSCREEN;
  } else if (name == "web-app-installation") {
    *permission_type = PermissionType::WEB_APP_INSTALLATION;
  } else {
    return Response::InvalidParams("Invalid PermissionDescriptor name: " +
                                   name);
  }

  return Response::Success();
}

Response FromProtocolPermissionType(
    const protocol::Browser::PermissionType& type,
    PermissionType* out_type) {
  if (type == protocol::Browser::PermissionTypeEnum::Notifications) {
    *out_type = PermissionType::NOTIFICATIONS;
  } else if (type == protocol::Browser::PermissionTypeEnum::Geolocation) {
    *out_type = PermissionType::GEOLOCATION;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::ProtectedMediaIdentifier) {
    *out_type = PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  } else if (type == protocol::Browser::PermissionTypeEnum::Midi) {
    *out_type = PermissionType::MIDI;
  } else if (type == protocol::Browser::PermissionTypeEnum::MidiSysex) {
    *out_type = PermissionType::MIDI_SYSEX;
  } else if (type == protocol::Browser::PermissionTypeEnum::DurableStorage) {
    *out_type = PermissionType::DURABLE_STORAGE;
  } else if (type == protocol::Browser::PermissionTypeEnum::AudioCapture) {
    *out_type = PermissionType::AUDIO_CAPTURE;
  } else if (type == protocol::Browser::PermissionTypeEnum::VideoCapture) {
    *out_type = PermissionType::VIDEO_CAPTURE;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::VideoCapturePanTiltZoom) {
    *out_type = PermissionType::CAMERA_PAN_TILT_ZOOM;
  } else if (type == protocol::Browser::PermissionTypeEnum::BackgroundSync) {
    *out_type = PermissionType::BACKGROUND_SYNC;
  } else if (type == protocol::Browser::PermissionTypeEnum::Sensors) {
    *out_type = PermissionType::SENSORS;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::ClipboardReadWrite) {
    *out_type = PermissionType::CLIPBOARD_READ_WRITE;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::ClipboardSanitizedWrite) {
    *out_type = PermissionType::CLIPBOARD_SANITIZED_WRITE;
  } else if (type == protocol::Browser::PermissionTypeEnum::PaymentHandler) {
    *out_type = PermissionType::PAYMENT_HANDLER;
  } else if (type == protocol::Browser::PermissionTypeEnum::BackgroundFetch) {
    *out_type = PermissionType::BACKGROUND_FETCH;
  } else if (type == protocol::Browser::PermissionTypeEnum::IdleDetection) {
    *out_type = PermissionType::IDLE_DETECTION;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::PeriodicBackgroundSync) {
    *out_type = PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (type == protocol::Browser::PermissionTypeEnum::WakeLockScreen) {
    *out_type = PermissionType::WAKE_LOCK_SCREEN;
  } else if (type == protocol::Browser::PermissionTypeEnum::WakeLockSystem) {
    *out_type = PermissionType::WAKE_LOCK_SYSTEM;
  } else if (type == protocol::Browser::PermissionTypeEnum::Nfc) {
    *out_type = PermissionType::NFC;
  } else if (type == protocol::Browser::PermissionTypeEnum::WindowManagement) {
    *out_type = PermissionType::WINDOW_MANAGEMENT;
  } else if (type == protocol::Browser::PermissionTypeEnum::LocalFonts) {
    *out_type = PermissionType::LOCAL_FONTS;
  } else if (type == protocol::Browser::PermissionTypeEnum::DisplayCapture) {
    *out_type = PermissionType::DISPLAY_CAPTURE;
  } else if (type == protocol::Browser::PermissionTypeEnum::StorageAccess) {
    *out_type = PermissionType::STORAGE_ACCESS_GRANT;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::TopLevelStorageAccess) {
    *out_type = PermissionType::TOP_LEVEL_STORAGE_ACCESS;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::CapturedSurfaceControl) {
    *out_type = PermissionType::CAPTURED_SURFACE_CONTROL;
  } else if (type == protocol::Browser::PermissionTypeEnum::SpeakerSelection) {
    *out_type = PermissionType::SPEAKER_SELECTION;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::WebAppInstallation) {
    *out_type = PermissionType::WEB_APP_INSTALLATION;
  } else {
    return Response::InvalidParams("Unknown permission type: " + type);
  }
  return Response::Success();
}

Response PermissionSettingToPermissionStatus(
    const protocol::Browser::PermissionSetting& setting,
    blink::mojom::PermissionStatus* out_status) {
  if (setting == protocol::Browser::PermissionSettingEnum::Granted) {
    *out_status = blink::mojom::PermissionStatus::GRANTED;
  } else if (setting == protocol::Browser::PermissionSettingEnum::Denied) {
    *out_status = blink::mojom::PermissionStatus::DENIED;
  } else if (setting == protocol::Browser::PermissionSettingEnum::Prompt) {
    *out_status = blink::mojom::PermissionStatus::ASK;
  } else {
    return Response::InvalidParams("Unknown permission setting: " + setting);
  }
  return Response::Success();
}

}  // namespace

// static
Response BrowserHandler::FindBrowserContext(
    const Maybe<std::string>& browser_context_id,
    BrowserContext** browser_context) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::ServerError(
        "Browser context management is not supported.");
  if (!browser_context_id.has_value()) {
    *browser_context = delegate->GetDefaultBrowserContext();
    if (*browser_context == nullptr)
      return Response::ServerError(
          "Browser context management is not supported.");
    return Response::Success();
  }

  std::string context_id = browser_context_id.value();
  for (auto* context : delegate->GetBrowserContexts()) {
    if (context->UniqueId() == context_id) {
      *browser_context = context;
      return Response::Success();
    }
  }
  return Response::InvalidParams("Failed to find browser context for id " +
                                 context_id);
}

// static
std::vector<BrowserHandler*> BrowserHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<BrowserHandler>(Browser::Metainfo::domainName);
}

Response BrowserHandler::SetPermission(
    std::unique_ptr<protocol::Browser::PermissionDescriptor> permission,
    const protocol::Browser::PermissionSetting& setting,
    Maybe<std::string> origin,
    Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess())
    return response;

  PermissionType type;
  Response parse_response =
      PermissionDescriptorToPermissionType(std::move(permission), &type);
  if (!parse_response.IsSuccess())
    return parse_response;

  blink::mojom::PermissionStatus permission_status;
  Response setting_response =
      PermissionSettingToPermissionStatus(setting, &permission_status);
  if (!setting_response.IsSuccess())
    return setting_response;

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(browser_context);

  std::optional<url::Origin> overridden_origin;
  if (origin.has_value()) {
    overridden_origin = url::Origin::Create(GURL(origin.value()));
    if (overridden_origin->opaque())
      return Response::InvalidParams(
          "Permission can't be granted to opaque origins.");
  }
  PermissionControllerImpl::OverrideStatus status =
      permission_controller->SetOverrideForDevTools(overridden_origin, type,
                                                    permission_status);
  if (status != PermissionControllerImpl::OverrideStatus::kOverrideSet) {
    return Response::InvalidParams(
        "Permission can't be granted in current context.");
  }
  contexts_with_overridden_permissions_.insert(
      browser_context_id.value_or(std::string()));
  return Response::Success();
}

Response BrowserHandler::GrantPermissions(
    std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
        permissions,
    Maybe<std::string> origin,
    Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess())
    return response;

  std::vector<PermissionType> internal_permissions;
  internal_permissions.reserve(permissions->size());
  for (const protocol::Browser::PermissionType& t : *permissions) {
    PermissionType type;
    Response type_response = FromProtocolPermissionType(t, &type);
    if (!type_response.IsSuccess())
      return type_response;
    internal_permissions.push_back(type);
  }

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(browser_context);
  std::optional<url::Origin> overridden_origin;
  if (origin.has_value()) {
    overridden_origin = url::Origin::Create(GURL(origin.value()));
    if (overridden_origin->opaque())
      return Response::InvalidParams(
          "Permission can't be granted to opaque origins.");
  }
  PermissionControllerImpl::OverrideStatus status =
      permission_controller->GrantOverridesForDevTools(overridden_origin,
                                                       internal_permissions);

  if (status != PermissionControllerImpl::OverrideStatus::kOverrideSet) {
    return Response::InvalidParams(
        "Permissions can't be granted in current context.");
  }
  contexts_with_overridden_permissions_.insert(browser_context_id.value_or(""));
  return Response::Success();
}

Response BrowserHandler::ResetPermissions(
    Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess())
    return response;
  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(browser_context);
  permission_controller->ResetOverridesForDevTools();
  contexts_with_overridden_permissions_.erase(browser_context_id.value_or(""));
  return Response::Success();
}

Response BrowserHandler::SetDownloadBehavior(
    const std::string& behavior,
    Maybe<std::string> browser_context_id,
    Maybe<std::string> download_path,
    Maybe<bool> events_enabled) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess())
    return response;
  response = DoSetDownloadBehavior(behavior, browser_context,
                                   std::move(download_path));
  if (!response.IsSuccess())
    return response;
  SetDownloadEventsEnabled(events_enabled.value_or(false));
  return response;
}

Response BrowserHandler::DoSetDownloadBehavior(
    const std::string& behavior,
    BrowserContext* browser_context,
    Maybe<std::string> download_path) {
  if (!allow_set_download_behavior_)
    return Response::ServerError("Not allowed");
  if (behavior == Browser::SetDownloadBehavior::BehaviorEnum::Allow &&
      !download_path.has_value()) {
    return Response::InvalidParams("downloadPath not provided");
  }
  DevToolsManagerDelegate* manager_delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!manager_delegate) {
    return Response::ServerError(
        "Browser context management is not supported.");
  }

  auto* delegate =
      DevToolsDownloadManagerDelegate::GetOrCreateInstance(browser_context);
  if (behavior == Browser::SetDownloadBehavior::BehaviorEnum::Allow) {
    delegate->set_download_behavior(
        DevToolsDownloadManagerDelegate::DownloadBehavior::ALLOW);
    delegate->set_download_path(download_path.value());
  } else if (behavior ==
             Browser::SetDownloadBehavior::BehaviorEnum::AllowAndName) {
    delegate->set_download_behavior(
        DevToolsDownloadManagerDelegate::DownloadBehavior::ALLOW_AND_NAME);
    delegate->set_download_path(download_path.value());
  } else if (behavior == Browser::SetDownloadBehavior::BehaviorEnum::Deny) {
    delegate->set_download_behavior(
        DevToolsDownloadManagerDelegate::DownloadBehavior::DENY);
  } else {
    delegate->set_download_behavior(
        DevToolsDownloadManagerDelegate::DownloadBehavior::DEFAULT);
  }
  contexts_with_overridden_downloads_.insert(
      manager_delegate->GetDefaultBrowserContext() == browser_context
          ? ""
          : browser_context->UniqueId());

  return Response::Success();
}

Response BrowserHandler::CancelDownload(const std::string& guid,
                                        Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.IsSuccess())
    return response;
  auto* delegate =
      DevToolsDownloadManagerDelegate::GetOrCreateInstance(browser_context);
  auto* download_item = delegate->GetDownloadByGuid(guid);
  if (!download_item)
    return Response::InvalidParams("No download item found for the given GUID");
  // DownloadItem::Cancel is implemented in a soft way, where there would be no
  // error triggered if the state is not suitable for cancallation (e.g.
  // already cancelled or finished).
  download_item->Cancel(true);
  return Response::Success();
}

Response BrowserHandler::GetHistograms(
    const Maybe<std::string> in_query,
    const Maybe<bool> in_delta,
    std::unique_ptr<Array<Browser::Histogram>>* const out_histograms) {
  DCHECK(out_histograms);
  bool get_deltas = in_delta.value_or(false);
  *out_histograms = std::make_unique<Array<Browser::Histogram>>();
  for (base::HistogramBase* const h :
       base::StatisticsRecorder::Sort(base::StatisticsRecorder::WithName(
           base::StatisticsRecorder::GetHistograms(), in_query.value_or("")))) {
    DCHECK(h);
    (*out_histograms)->emplace_back(GetHistogramData(*h, get_deltas));
  }

  return Response::Success();
}

Response BrowserHandler::GetHistogram(
    const std::string& in_name,
    const Maybe<bool> in_delta,
    std::unique_ptr<Browser::Histogram>* const out_histogram) {
  // Get histogram by name.
  base::HistogramBase* const in_histogram =
      base::StatisticsRecorder::FindHistogram(in_name);
  if (!in_histogram)
    return Response::InvalidParams("Cannot find histogram: " + in_name);

  DCHECK(out_histogram);
  *out_histogram = GetHistogramData(*in_histogram, in_delta.value_or(false));

  return Response::Success();
}

Response BrowserHandler::GetBrowserCommandLine(
    std::unique_ptr<protocol::Array<std::string>>* arguments) {
  *arguments = std::make_unique<protocol::Array<std::string>>();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // The commandline is potentially sensitive, only return it if it
  // contains kEnableAutomation.
  if (command_line->HasSwitch(switches::kEnableAutomation)) {
    for (const auto& arg : command_line->argv()) {
#if BUILDFLAG(IS_WIN)
      (*arguments)->emplace_back(base::WideToUTF8(arg));
#else
      (*arguments)->emplace_back(arg);
#endif
    }
    return Response::Success();
  } else {
    return Response::ServerError(
        "Command line not returned because --enable-automation not set.");
  }
}

Response BrowserHandler::Crash() {
  CHECK(false);
  return Response::Success();
}

Response BrowserHandler::CrashGpuProcess() {
  auto* host = GpuProcessHost::Get();
  if (host) {
    host->gpu_service()->Crash();
  }
  return Response::Success();
}

void BrowserHandler::OnDownloadUpdated(download::DownloadItem* item) {
  std::string state;
  switch (item->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      state = Browser::DownloadProgress::StateEnum::InProgress;
      break;
    case download::DownloadItem::COMPLETE:
      state = Browser::DownloadProgress::StateEnum::Completed;
      break;
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED:
      state = Browser::DownloadProgress::StateEnum::Canceled;
      break;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }
  frontend_->DownloadProgress(item->GetGuid(), item->GetTotalBytes(),
                              item->GetReceivedBytes(), state);
  if (state != Browser::DownloadProgress::StateEnum::InProgress) {
    item->RemoveObserver(this);
    pending_downloads_.erase(item);
  }
}

void BrowserHandler::OnDownloadDestroyed(download::DownloadItem* item) {
  pending_downloads_.erase(item);
}

void BrowserHandler::DownloadWillBegin(FrameTreeNode* ftn,
                                       download::DownloadItem* item) {
  if (!download_events_enabled_)
    return;
  const std::u16string likely_filename = net::GetSuggestedFilename(
      item->GetURL(), item->GetContentDisposition(), std::string(),
      item->GetSuggestedFilename(), item->GetMimeType(), "download");

  frontend_->DownloadWillBegin(
      ftn->current_frame_host()->devtools_frame_token().ToString(),
      item->GetGuid(), item->GetURL().spec(),
      base::UTF16ToUTF8(likely_filename));
  item->AddObserver(this);
  pending_downloads_.insert(item);
}

void BrowserHandler::SetDownloadEventsEnabled(bool enabled) {
  if (!enabled) {
    for (download::DownloadItem* item : pending_downloads_) {
      item->RemoveObserver(this);
    }
    pending_downloads_.clear();
  }
  download_events_enabled_ = enabled;
}

std::unique_ptr<Browser::Histogram> BrowserHandler::GetHistogramData(
    const base::HistogramBase& histogram,
    bool get_delta) {
  std::unique_ptr<base::HistogramSamples> data = histogram.SnapshotSamples();
  std::unique_ptr<base::HistogramSamples> previous_data;
  if (get_delta) {
    auto it = histograms_snapshots_.find(histogram.histogram_name());
    if (it != histograms_snapshots_.end()) {
      previous_data = std::move(it->second);
      data->Subtract(*previous_data);
    }
  }

  auto out_buckets = std::make_unique<Array<Browser::Bucket>>();
  for (const std::unique_ptr<base::SampleCountIterator> it = data->Iterator();
       !it->Done(); it->Next()) {
    base::HistogramBase::Count count;
    base::HistogramBase::Sample low;
    int64_t high;
    it->Get(&low, &high, &count);
    out_buckets->emplace_back(Browser::Bucket::Create()
                                  .SetLow(low)
                                  .SetHigh(high)
                                  .SetCount(count)
                                  .Build());
  }

  auto result = Browser::Histogram::Create()
                    .SetName(histogram.histogram_name())
                    .SetSum(data->sum())
                    .SetCount(data->TotalCount())
                    .SetBuckets(std::move(out_buckets))
                    .Build();

  // Keep track of the data we returned for future delta requests.
  if (get_delta) {
    if (previous_data) {
      // If we had subtracted previous data, re-add it to get the full snapshot.
      data->Add(*previous_data);
    }
    histograms_snapshots_[histogram.histogram_name()] = std::move(data);
  }

  return result;
}

}  // namespace protocol
}  // namespace content
