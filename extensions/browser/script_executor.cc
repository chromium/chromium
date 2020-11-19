// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/script_executor.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/hash/hash.h"
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
// <host_id> is the host ID, and <digest> is an unspecified hash digest of the
// file URL or the code string, respectively.
const std::string GenerateInjectionKey(const HostID& host_id,
                                       const GURL& script_url,
                                       const std::string& code) {
  const std::string& source = script_url.is_valid() ? script_url.spec() : code;
  return base::StringPrintf("%c%s%zu", script_url.is_valid() ? 'F' : 'C',
                            host_id.id().c_str(), base::FastHash(source));
}

// A handler for a single injection request. On creation this will send the
// injection request to the renderer, and it will be destroyed after either the
// corresponding response comes from the renderer, or the renderer is destroyed.
class Handler : public content::WebContentsObserver {
 public:
  // OnceCallback version of ScriptExecutor::ScriptsExecutedNotification:
  using ScriptsExecutedOnceCallback = base::OnceCallback<
      void(content::WebContents*, const ExecutingScriptsMap&, const GURL&)>;

  Handler(ScriptsExecutedOnceCallback observer,
          content::WebContents* web_contents,
          const ExtensionMsg_ExecuteCode_Params& params,
          ScriptExecutor::FrameScope scope,
          const std::vector<int>& frame_ids,
          ScriptExecutor::ScriptFinishedCallback callback)
      : content::WebContentsObserver(web_contents),
        observer_(std::move(observer)),
        host_id_(params.host_id),
        request_id_(params.request_id),
        callback_(std::move(callback)) {
    for (int frame_id : frame_ids) {
      content::RenderFrameHost* frame =
          ExtensionApiFrameIdMap::GetRenderFrameHostById(web_contents,
                                                         frame_id);
      if (!frame)
        continue;

      DCHECK(!base::Contains(pending_render_frames_, frame));
      if (frame->IsRenderFrameLive())
        pending_render_frames_.push_back(frame);
    }

    // If there is a single frame specified (and it was valid), we consider it
    // the "root" frame, which is used in result ordering and error collection.
    if (frame_ids.size() == 1 && pending_render_frames_.size() == 1) {
      root_rfh_ = pending_render_frames_[0];
      root_is_main_frame_ = root_rfh_->GetParent() == nullptr;
    }

    // If we are to include subframes, iterate over all frames in the
    // WebContents and add them iff they are a child of an included frame.
    if (scope == ScriptExecutor::INCLUDE_SUB_FRAMES) {
      auto check_frame =
          [](std::vector<content::RenderFrameHost*>* pending_frames,
             content::RenderFrameHost* frame) {
            if (!frame->IsRenderFrameLive() ||
                base::Contains(*pending_frames, frame)) {
              return;
            }

            for (auto* pending_frame : *pending_frames) {
              if (frame->IsDescendantOf(pending_frame)) {
                pending_frames->push_back(frame);
                break;
              }
            }
          };
      web_contents->ForEachFrame(
          base::BindRepeating(check_frame, &pending_render_frames_));
    }

    for (content::RenderFrameHost* frame : pending_render_frames_)
      SendExecuteCode(params, frame);

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
    if (base::Erase(pending_render_frames_, render_frame_host) == 1 &&
        pending_render_frames_.empty()) {
      Finish();
    }
  }

  // Sends an ExecuteCode message to the given frame host, and increments
  // the number of pending messages.
  void SendExecuteCode(const ExtensionMsg_ExecuteCode_Params& params,
                       content::RenderFrameHost* frame) {
    DCHECK(frame->IsRenderFrameLive());
    DCHECK(base::Contains(pending_render_frames_, frame));
    URLLoaderFactoryManager::WillExecuteCode(frame, host_id_);
    frame->Send(new ExtensionMsg_ExecuteCode(frame->GetRoutingID(), params));
  }

