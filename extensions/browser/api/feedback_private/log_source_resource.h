// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_RESOURCE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_RESOURCE_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/feedback_private/log_source_access_manager.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Holds a SystemLogsSource object that is used by an extension using the
// feedbackPrivate API.
class LogSourceResource : public ApiResource {
 public:
  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

  LogSourceResource(const ExtensionId& extension_id,
                    std::unique_ptr<system_logs::SystemLogsSource> source);

  LogSourceResource(const LogSourceResource&) = delete;
  LogSourceResource& operator=(const LogSourceResource&) = delete;

  ~LogSourceResource() override;

  system_logs::SystemLogsSource* GetLogSource() const { return source_.get(); }

  void set_unregister_callback(base::OnceClosure unregister_callback) {
    unregister_callback_ = std::move(unregister_callback);
  }

 private:
  friend class ApiResourceManager<LogSourceResource>;
  static const char* service_name() { return "LogSourceResource"; }

  std::unique_ptr<system_logs::SystemLogsSource> source_;

  // This unregisters the LogSourceResource from a LogSourceAccessManager when
  // this resource is cleaned up.
  base::OnceClosure unregister_callback_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_LOG_SOURCE_RESOURCE_H_
