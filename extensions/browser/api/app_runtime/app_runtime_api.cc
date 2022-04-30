// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/app_runtime/app_runtime_api.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/constants.h"
#include "extensions/common/feature_switch.h"
#include "url/gurl.h"

using content::BrowserContext;

namespace extensions {

namespace app_runtime = api::app_runtime;

namespace {

void DispatchOnEmbedRequestedEventImpl(
    const std::string& extension_id,
    std::unique_ptr<base::DictionaryValue> app_embedding_request_data,
    content::BrowserContext* context) {
  std::vector<base::Value> args;
  args.emplace_back(
      base::Value::FromUniquePtrValue(std::move(app_embedding_request_data)));
  auto event = std::make_unique<Event>(
      events::APP_RUNTIME_ON_EMBED_REQUESTED,
      app_runtime::OnEmbedRequested::kEventName, std::move(args), context);
  EventRouter::Get(context)
      ->DispatchEventWithLazyListener(extension_id, std::move(event));

  ExtensionPrefs::Get(context)
      ->SetLastLaunchTime(extension_id, base::Time::Now());
}

void DispatchOnLaunchedEventImpl(
    const std::string& extension_id,
    app_runtime::LaunchSource source,
    std::unique_ptr<base::DictionaryValue> launch_data,
    BrowserContext* context) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunchSource", source,
                            app_runtime::LaunchSource::LAUNCH_SOURCE_LAST + 1);

  launch_data->SetBoolKey("isDemoSession",
                          ExtensionsBrowserClient::Get()->IsInDemoMode());

  // "Forced app mode" is true for Chrome OS kiosk mode.
  launch_data->SetBoolKey(
      "isKioskSession",
      ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode());

  launch_data->SetBoolKey(
      "isPublicSession",
      ExtensionsBrowserClient::Get()->IsLoggedInAsPublicAccount());

  std::vector<base::Value> args;
  args.emplace_back(base::Value::FromUniquePtrValue(std::move(launch_data)));
  auto event = std::make_unique<Event>(events::APP_RUNTIME_ON_LAUNCHED,
                                       app_runtime::OnLaunched::kEventName,
                                       std::move(args), context);
  EventRouter::Get(context)
      ->DispatchEventWithLazyListener(extension_id, std::move(event));
  ExtensionPrefs::Get(context)
      ->SetLastLaunchTime(extension_id, base::Time::Now());
}

