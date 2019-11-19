// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/browser_handler.h"

#include <string.h>
#include <algorithm>

#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "url/gurl.h"
#include "v8/include/v8-version-string.h"

namespace content {
namespace protocol {

BrowserHandler::BrowserHandler()
    : DevToolsDomainHandler(Browser::Metainfo::domainName) {}

BrowserHandler::~BrowserHandler() {}

Response BrowserHandler::Disable() {
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
  return Response::OK();
}

void BrowserHandler::Wire(UberDispatcher* dispatcher) {
  Browser::Dispatcher::wire(dispatcher, this);
}

Response BrowserHandler::GetVersion(std::string* protocol_version,
                                    std::string* product,
                                    std::string* revision,
                                    std::string* user_agent,
                                    std::string* js_version) {
  *protocol_version = DevToolsAgentHost::GetProtocolVersion();
  *revision = GetWebKitRevision();
  *product = GetContentClient()->browser()->GetProduct();
  *user_agent = GetContentClient()->browser()->GetUserAgent();
  *js_version = V8_VERSION_STRING;
  return Response::OK();
}

namespace {

// Converts an histogram.
std::unique_ptr<Browser::Histogram> Convert(base::HistogramBase& in_histogram,
                                            bool in_delta) {
  std::unique_ptr<const base::HistogramSamples> in_buckets;
  if (!in_delta) {
    in_buckets = in_histogram.SnapshotSamples();
  } else {
    in_buckets = in_histogram.SnapshotDelta();
  }
  DCHECK(in_buckets);

  auto out_buckets = std::make_unique<Array<Browser::Bucket>>();

  for (const std::unique_ptr<base::SampleCountIterator> bucket_it =
           in_buckets->Iterator();
       !bucket_it->Done(); bucket_it->Next()) {
    base::HistogramBase::Count count;
    base::HistogramBase::Sample low;
    int64_t high;
    bucket_it->Get(&low, &high, &count);
    out_buckets->emplace_back(Browser::Bucket::Create()
                                  .SetLow(low)
                                  .SetHigh(high)
                                  .SetCount(count)
                                  .Build());
  }

  return Browser::Histogram::Create()
      .SetName(in_histogram.histogram_name())
      .SetSum(in_buckets->sum())
      .SetCount(in_buckets->TotalCount())
      .SetBuckets(std::move(out_buckets))
      .Build();
}

// Parses PermissionDescriptors (|descriptor|) into their appropriate
// PermissionType |permission_type| by duplicating the logic in the methods
// //third_party/blink/renderer/modules/permissions:permissions
// ::ParsePermission and
// //content/browser/permissions:permission_service_impl
// ::PermissionDescriptorToPermissionType, producing an error in
// |error_message| as necessary.
// TODO(crbug.com/989983): De-duplicate this logic.
Response PermissionDescriptorToPermissionType(
    std::unique_ptr<protocol::Browser::PermissionDescriptor> descriptor,
    PermissionType* permission_type) {
  const std::string name = descriptor->GetName();

  if (name == "geolocation") {
    *permission_type = PermissionType::GEOLOCATION;
  } else if (name == "camera") {
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
  } else if (name == "accessibility-events") {
    *permission_type = PermissionType::ACCESSIBILITY_EVENTS;
  } else if (name == "clipboard-read") {
    *permission_type = PermissionType::CLIPBOARD_READ;
  } else if (name == "clipboard-write") {
    *permission_type = PermissionType::CLIPBOARD_WRITE;
  } else if (name == "payment-handler") {
    *permission_type = PermissionType::PAYMENT_HANDLER;
  } else if (name == "background-fetch") {
    *permission_type = PermissionType::BACKGROUND_FETCH;
  } else if (name == "idle-detection") {
    *permission_type = PermissionType::IDLE_DETECTION;
  } else if (name == "periodic-background-sync") {
    *permission_type = PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (name == "wake-lock") {
    if (!descriptor->HasType()) {
      return Response::InvalidParams(
          "Could not parse WakeLockPermissionDescriptor with property type");
    }
    const std::string type = descriptor->GetType("");
    if (type == "screen") {
      *permission_type = PermissionType::WAKE_LOCK_SCREEN;
    } else if (type == "system") {
      *permission_type = PermissionType::WAKE_LOCK_SYSTEM;
    } else {
      return Response::InvalidParams("Invalid WakeLockType: " + type);
    }
  } else if (name == "nfc") {
    *permission_type = PermissionType::NFC;
  } else {
    return Response::InvalidParams("Invalid PermissionDescriptor name: " +
                                   name);
  }

  return Response::OK();
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
  } else if (type == protocol::Browser::PermissionTypeEnum::BackgroundSync) {
    *out_type = PermissionType::BACKGROUND_SYNC;
  } else if (type == protocol::Browser::PermissionTypeEnum::Flash) {
    *out_type = PermissionType::FLASH;
  } else if (type == protocol::Browser::PermissionTypeEnum::Sensors) {
    *out_type = PermissionType::SENSORS;
  } else if (type ==
             protocol::Browser::PermissionTypeEnum::AccessibilityEvents) {
    *out_type = PermissionType::ACCESSIBILITY_EVENTS;
  } else if (type == protocol::Browser::PermissionTypeEnum::ClipboardRead) {
    *out_type = PermissionType::CLIPBOARD_READ;
  } else if (type == protocol::Browser::PermissionTypeEnum::ClipboardWrite) {
    *out_type = PermissionType::CLIPBOARD_WRITE;
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
  } else {
    return Response::InvalidParams("Unknown permission type: " + type);
  }
  return Response::OK();
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
  return Response::OK();
}

}  // namespace

Response BrowserHandler::GetHistograms(
    const Maybe<std::string> in_query,
    const Maybe<bool> in_delta,
    std::unique_ptr<Array<Browser::Histogram>>* const out_histograms) {
  // Convert histograms.
  DCHECK(out_histograms);
  *out_histograms = std::make_unique<Array<Browser::Histogram>>();
  for (base::HistogramBase* const h :
       base::StatisticsRecorder::Sort(base::StatisticsRecorder::WithName(
           base::StatisticsRecorder::GetHistograms(),
           in_query.fromMaybe("")))) {
    DCHECK(h);
    (*out_histograms)->emplace_back(Convert(*h, in_delta.fromMaybe(false)));
  }

  return Response::OK();
}

Response BrowserHandler::FindBrowserContext(
    const Maybe<std::string>& browser_context_id,
    BrowserContext** browser_context) {
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::Error("Browser context management is not supported.");
  if (!browser_context_id.isJust()) {
    *browser_context = delegate->GetDefaultBrowserContext();
    if (*browser_context == nullptr)
      return Response::Error("Browser context management is not supported.");
    return Response::OK();
  }

  std::string context_id = browser_context_id.fromJust();
  for (auto* context : delegate->GetBrowserContexts()) {
    if (context->UniqueId() == context_id) {
      *browser_context = context;
      return Response::OK();
    }
  }
  return Response::InvalidParams("Failed to find browser context for id " +
                                 context_id);
}

Response BrowserHandler::SetPermission(
    const std::string& origin,
    std::unique_ptr<protocol::Browser::PermissionDescriptor> permission,
    const protocol::Browser::PermissionSetting& setting,
    Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.isSuccess())
    return response;

