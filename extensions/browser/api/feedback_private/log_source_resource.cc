// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/log_source_resource.h"

#include "base/lazy_instance.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// For managing API resources of type LogSourceResource.
static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<LogSourceResource>>>::DestructorAtExit
    g_log_source_resource_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<LogSourceResource>>*
ApiResourceManager<LogSourceResource>::GetFactoryInstance() {
  return g_log_source_resource_factory.Pointer();
}

LogSourceResource::LogSourceResource(
    const ExtensionId& extension_id,
    std::unique_ptr<system_logs::SystemLogsSource> source)
    : ApiResource(extension_id), source_(source.release()) {}

LogSourceResource::~LogSourceResource() {
  if (!unregister_callback_.is_null())
    std::move(unregister_callback_).Run();
}

}  // namespace extensions
