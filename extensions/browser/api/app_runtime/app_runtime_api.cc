// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_runtime/app_runtime_api.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/feature_switch.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace extensions {

namespace app_runtime = api::app_runtime;

namespace {

void DispatchOnEmbedRequestedEventImpl(
    const ExtensionId& extension_id,
    base::Value::Dict app_embedding_request_data,
    content::BrowserContext* context) {
  base::Value::List args;
  args.Append(std::move(app_embedding_request_data));
  auto event = std::make_unique<Event>(
      events::APP_RUNTIME_ON_EMBED_REQUESTED,
      app_runtime::OnEmbedRequested::kEventName, std::move(args), context);
  EventRouter::Get(context)->DispatchEventWithLazyListener(extension_id,
                                                           std::move(event));

  ExtensionPrefs::Get(context)->SetLastLaunchTime(extension_id,
                                                  base::Time::Now());
}

void DispatchOnLaunchedEventImpl(const ExtensionId& extension_id,
                                 app_runtime::LaunchSource source,
                                 base::Value::Dict launch_data,
                                 BrowserContext* context) {
  launch_data.Set("isDemoSession",
                  ExtensionsBrowserClient::Get()->IsInDemoMode());

  // "Forced app mode" is true for Chrome OS kiosk mode.
  launch_data.Set("isKioskSession",
                  ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode());

  launch_data.Set("isPublicSession",
                  ExtensionsBrowserClient::Get()->IsLoggedInAsPublicAccount());

  base::Value::List args;
  args.Append(std::move(launch_data));
  auto event = std::make_unique<Event>(events::APP_RUNTIME_ON_LAUNCHED,
                                       app_runtime::OnLaunched::kEventName,
                                       std::move(args), context);
  EventRouter::Get(context)->DispatchEventWithLazyListener(extension_id,
                                                           std::move(event));
  ExtensionPrefs::Get(context)->SetLastLaunchTime(extension_id,
                                                  base::Time::Now());
}

#define ASSERT_ENUM_EQUAL(Name, Name2)                                     \
  static_assert(base::to_underlying(extensions::AppLaunchSource::Name) ==  \
                    base::to_underlying(app_runtime::LaunchSource::Name2), \
                "The value of extensions::" #Name                          \
                " and app_runtime::LAUNCH_" #Name2 " should be the same");

app_runtime::LaunchSource GetLaunchSourceEnum(AppLaunchSource source) {
  ASSERT_ENUM_EQUAL(kSourceNone, kNone);
  ASSERT_ENUM_EQUAL(kSourceUntracked, kUntracked);
  ASSERT_ENUM_EQUAL(kSourceAppLauncher, kAppLauncher);
  ASSERT_ENUM_EQUAL(kSourceNewTabPage, kNewTabPage);
  ASSERT_ENUM_EQUAL(kSourceReload, kReload);
  ASSERT_ENUM_EQUAL(kSourceRestart, kRestart);
  ASSERT_ENUM_EQUAL(kSourceLoadAndLaunch, kLoadAndLaunch);
  ASSERT_ENUM_EQUAL(kSourceCommandLine, kCommandLine);
  ASSERT_ENUM_EQUAL(kSourceFileHandler, kFileHandler);
  ASSERT_ENUM_EQUAL(kSourceUrlHandler, kUrlHandler);
  ASSERT_ENUM_EQUAL(kSourceSystemTray, kSystemTray);
  ASSERT_ENUM_EQUAL(kSourceAboutPage, kAboutPage);
  ASSERT_ENUM_EQUAL(kSourceKeyboard, kKeyboard);
  ASSERT_ENUM_EQUAL(kSourceExtensionsPage, kExtensionsPage);
  ASSERT_ENUM_EQUAL(kSourceManagementApi, kManagementApi);
  ASSERT_ENUM_EQUAL(kSourceEphemeralAppDeprecated, kEphemeralApp);
  ASSERT_ENUM_EQUAL(kSourceBackground, kBackground);
  ASSERT_ENUM_EQUAL(kSourceKiosk, kKiosk);
  ASSERT_ENUM_EQUAL(kSourceChromeInternal, kChromeInternal);
  ASSERT_ENUM_EQUAL(kSourceTest, kTest);
  ASSERT_ENUM_EQUAL(kSourceInstalledNotification, kInstalledNotification);
  ASSERT_ENUM_EQUAL(kSourceContextMenu, kContextMenu);
  ASSERT_ENUM_EQUAL(kSourceArc, kArc);
  ASSERT_ENUM_EQUAL(kSourceIntentUrl, kIntentUrl);

  // The +3 accounts for kSourceRunOnOsLogin, kSourceProtocolHandler and
  // kSourceReparenting not having a corresponding entry in
  // app_runtime::LaunchSource.
  static_assert(
      base::to_underlying(extensions::AppLaunchSource::kMaxValue) ==
          base::to_underlying(app_runtime::LaunchSource::kMaxValue) + 3,
      "");

  switch (source) {
    case AppLaunchSource::kSourceNone:
    case AppLaunchSource::kSourceUntracked:
    case AppLaunchSource::kSourceAppLauncher:
    case AppLaunchSource::kSourceNewTabPage:
    case AppLaunchSource::kSourceReload:
    case AppLaunchSource::kSourceRestart:
    case AppLaunchSource::kSourceLoadAndLaunch:
    case AppLaunchSource::kSourceCommandLine:
    case AppLaunchSource::kSourceFileHandler:
    case AppLaunchSource::kSourceUrlHandler:
    case AppLaunchSource::kSourceSystemTray:
    case AppLaunchSource::kSourceAboutPage:
    case AppLaunchSource::kSourceKeyboard:
    case AppLaunchSource::kSourceExtensionsPage:
    case AppLaunchSource::kSourceManagementApi:
    case AppLaunchSource::kSourceEphemeralAppDeprecated:
    case AppLaunchSource::kSourceBackground:
    case AppLaunchSource::kSourceKiosk:
    case AppLaunchSource::kSourceChromeInternal:
    case AppLaunchSource::kSourceTest:
    case AppLaunchSource::kSourceInstalledNotification:
    case AppLaunchSource::kSourceContextMenu:
    case AppLaunchSource::kSourceArc:
    case AppLaunchSource::kSourceIntentUrl:
      return static_cast<app_runtime::LaunchSource>(source);

    // We don't allow extensions to launch an app specifying
    // kSourceRunOnOsLogin, kSourceProtocolHandler or kSourceReparenting as the
    // source. In this case we map it to LaunchSource::kChromeInternal.
    case AppLaunchSource::kSourceRunOnOsLogin:
    case AppLaunchSource::kSourceProtocolHandler:
    case AppLaunchSource::kSourceReparenting:
      return app_runtime::LaunchSource::kChromeInternal;

    // New enumerators must be added here. Because the three previous entries in
    // AppLaunchSource are missing entries in LaunchSource, we need to subtract
    // three to remain in sync with LaunchSource.
    case AppLaunchSource::kSourceAppHomePage:
    case AppLaunchSource::kSourceFocusMode:
    case AppLaunchSource::kSourceSparky:
      return static_cast<app_runtime::LaunchSource>(
          base::to_underlying(source) - 3);
  }
}

}  // namespace

// static
void AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
    content::BrowserContext* context,
    base::Value::Dict embed_app_data,
    const Extension* extension) {
  DispatchOnEmbedRequestedEventImpl(extension->id(), std::move(embed_app_data),
                                    context);
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEvent(
    BrowserContext* context,
    const Extension* extension,
    extensions::AppLaunchSource source,
    std::optional<app_runtime::LaunchData> launch_data) {
  if (!launch_data) {
    launch_data.emplace();
  }
  app_runtime::LaunchSource source_enum = GetLaunchSourceEnum(source);
  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data->source = source_enum;
  }

  DispatchOnLaunchedEventImpl(extension->id(), source_enum,
                              launch_data->ToValue(), context);
}