  // Handles the ExecuteCodeFinished message.
  void OnExecuteCodeFinished(content::RenderFrameHost* render_frame_host,
                             int request_id,
                             const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& result_list) {
    DCHECK_EQ(request_id_, request_id);
    DCHECK(!pending_render_frames_.empty());
    size_t erased = base::Erase(pending_render_frames_, render_frame_host);
    DCHECK_EQ(1u, erased);
    bool is_root_frame = root_rfh_ == render_frame_host;
    finished_any_execution_ = true;

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
    if (root_rfh_ && root_frame_url_.is_empty()) {
      // We never finished the root frame injection.
      root_frame_error_ =
          root_is_main_frame_ ? kRendererDestroyed : kFrameRemoved;
      results_.Clear();
    } else if (!finished_any_execution_) {
      // We never executed in any frame.
      root_frame_error_ = kFrameRemoved;
    }

    if (observer_ && root_frame_error_.empty() &&
        host_id_.type() == HostID::EXTENSIONS) {
      std::move(observer_).Run(web_contents(), {{host_id_.id(), {}}},
                               root_frame_url_);
    }

    if (callback_)
      std::move(callback_).Run(root_frame_error_, root_frame_url_, results_);
    delete this;
  }

  ScriptsExecutedOnceCallback observer_;

  // The id of the host (the extension or the webui) doing the injection.
  HostID host_id_;

  // The request id of the injection.
  int request_id_ = 0;

  // The primary frame of the injection, if only a single frame is explicitly
  // specified.
  content::RenderFrameHost* root_rfh_ = nullptr;

  // Whether |root_rfh_| is the main frame of a tab.
  bool root_is_main_frame_ = false;

  // Whether execution has finished in any frame.
  bool finished_any_execution_ = false;

  // The hosts of the still-running injections. Note: this is a vector because
  // order matters (some tests - and therefore perhaps some extensions - rely on
  // the execution mirroring the frame tree hierarchy). The contents, however,
  // should be unique (i.e., no duplicated frames).
  // TODO(devlin): Extensions *shouldn't* rely on order here, because there's
  // never a guarantee. We should probably just adjust the test and disregard
  // order (except the root frame).
  std::vector<content::RenderFrameHost*> pending_render_frames_;

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
                                   UserScript::ActionType action_type,
                                   const std::string& code,
                                   ScriptExecutor::FrameScope frame_scope,
                                   const std::vector<int>& frame_ids,
                                   ScriptExecutor::MatchAboutBlank about_blank,
                                   UserScript::RunLocation run_at,
                                   ScriptExecutor::ProcessType process_type,
                                   const GURL& webview_src,
                                   const GURL& script_url,
                                   bool user_gesture,
                                   base::Optional<CSSOrigin> css_origin,
                                   ScriptExecutor::ResultType result_type,
                                   ScriptFinishedCallback callback) {
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
  params.action_type = action_type;
  params.code = code;
  params.match_about_blank = (about_blank == MATCH_ABOUT_BLANK);
  params.run_at = run_at;
  params.is_web_view = (process_type == WEB_VIEW_PROCESS);
  params.webview_src = webview_src;
  params.script_url = script_url;
  params.wants_result = (result_type == JSON_SERIALIZED_RESULT);
  params.user_gesture = user_gesture;
  params.css_origin = css_origin;

  // Generate the unique key that represents this CSS injection or removal
  // from an extension (i.e. tabs.insertCSS or tabs.removeCSS).
  if (host_id.type() == HostID::EXTENSIONS &&
      (action_type == UserScript::ADD_CSS ||
       action_type == UserScript::REMOVE_CSS))
    params.injection_key = GenerateInjectionKey(host_id, script_url, code);

  // Handler handles IPCs and deletes itself on completion.
  new Handler(observer_, web_contents_, params, frame_scope, frame_ids,
              std::move(callback));
}

}  // namespace extensions
