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
      if (!frame) {
        AddWillNotInjectResult(
            frame_id, base::StringPrintf("No frame with ID: %d", frame_id));
        continue;
      }

      DCHECK(!base::Contains(pending_render_frames_, frame));
      if (!frame->IsRenderFrameLive()) {
        AddWillNotInjectResult(
            frame_id,
            base::StringPrintf("Frame with ID %d is not ready", frame_id));
        continue;
      }

      pending_render_frames_.push_back(frame);
    }

    // If there is a single frame specified (and it was valid), we consider it
    // the "root" frame, which is used in result ordering and error collection.
    if (frame_ids.size() == 1 && pending_render_frames_.size() == 1)
      root_rfh_id_ = frame_ids[0];

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
  // TODO(devlin): Could we just rely on the RenderFrameDeleted() notification?
  // If so, we could remove this.
  void WebContentsDestroyed() override {
    for (content::RenderFrameHost* frame : pending_render_frames_) {
      int frame_id = ExtensionApiFrameIdMap::GetFrameId(frame);
      AddWillNotInjectResult(
          frame_id,
          base::StringPrintf("Tab containing frame with ID %d was removed.",
                             frame_id));
    }
    pending_render_frames_.clear();
    Finish();
  }

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
    int erased_count = base::Erase(pending_render_frames_, render_frame_host);
    DCHECK_LE(erased_count, 1);
    if (erased_count == 0)
      return;

    int frame_id = ExtensionApiFrameIdMap::GetFrameId(render_frame_host);
    AddWillNotInjectResult(
        frame_id,
        base::StringPrintf("Frame with ID %d was removed.", frame_id));
    if (pending_render_frames_.empty())
      Finish();
  }

  void AddWillNotInjectResult(int frame_id, std::string error) {
    ScriptExecutor::FrameResult result;
    result.frame_id = frame_id;
    result.error = std::move(error);
    results_.push_back(std::move(result));
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
                             const base::Optional<base::Value>& result) {
    DCHECK_EQ(request_id_, request_id);
    DCHECK(!pending_render_frames_.empty());
    size_t erased = base::Erase(pending_render_frames_, render_frame_host);
    DCHECK_EQ(1u, erased);

    ScriptExecutor::FrameResult frame_result;
    frame_result.frame_responded = true;
    frame_result.frame_id =
        ExtensionApiFrameIdMap::GetFrameId(render_frame_host);
    frame_result.error = error;
    // TODO(devlin): Do we need to trust the renderer for the URL here? Is there
    // a risk of the frame having navigated since the injection happened?
    frame_result.url = on_url;
    if (result.has_value())
      frame_result.value = result->Clone();

    results_.push_back(std::move(frame_result));

    // Wait until the final request finishes before reporting back.
    if (pending_render_frames_.empty())
      Finish();
  }

  void Finish() {
    DCHECK(pending_render_frames_.empty());
    DCHECK(!results_.empty());

    // TODO(devlin): This would be simpler (and more thorough) if we could just
    // invoke the observer for each frame. Investigate.
    if (observer_ && root_rfh_id_) {
      auto root_frame_result =
          std::find_if(results_.begin(), results_.end(),
                       [root_rfh_id = *root_rfh_id_](const auto& frame_result) {
                         return frame_result.frame_id == root_rfh_id;
                       });
      DCHECK(root_frame_result != results_.end());
      if (root_frame_result->error.empty() &&
          host_id_.type() == HostID::EXTENSIONS) {
        std::move(observer_).Run(web_contents(), {{host_id_.id(), {}}},
                                 root_frame_result->url);
      }
    }

    if (callback_)
      std::move(callback_).Run(std::move(results_));

    delete this;
  }

  ScriptsExecutedOnceCallback observer_;

  // The id of the host (the extension or the webui) doing the injection.
  HostID host_id_;

  // The request id of the injection.
  int request_id_ = 0;

  // The id of the primary frame of the injection, if only a single frame is
  // explicitly specified.
  base::Optional<int> root_rfh_id_;

  // The hosts of the still-running injections. Note: this is a vector because
  // order matters (some tests - and therefore perhaps some extensions - rely on
  // the execution mirroring the frame tree hierarchy). The contents, however,
  // should be unique (i.e., no duplicated frames).
  // TODO(devlin): Extensions *shouldn't* rely on order here, because there's
  // never a guarantee. We should probably just adjust the test and disregard
  // order (except the root frame).
  std::vector<content::RenderFrameHost*> pending_render_frames_;

  // The results of the injection.
  std::vector<ScriptExecutor::FrameResult> results_;

  // The callback to run after all injections complete.
  ScriptExecutor::ScriptFinishedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(Handler);
};

}  // namespace

ScriptExecutor::FrameResult::FrameResult() = default;
ScriptExecutor::FrameResult::FrameResult(FrameResult&&) = default;
ScriptExecutor::FrameResult& ScriptExecutor::FrameResult::operator=(
    FrameResult&&) = default;

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
                                   CSSOrigin css_origin,
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
