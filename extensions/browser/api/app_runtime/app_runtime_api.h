// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_APP_RUNTIME_APP_RUNTIME_API_H_
#define EXTENSIONS_BROWSER_API_APP_RUNTIME_APP_RUNTIME_API_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/constants.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace extensions {

namespace api::app_runtime {
struct ActionData;
struct LaunchData;
}  // namespace api::app_runtime

class Extension;
struct EntryInfo;
struct GrantedFileEntry;

class AppRuntimeEventRouter {
 public:
  // Dispatches the onEmbedRequested event to the given app.
  static void DispatchOnEmbedRequestedEvent(
      content::BrowserContext* context,
      base::Value::Dict app_embedding_request_data,
      const Extension* extension);

  // Dispatches the onLaunched event to the given app.
  static void DispatchOnLaunchedEvent(
      content::BrowserContext* context,
      const Extension* extension,
      AppLaunchSource source,
      std::optional<api::app_runtime::LaunchData> launch_data);

  // Dispatches the onRestarted event to the given app, providing a list of
  // restored file entries from the previous run.
  static void DispatchOnRestartedEvent(content::BrowserContext* context,
                                       const Extension* extension);

  // TODO(benwells): Update this comment, it is out of date.
  // Dispatches the onLaunched event to the given app, providing launch data of
  // the form:
  // {
  //   "intent" : {
  //     "type" : "chrome-extension://fileentry",
  //     "data" : a FileEntry,
  //     "postResults" : a null function,
  //     "postFailure" : a null function
  //   }
  // }

  // The FileEntries are created from |file_system_id| and |base_name|.
  // |handler_id| corresponds to the id of the file_handlers item in the
  // manifest that resulted in a match which triggered this launch.
  static void DispatchOnLaunchedEventWithFileEntries(
      content::BrowserContext* context,
      const Extension* extension,
      AppLaunchSource source,
      const std::string& handler_id,
      const std::vector<EntryInfo>& entries,
      const std::vector<GrantedFileEntry>& file_entries,
      std::optional<api::app_runtime::ActionData> action_data);

  // |handler_id| corresponds to the id of the url_handlers item
  // in the manifest that resulted in a match which triggered this launch.
  static void DispatchOnLaunchedEventWithUrl(content::BrowserContext* context,
                                             const Extension* extension,
                                             const std::string& handler_id,
                                             const GURL& url,
                                             const GURL& referrer_url);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_APP_RUNTIME_APP_RUNTIME_API_H_
