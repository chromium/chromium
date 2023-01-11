// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

namespace content {
namespace {

const char kURLDataManagerKeyName[] = "url_data_manager";

base::LazyInstance<base::Lock>::Leaky g_delete_lock = LAZY_INSTANCE_INITIALIZER;

URLDataManager* GetFromBrowserContext(BrowserContext* context) {
  if (!context->GetUserData(kURLDataManagerKeyName)) {
    context->SetUserData(kURLDataManagerKeyName,
                         std::make_unique<URLDataManager>(context));
  }
  return static_cast<URLDataManager*>(
      context->GetUserData(kURLDataManagerKeyName));
}

}  // namespace

// static
URLDataManager::URLDataSources* URLDataManager::data_sources_ PT_GUARDED_BY(
    g_delete_lock.Get()) = nullptr;

URLDataManager::URLDataManager(BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

URLDataManager::~URLDataManager() = default;

void URLDataManager::AddDataSource(URLDataSourceImpl* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  URLDataManagerBackend::GetForBrowserContext(browser_context_)
      ->AddDataSource(source);
}

void URLDataManager::UpdateWebUIDataSource(const std::string& source_name,
                                           const base::Value::Dict& update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  URLDataManagerBackend::GetForBrowserContext(browser_context_)
      ->UpdateWebUIDataSource(source_name, update);
}

// static
void URLDataManager::DeleteDataSources() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  URLDataSources sources;
  {
    base::AutoLock lock(g_delete_lock.Get());
    if (!data_sources_)
      return;
    data_sources_->swap(sources);
  }
  for (size_t i = 0; i < sources.size(); ++i)
    delete sources[i];
}

// static
void URLDataManager::DeleteDataSource(const URLDataSourceImpl* data_source) {
  // Invoked when a DataSource is no longer referenced and needs to be deleted.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // We're on the UI thread, delete right away.
    delete data_source;
    return;
  }

  // We're not on the UI thread, add the DataSource to the list of DataSources
  // to delete.
  bool schedule_delete = false;
  {
    base::AutoLock lock(g_delete_lock.Get());
    if (!data_sources_)
      data_sources_ = new URLDataSources();
    schedule_delete = data_sources_->empty();
    data_sources_->push_back(data_source);
  }
  if (schedule_delete) {
    // Schedule a task to delete the DataSource back on the UI thread.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&URLDataManager::DeleteDataSources));
  }
}

// static
void URLDataManager::AddDataSource(BrowserContext* browser_context,
                                   std::unique_ptr<URLDataSource> source) {
  std::string name = source->GetSource();
  auto source_impl =
      base::MakeRefCounted<URLDataSourceImpl>(name, std::move(source));
  GetFromBrowserContext(browser_context)->AddDataSource(source_impl.get());
}

// static
void URLDataManager::AddWebUIDataSource(BrowserContext* browser_context,
                                        WebUIDataSource* source) {
  WebUIDataSourceImpl* impl = static_cast<WebUIDataSourceImpl*>(source);
  GetFromBrowserContext(browser_context)->AddDataSource(impl);
}

void URLDataManager::UpdateWebUIDataSource(BrowserContext* browser_context,
                                           const std::string& source_name,
                                           const base::Value::Dict& update) {
  GetFromBrowserContext(browser_context)
      ->UpdateWebUIDataSource(source_name, std::move(update));
}

// static
bool URLDataManager::IsScheduledForDeletion(
    const URLDataSourceImpl* data_source) {
  base::AutoLock lock(g_delete_lock.Get());
  return data_sources_ && base::Contains(*data_sources_, data_source);
}

}  // namespace content