// static
void AppRuntimeEventRouter::DispatchOnRestartedEvent(
    BrowserContext* context,
    const Extension* extension) {
  auto event = std::make_unique<Event>(events::APP_RUNTIME_ON_RESTARTED,
                                       app_runtime::OnRestarted::kEventName,
                                       base::Value::List(), context);
  EventRouter::Get(context)->DispatchEventToExtension(extension->id(),
                                                      std::move(event));
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEventWithFileEntries(
    BrowserContext* context,
    const Extension* extension,
    extensions::AppLaunchSource source,
    const std::string& handler_id,
    const std::vector<EntryInfo>& entries,
    const std::vector<GrantedFileEntry>& file_entries,
    std::optional<app_runtime::ActionData> action_data) {
  app_runtime::LaunchSource source_enum = GetLaunchSourceEnum(source);

  // TODO(sergeygs): Use the same way of creating an event (using the generated
  // boilerplate) as below in DispatchOnLaunchedEventWithUrl.
  base::Value::Dict launch_data;
  launch_data.Set("id", handler_id);

  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data.Set("source", app_runtime::ToString(source_enum));
  }

  if (action_data) {
    launch_data.Set("actionData", action_data->ToValue());
  }

  base::Value::List items;
  DCHECK(file_entries.size() == entries.size());
  for (size_t i = 0; i < file_entries.size(); ++i) {
    base::Value::Dict launch_item;

    // TODO: The launch item type should be documented in the idl so that this
    // entire function can be strongly typed and built using an
    // app_runtime::LaunchData instance.
    launch_item.Set("fileSystemId", file_entries[i].filesystem_id);
    launch_item.Set("baseName", file_entries[i].registered_name);
    launch_item.Set("mimeType", entries[i].mime_type);
    launch_item.Set("entryId", file_entries[i].id);
    launch_item.Set("isDirectory", entries[i].is_directory);
    items.Append(std::move(launch_item));
  }
  launch_data.Set("items", std::move(items));
  DispatchOnLaunchedEventImpl(extension->id(), source_enum,
                              std::move(launch_data), context);
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEventWithUrl(
    BrowserContext* context,
    const Extension* extension,
    const std::string& handler_id,
    const GURL& url,
    const GURL& referrer_url) {
  app_runtime::LaunchData launch_data;
  app_runtime::LaunchSource source_enum =
      app_runtime::LaunchSource::kUrlHandler;
  launch_data.id = handler_id;
  launch_data.url = url.spec();
  launch_data.referrer_url = referrer_url.spec();
  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data.source = source_enum;
  }
  DispatchOnLaunchedEventImpl(extension->id(), source_enum,
                              launch_data.ToValue(), context);
}

}  // namespace extensions
