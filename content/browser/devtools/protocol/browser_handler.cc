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
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
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
      permission_controller->ResetPermissionOverridesForDevTools();
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
  *product = GetContentClient()->GetProduct();
  *user_agent = GetContentClient()->GetUserAgent();
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
    out_buckets->addItem(Browser::Bucket::Create()
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
  } else {
    return Response::InvalidParams("Unknown permission type: " + type);
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
    (*out_histograms)->addItem(Convert(*h, in_delta.fromMaybe(false)));
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

Response BrowserHandler::GrantPermissions(
    const std::string& origin,
    std::unique_ptr<protocol::Array<protocol::Browser::PermissionType>>
        permissions,
    Maybe<std::string> browser_context_id) {
  BrowserContext* browser_context = nullptr;
  Response response = FindBrowserContext(browser_context_id, &browser_context);
  if (!response.isSuccess())
    return response;
  PermissionControllerImpl::PermissionOverrides overrides;
  for (size_t i = 0; i < permissions->length(); ++i) {
    PermissionType type;
    Response type_response =
        FromProtocolPermissionType(permissions->get(i), &type);
    if (!type_response.isSuccess())
      return type_response;
    overrides.insert(type);
  }

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(browser_context);
  GURL url = GURL(origin).GetOrigin();
  permission_controller->SetPermissionOverridesForDevTools(url, overrides);
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
  permission_controller->ResetPermissionOverridesForDevTools();
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
  *arguments = protocol::Array<std::string>::create();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // The commandline is potentially sensitive, only return it if it
  // contains kEnableAutomation.
  if (command_line->HasSwitch(switches::kEnableAutomation)) {
    for (const auto& arg : command_line->argv()) {
#if defined(OS_WIN)
      (*arguments)->addItem(base::UTF16ToUTF8(arg.c_str()));
#else
      (*arguments)->addItem(arg.c_str());
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

}  // namespace protocol
}  // namespace content
