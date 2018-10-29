// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/script_executor.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/url_loader_factory_manager.h"
#include "extensions/common/extension_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace base {
class ListValue;
}  // namespace base

namespace extensions {

namespace {

const char kRendererDestroyed[] = "The tab was closed.";
const char kFrameRemoved[] = "The frame was removed.";

// Generates an injection key based on the host ID and either the file URL, if
// available, or the code string. The format of the key is
// "<type><host_id><digest>", where <type> is one of "F" (file) and "C" (code),
// <host_id> is the host ID, and <digest> is the hash digest (base::Hash) of
// the file URL or the code string, respectively.
const std::string GenerateInjectionKey(const HostID& host_id,
                                       const GURL& file_url,
                                       const std::string& code) {
  const std::string& source = file_url.is_valid() ? file_url.spec() : code;
  return base::StringPrintf("%c%s%u", file_url.is_valid() ? 'F' : 'C',
                            host_id.id().c_str(), base::Hash(source));
}

// A handler for a single injection request. On creation this will send the
// injection request to the renderer, and it will be destroyed after either the
// corresponding response comes from the renderer, or the renderer is destroyed.
class Handler : public content::WebContentsObserver {
 public:
  Handler(ScriptsExecutedNotification observer,
          content::WebContents* web_contents,
          const ExtensionMsg_ExecuteCode_Params& params,
          ScriptExecutor::FrameScope scope,
          int frame_id,
          const ScriptExecutor::ScriptFinishedCallback& callback)
      : content::WebContentsObserver(web_contents),
        observer_(std::move(observer)),
        host_id_(params.host_id),
        request_id_(params.request_id),
        include_sub_frames_(scope == ScriptExecutor::INCLUDE_SUB_FRAMES),
        root_rfh_(ExtensionApiFrameIdMap::GetRenderFrameHostById(web_contents,
                                                                 frame_id)),
        root_is_main_frame_(root_rfh_ ? !root_rfh_->GetParent() : false),
        callback_(callback) {
    if (root_rfh_) {
      if (include_sub_frames_) {
        web_contents->ForEachFrame(base::BindRepeating(
            &Handler::SendExecuteCode, base::Unretained(this), params));
      } else {
        SendExecuteCode(params, root_rfh_);
      }
    }

    if (pending_render_frames_.empty())
      Finish();
  }

 private:
  // This class manages its own lifetime.
  ~Handler() override {}

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { Finish(); }

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    // Unpack by hand to check the request_id, since there may be multiple
    // requests in flight but only one is for this.
    if (message.type() != ExtensionHostMsg_ExecuteCodeFinished::ID)
      return false;

    int message_request_id;
    base::PickleIterator iter(message);
    CHECK(iter.ReadInt(&message_request_id));

    if (message_request_id != request_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(Handler, message, render_frame_host)
      IPC_MESSAGE_HANDLER(ExtensionHostMsg_ExecuteCodeFinished,
                          OnExecuteCodeFinished)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (pending_render_frames_.erase(render_frame_host) == 1 &&
        pending_render_frames_.empty()) {
      Finish();
    }
  }

  // Sends an ExecuteCode message to the given frame host, and increments
  // the number of pending messages.
  void SendExecuteCode(const ExtensionMsg_ExecuteCode_Params& params,
                       content::RenderFrameHost* frame) {
    if (!frame->IsRenderFrameLive())
      return;
    DCHECK(!root_is_main_frame_ || ShouldIncludeFrame(frame));
    if (!root_is_main_frame_ && !ShouldIncludeFrame(frame))
      return;
    pending_render_frames_.insert(frame);
    URLLoaderFactoryManager::WillExecuteCode(frame, host_id_);
    frame->Send(new ExtensionMsg_ExecuteCode(frame->GetRoutingID(), params));
  }

  // Returns whether a frame is the root frame or a descendant of it.
  bool ShouldIncludeFrame(content::RenderFrameHost* frame) {
    while (frame) {
      if (frame == root_rfh_)
        return true;
      frame = frame->GetParent();
    }
    return false;
  }

