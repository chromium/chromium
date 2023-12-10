// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/web_ui_user_script_loader.h"

#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/guest_view/web_view/web_ui/web_ui_url_fetcher.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "url/gurl.h"

namespace {

void SerializeOnBlockingTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    extensions::UserScriptList user_scripts,
    extensions::UserScriptLoader::LoadScriptsCallback callback) {
  base::ReadOnlySharedMemoryRegion memory =
      extensions::UserScriptLoader::Serialize(user_scripts);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(user_scripts),
                                std::move(memory)));
}

}  // namespace

struct WebUIUserScriptLoader::UserScriptRenderInfo {
  const int render_process_id;
  const int render_frame_id;

  UserScriptRenderInfo(int render_process_id, int render_frame_id)
      : render_process_id(render_process_id),
        render_frame_id(render_frame_id) {}
};

WebUIUserScriptLoader::WebUIUserScriptLoader(
    content::BrowserContext* browser_context,
    const GURL& url)
    : UserScriptLoader(browser_context,
                       extensions::mojom::HostID(
                           extensions::mojom::HostID::HostID::HostType::kWebUi,
                           url.spec())),
      complete_fetchers_(0) {
  SetReady(true);
}

WebUIUserScriptLoader::~WebUIUserScriptLoader() {
}

void WebUIUserScriptLoader::AddScripts(extensions::UserScriptList scripts,
                                       int render_process_id,
                                       int render_frame_id,
                                       ScriptsLoadedCallback callback) {
  UserScriptRenderInfo info(render_process_id, render_frame_id);
  for (const std::unique_ptr<extensions::UserScript>& script : scripts) {
    script_render_info_map_.emplace(script->id(), info);
  }

  extensions::UserScriptLoader::AddScripts(std::move(scripts),
                                           std::move(callback));
}

void WebUIUserScriptLoader::LoadScripts(
    extensions::UserScriptList user_scripts,
    const std::set<std::string>& added_script_ids,
    LoadScriptsCallback callback) {
  DCHECK(user_scripts_cache_.empty()) << "Loading scripts in flight.";
  user_scripts_cache_ = std::move(user_scripts);
  scripts_loaded_callback_ = std::move(callback);

  // The total number of the tasks is used to trace whether all the fetches
  // are complete. Therefore, we store all the fetcher pointers in |fetchers_|
  // before we get theis number. Once we get the total number, start each
  // fetch tasks.
  DCHECK_EQ(0u, complete_fetchers_);

  for (const std::unique_ptr<extensions::UserScript>& script :
       user_scripts_cache_) {
    if (added_script_ids.count(script->id()) == 0)
      continue;

    auto iter = script_render_info_map_.find(script->id());
    DCHECK(iter != script_render_info_map_.end());
    int render_process_id = iter->second.render_process_id;
    int render_frame_id = iter->second.render_frame_id;

    CreateWebUIURLFetchers(script->js_scripts(), render_process_id,
                           render_frame_id);
    CreateWebUIURLFetchers(script->css_scripts(), render_process_id,
                           render_frame_id);

    script_render_info_map_.erase(script->id());
  }

  // If no fetch is needed, call OnWebUIURLFetchComplete directly.
  if (fetchers_.empty()) {
    OnWebUIURLFetchComplete();
    return;
  }
  for (const auto& fetcher : fetchers_)
    fetcher->Start();
}

void WebUIUserScriptLoader::CreateWebUIURLFetchers(
    const extensions::UserScript::ContentList& contents,
    int render_process_id,
    int render_frame_id) {
  for (const std::unique_ptr<extensions::UserScript::Content>& content :
       contents) {
    if (content->GetContent().empty()) {
      // The WebUIUserScriptLoader owns these WebUIURLFetchers. Once the
      // loader is destroyed, all the fetchers will be destroyed. Therefore,
      // we are sure it is safe to use base::Unretained(this) here.
      // `user_scripts_cache_` retains ownership of the scripts while they are
      // being loaded, so passing a raw pointer to `content` below to
      // WebUIUserScriptLoader is also safe.
      std::unique_ptr<WebUIURLFetcher> fetcher(new WebUIURLFetcher(
          render_process_id, render_frame_id, content->url(),
          base::BindOnce(&WebUIUserScriptLoader::OnSingleWebUIURLFetchComplete,
                         base::Unretained(this), content.get())));
      fetchers_.push_back(std::move(fetcher));
    }
  }
}

void WebUIUserScriptLoader::OnSingleWebUIURLFetchComplete(
    extensions::UserScript::Content* content,
    bool success,
    std::unique_ptr<std::string> data) {
  if (success) {
    // Remove BOM from |data|.
    if (base::StartsWith(*data, base::kUtf8ByteOrderMark,
                         base::CompareCase::SENSITIVE)) {
      data->erase(0, strlen(base::kUtf8ByteOrderMark));
    }
    content->set_content(std::move(*data));
  }

  ++complete_fetchers_;
  if (complete_fetchers_ == fetchers_.size()) {
    complete_fetchers_ = 0;
    OnWebUIURLFetchComplete();
    fetchers_.clear();
  }
}

void WebUIUserScriptLoader::OnWebUIURLFetchComplete() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SerializeOnBlockingTask,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(user_scripts_cache_),
                     std::move(scripts_loaded_callback_)));
  user_scripts_cache_.clear();
}