#define ASSERT_ENUM_EQUAL(Name, Name2)                                 \
  static_assert(static_cast<int>(extensions::AppLaunchSource::Name) == \
                    static_cast<int>(app_runtime::LAUNCH_##Name2),     \
                "The value of extensions::" #Name                      \
                " and app_runtime::LAUNCH_" #Name2 " should be the same");

app_runtime::LaunchSource GetLaunchSourceEnum(
    extensions::AppLaunchSource source) {
  ASSERT_ENUM_EQUAL(kSourceNone, SOURCE_NONE);
  ASSERT_ENUM_EQUAL(kSourceUntracked, SOURCE_UNTRACKED);
  ASSERT_ENUM_EQUAL(kSourceAppLauncher, SOURCE_APP_LAUNCHER);
  ASSERT_ENUM_EQUAL(kSourceNewTabPage, SOURCE_NEW_TAB_PAGE);
  ASSERT_ENUM_EQUAL(kSourceReload, SOURCE_RELOAD);
  ASSERT_ENUM_EQUAL(kSourceRestart, SOURCE_RESTART);
  ASSERT_ENUM_EQUAL(kSourceLoadAndLaunch, SOURCE_LOAD_AND_LAUNCH);
  ASSERT_ENUM_EQUAL(kSourceCommandLine, SOURCE_COMMAND_LINE);
  ASSERT_ENUM_EQUAL(kSourceFileHandler, SOURCE_FILE_HANDLER);
  ASSERT_ENUM_EQUAL(kSourceUrlHandler, SOURCE_URL_HANDLER);
  ASSERT_ENUM_EQUAL(kSourceSystemTray, SOURCE_SYSTEM_TRAY);
  ASSERT_ENUM_EQUAL(kSourceAboutPage, SOURCE_ABOUT_PAGE);
  ASSERT_ENUM_EQUAL(kSourceKeyboard, SOURCE_KEYBOARD);
  ASSERT_ENUM_EQUAL(kSourceExtensionsPage, SOURCE_EXTENSIONS_PAGE);
  ASSERT_ENUM_EQUAL(kSourceManagementApi, SOURCE_MANAGEMENT_API);
  ASSERT_ENUM_EQUAL(kSourceEphemeralAppDeprecated, SOURCE_EPHEMERAL_APP);
  ASSERT_ENUM_EQUAL(kSourceBackground, SOURCE_BACKGROUND);
  ASSERT_ENUM_EQUAL(kSourceKiosk, SOURCE_KIOSK);
  ASSERT_ENUM_EQUAL(kSourceChromeInternal, SOURCE_CHROME_INTERNAL);
  ASSERT_ENUM_EQUAL(kSourceTest, SOURCE_TEST);
  ASSERT_ENUM_EQUAL(kSourceInstalledNotification,
                    SOURCE_INSTALLED_NOTIFICATION);
  ASSERT_ENUM_EQUAL(kSourceContextMenu, SOURCE_CONTEXT_MENU);
  ASSERT_ENUM_EQUAL(kSourceArc, SOURCE_ARC);
  ASSERT_ENUM_EQUAL(kSourceIntentUrl, SOURCE_INTENT_URL);

  // We don't allow extensions to launch an app specifying RunOnOSLogin,
  // ProtocolHandler or Reparenting as the source. In this case we map
  // it to SOURCE_CHROME_INTERNAL.
  if (source == extensions::AppLaunchSource::kSourceRunOnOsLogin ||
      source == extensions::AppLaunchSource::kSourceProtocolHandler ||
      source == extensions::AppLaunchSource::kSourceReparenting)
    source = extensions::AppLaunchSource::kSourceChromeInternal;

  // The +3 accounts for kSourceRunOnOsLogin, kSourceProtocolHandler and
  // kSourceReparenting not having a corresponding entry in
  // app_runtime::LaunchSource.
  static_assert(static_cast<int>(extensions::AppLaunchSource::kMaxValue) ==
                    app_runtime::LaunchSource::LAUNCH_SOURCE_LAST + 3,
                "");

  return static_cast<app_runtime::LaunchSource>(source);
}

}  // namespace

// static
void AppRuntimeEventRouter::DispatchOnEmbedRequestedEvent(
    content::BrowserContext* context,
    std::unique_ptr<base::DictionaryValue> embed_app_data,
    const Extension* extension) {
  DispatchOnEmbedRequestedEventImpl(extension->id(), std::move(embed_app_data),
                                    context);
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEvent(
    BrowserContext* context,
    const Extension* extension,
    extensions::AppLaunchSource source,
    std::unique_ptr<app_runtime::LaunchData> launch_data) {
  if (!launch_data)
    launch_data = std::make_unique<app_runtime::LaunchData>();
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
                                       std::vector<base::Value>(), context);
  EventRouter::Get(context)
      ->DispatchEventToExtension(extension->id(), std::move(event));
}

// static
void AppRuntimeEventRouter::DispatchOnLaunchedEventWithFileEntries(
    BrowserContext* context,
    const Extension* extension,
    extensions::AppLaunchSource source,
    const std::string& handler_id,
    const std::vector<EntryInfo>& entries,
    const std::vector<GrantedFileEntry>& file_entries,
    std::unique_ptr<app_runtime::ActionData> action_data) {
  app_runtime::LaunchSource source_enum = GetLaunchSourceEnum(source);

  // TODO(sergeygs): Use the same way of creating an event (using the generated
  // boilerplate) as below in DispatchOnLaunchedEventWithUrl.
  std::unique_ptr<base::DictionaryValue> launch_data(new base::DictionaryValue);
  launch_data->SetStringKey("id", handler_id);

  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data->SetStringKey("source", app_runtime::ToString(source_enum));
  }

  if (action_data)
    launch_data->SetKey(
        "actionData", base::Value::FromUniquePtrValue(action_data->ToValue()));

  base::Value items(base::Value::Type::LIST);
  DCHECK(file_entries.size() == entries.size());
  for (size_t i = 0; i < file_entries.size(); ++i) {
    std::unique_ptr<base::DictionaryValue> launch_item(
        new base::DictionaryValue);

    // TODO: The launch item type should be documented in the idl so that this
    // entire function can be strongly typed and built using an
    // app_runtime::LaunchData instance.
    launch_item->SetStringKey("fileSystemId", file_entries[i].filesystem_id);
    launch_item->SetStringKey("baseName", file_entries[i].registered_name);
    launch_item->SetStringKey("mimeType", entries[i].mime_type);
    launch_item->SetStringKey("entryId", file_entries[i].id);
    launch_item->SetBoolKey("isDirectory", entries[i].is_directory);
    items.Append(std::move(*launch_item));
  }
  launch_data->SetKey("items", std::move(items));
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
      app_runtime::LAUNCH_SOURCE_URL_HANDLER;
  launch_data.id = std::make_unique<std::string>(handler_id);
  launch_data.url = std::make_unique<std::string>(url.spec());
  launch_data.referrer_url = std::make_unique<std::string>(referrer_url.spec());
  if (extensions::FeatureSwitch::trace_app_source()->IsEnabled()) {
    launch_data.source = source_enum;
  }
  DispatchOnLaunchedEventImpl(extension->id(), source_enum,
                              launch_data.ToValue(), context);
}

}  // namespace extensions