  PermissionType type;
  Response parse_response =
      PermissionDescriptorToPermissionType(std::move(permission), &type);
  if (!parse_response.isSuccess())
    return parse_response;

  blink::mojom::PermissionStatus permission_status;
  Response setting_response =
      PermissionSettingToPermissionStatus(setting, &permission_status);
  if (!setting_response.isSuccess())
    return setting_response;

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(browser_context);
  url::Origin overridden_origin = url::Origin::Create(GURL(origin));
  if (overridden_origin.opaque())
    return Response::InvalidParams(
        "Permission can't be granted to opaque origins.");

  PermissionControllerImpl::OverrideStatus status =
      permission_controller->SetOverrideForDevTools(overridden_origin, type,
                                                    permission_status);
  if (status != PermissionControllerImpl::OverrideStatus::kOverrideSet) {
    return Response::InvalidParams(
        "Permission can't be granted in current context.");
  }
  contexts_with_overridden_permissions_.insert(
      browser_context_id.fromMaybe(std::string()));
  return Response::OK();
}

Response BrowserHandler::GrantPermissions(
    const std::string& origin,
    std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
        permissions,
    Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.isSuccess())
    return response;

  std::vector<PermissionType> internal_permissions;
  internal_permissions.reserve(permissions->size());
  for (const protocol::Browser::PermissionType& t : *permissions) {
    PermissionType type;
    Response type_response = FromProtocolPermissionType(t, &type);
    if (!type_response.isSuccess())
      return type_response;
    internal_permissions.push_back(type);
  }

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(browser_context);
  url::Origin overridden_origin = url::Origin::Create(GURL(origin));
  if (overridden_origin.opaque())
    return Response::InvalidParams(
        "Permission can't be granted to opaque origins.");

  PermissionControllerImpl::OverrideStatus status =
      permission_controller->GrantOverridesForDevTools(overridden_origin,
                                                       internal_permissions);
  if (status != PermissionControllerImpl::OverrideStatus::kOverrideSet) {
    return Response::InvalidParams(
        "Permissions can't be granted in current context.");
  }
  contexts_with_overridden_permissions_.insert(
      browser_context_id.fromMaybe(""));
  return Response::OK();
}

Response BrowserHandler::ResetPermissions(
    Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.isSuccess())
    return response;
  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(browser_context);
  permission_controller->ResetOverridesForDevTools();
  contexts_with_overridden_permissions_.erase(browser_context_id.fromMaybe(""));
  return Response::OK();
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

  // Convert histogram.
  DCHECK(out_histogram);
  *out_histogram = Convert(*in_histogram, in_delta.fromMaybe(false));

  return Response::OK();
}

Response BrowserHandler::GetBrowserCommandLine(
    std::unique_ptr<protocol::Array<std::string>>* arguments) {
  *arguments = std::make_unique<protocol::Array<std::string>>();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // The commandline is potentially sensitive, only return it if it
  // contains kEnableAutomation.
  if (command_line->HasSwitch(switches::kEnableAutomation)) {
    for (const auto& arg : command_line->argv()) {
#if defined(OS_WIN)
      (*arguments)->emplace_back(base::UTF16ToUTF8(arg));
#else
      (*arguments)->emplace_back(arg);
#endif
    }
    return Response::OK();
  } else {
    return Response::Error(
        "Command line not returned because --enable-automation not set.");
  }
}

Response BrowserHandler::Crash() {
  CHECK(false);
  return Response::OK();
}

Response BrowserHandler::CrashGpuProcess() {
  GpuProcessHost::CallOnIO(GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->Crash();
                           }));
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