  // Handles the ExecuteCodeFinished message.
  void OnExecuteCodeFinished(content::RenderFrameHost* render_frame_host,
                             int request_id,
                             const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& result_list) {
    DCHECK_EQ(request_id_, request_id);
    DCHECK(!pending_render_frames_.empty());
    bool erased = pending_render_frames_.erase(render_frame_host) == 1;
    DCHECK(erased);
    bool is_root_frame = root_rfh_ == render_frame_host;

    // Set the result, if there is one.
    const base::Value* script_value = nullptr;
    if (result_list.Get(0u, &script_value)) {
      // If this is the main result, we put it at index 0. Otherwise, we just
      // append it at the end.
      if (is_root_frame && !results_.empty())
        CHECK(results_.Insert(0u, script_value->CreateDeepCopy()));
      else
        results_.Append(script_value->CreateDeepCopy());
    }

    if (is_root_frame) {  // Only use the root frame's error and url.
      root_frame_error_ = error;
      root_frame_url_ = on_url;
    }

    // Wait until the final request finishes before reporting back.
    if (pending_render_frames_.empty())
      Finish();
  }

  void Finish() {
    if (root_frame_url_.is_empty()) {
      // We never finished the root frame injection.
      root_frame_error_ =
          root_is_main_frame_ ? kRendererDestroyed : kFrameRemoved;
      results_.Clear();
    }

    if (!observer_.is_null() && root_frame_error_.empty() &&
        host_id_.type() == HostID::EXTENSIONS) {
      observer_.Run(web_contents(), {{host_id_.id(), {}}}, root_frame_url_);
    }

    if (!callback_.is_null())
      callback_.Run(root_frame_error_, root_frame_url_, results_);
    delete this;
  }

  ScriptsExecutedNotification observer_;

  // The id of the host (the extension or the webui) doing the injection.
  HostID host_id_;

  // The request id of the injection.
  int request_id_;

  // Whether to inject in |root_rfh_| and all of its descendant frames.
  bool include_sub_frames_;

  // The frame (and optionally its descendant frames) where the injection will
  // occur.
  content::RenderFrameHost* root_rfh_;

  // Whether |root_rfh_| is the main frame of a tab.
  bool root_is_main_frame_;

  // The hosts of the still-running injections.
  std::set<content::RenderFrameHost*> pending_render_frames_;

  // The results of the injection.
  base::ListValue results_;

  // The error from injecting into the root frame.
  std::string root_frame_error_;

  // The url of the root frame.
  GURL root_frame_url_;

  // The callback to run after all injections complete.
  ScriptExecutor::ScriptFinishedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(Handler);
};

}  // namespace

ScriptExecutor::ScriptExecutor(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  CHECK(web_contents_);
}

ScriptExecutor::~ScriptExecutor() {}

void ScriptExecutor::ExecuteScript(const HostID& host_id,
                                   ScriptExecutor::ScriptType script_type,
                                   const std::string& code,
                                   ScriptExecutor::FrameScope frame_scope,
                                   int frame_id,
                                   ScriptExecutor::MatchAboutBlank about_blank,
                                   UserScript::RunLocation run_at,
                                   ScriptExecutor::WorldType world_type,
                                   ScriptExecutor::ProcessType process_type,
                                   const GURL& webview_src,
                                   const GURL& file_url,
                                   bool user_gesture,
                                   base::Optional<CSSOrigin> css_origin,
                                   ScriptExecutor::ResultType result_type,
                                   const ScriptFinishedCallback& callback) {
  if (host_id.type() == HostID::EXTENSIONS) {
    // Don't execute if the extension has been unloaded.
    const Extension* extension =
        ExtensionRegistry::Get(web_contents_->GetBrowserContext())
            ->enabled_extensions().GetByID(host_id.id());
    if (!extension)
      return;
  } else {
    CHECK(process_type == WEB_VIEW_PROCESS);
  }

  ExtensionMsg_ExecuteCode_Params params;
  params.request_id = next_request_id_++;
  params.host_id = host_id;
  params.is_javascript = (script_type == JAVASCRIPT);
  params.code = code;
  params.match_about_blank = (about_blank == MATCH_ABOUT_BLANK);
  params.run_at = run_at;
  params.in_main_world = (world_type == MAIN_WORLD);
  params.is_web_view = (process_type == WEB_VIEW_PROCESS);
  params.webview_src = webview_src;
  params.file_url = file_url;
  params.wants_result = (result_type == JSON_SERIALIZED_RESULT);
  params.user_gesture = user_gesture;
  params.css_origin = css_origin;

  // Generate an injection key if this is a CSS injection from an extension
  // (i.e. tabs.insertCSS).
  if (host_id.type() == HostID::EXTENSIONS && script_type == CSS)
    params.injection_key = GenerateInjectionKey(host_id, file_url, code);

  // Handler handles IPCs and deletes itself on completion.
  new Handler(observer_, web_contents_, params, frame_scope, frame_id,
              callback);
}

}  // namespace extensions
