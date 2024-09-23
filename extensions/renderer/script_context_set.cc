// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context_set.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/worker_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/utils/extension_utils.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/isolated_world_manager.h"
#include "extensions/renderer/renderer_frame_context_data.h"
#include "extensions/renderer/script_context.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"

namespace extensions {

namespace {
// There is only ever one instance of the ScriptContextSet.
ScriptContextSet* g_context_set = nullptr;
}

ScriptContextSet::ScriptContextSet(ExtensionIdSet* active_extension_ids)
    : active_extension_ids_(active_extension_ids) {
  DCHECK(!g_context_set);
  g_context_set = this;
}

ScriptContextSet::~ScriptContextSet() {
  g_context_set = nullptr;
}

ScriptContext* ScriptContextSet::Register(
    blink::WebLocalFrame* frame,
    const v8::Local<v8::Context>& v8_context,
    int32_t world_id,
    bool is_webview) {
  const Extension* extension =
      GetExtensionFromFrameAndWorld(frame, world_id, false);
  const Extension* effective_extension =
      GetExtensionFromFrameAndWorld(frame, world_id, true);

  mojom::ViewType view_type = mojom::ViewType::kInvalid;
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(frame);
  // In production, we should always have a corresponding render frame.
  // Unfortunately, this isn't the case in unit tests, so we can't DCHECK here.
  if (render_frame) {
    ExtensionFrameHelper* frame_helper =
        ExtensionFrameHelper::Get(render_frame);
    DCHECK(frame_helper);
    view_type = frame_helper->view_type();
  }
  GURL frame_url = ScriptContext::GetDocumentLoaderURLForFrame(frame);
  mojom::ContextType context_type = ClassifyJavaScriptContext(
      extension, world_id, frame_url, frame->GetDocument().GetSecurityOrigin(),
      view_type, is_webview);
  mojom::ContextType effective_context_type = ClassifyJavaScriptContext(
      effective_extension, world_id,
      ScriptContext::GetEffectiveDocumentURLForContext(frame, frame_url, true),
      frame->GetDocument().GetSecurityOrigin(), view_type, is_webview);

  mojom::HostID host_id;
  RendererFrameContextData context_data = RendererFrameContextData(frame);
  // By default, the context will use a HostID kExtensions type. Specific
  // cases override that value below.
  host_id.type = mojom::HostID::HostType::kExtensions;
  if (extension) {
    host_id.id = extension->id();
  } else if (effective_context_type == mojom::ContextType::kWebUi) {
    host_id.type = mojom::HostID::HostType::kWebUi;
  } else if (effective_context_type == mojom::ContextType::kWebPage &&
             !is_webview && context_data.HasControlledFrameCapability()) {
    host_id.type = mojom::HostID::HostType::kControlledFrameEmbedder;
    // TODO(crbug.com/41490370): Improve how we derive origin for controlled
    // frame embedders in renderer.
    host_id.id = "";
    if (frame_url.has_scheme()) {
      host_id.id += frame_url.scheme() + "://";
    }
    host_id.id += frame_url.host();
    if (frame_url.has_port()) {
      host_id.id += ":" + frame_url.port();
    }
  }

  std::optional<int> blink_isolated_world_id;
  if (IsolatedWorldManager::IsExtensionIsolatedWorld(world_id)) {
    blink_isolated_world_id = world_id;
  }

  ScriptContext* context = new ScriptContext(
      v8_context, frame, host_id, extension, std::move(blink_isolated_world_id),
      context_type, effective_extension, effective_context_type);
  contexts_.insert(context);  // takes ownership
  return context;
}

void ScriptContextSet::Remove(ScriptContext* context) {
  if (contexts_.erase(context)) {
    context->Invalidate();
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  context);
  }
}

ScriptContext* ScriptContextSet::GetCurrent() const {
  v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
  if (!isolate) [[unlikely]] {
    return nullptr;
  }
  return isolate->InContext() ? GetByV8Context(isolate->GetCurrentContext())
                              : nullptr;
}

ScriptContext* ScriptContextSet::GetByV8Context(
    const v8::Local<v8::Context>& v8_context) const {
  for (ScriptContext* script_context : contexts_) {
    if (script_context->v8_context() == v8_context)
      return script_context;
  }
  return nullptr;
}

ScriptContext* ScriptContextSet::GetContextByObject(
    const v8::Local<v8::Object>& object) {
  v8::Local<v8::Context> context;
  if (!object->GetCreationContext().ToLocal(&context))
    return nullptr;
  return GetContextByV8Context(context);
}

ScriptContext* ScriptContextSet::GetContextByV8Context(
    const v8::Local<v8::Context>& v8_context) {
  // g_context_set can be null in unittests.
  return g_context_set ? g_context_set->GetByV8Context(v8_context) : nullptr;
}

ScriptContext* ScriptContextSet::GetMainWorldContextForFrame(
    content::RenderFrame* render_frame) {
  blink::WebLocalFrame* web_frame = render_frame->GetWebFrame();
  v8::HandleScope handle_scope(web_frame->GetAgentGroupScheduler()->Isolate());
  return GetContextByV8Context(web_frame->MainWorldScriptContext());
}

void ScriptContextSet::ForEach(
    const mojom::HostID& host_id,
    content::RenderFrame* render_frame,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  // We copy the context list, because calling into javascript may modify it
  // out from under us.
  std::set<raw_ptr<ScriptContext, SetExperimental>> contexts_copy = contexts_;

  for (ScriptContext* context : contexts_copy) {
    // For the same reason as above, contexts may become invalid while we run.
    if (!context->is_valid()) {
      continue;
    }

    switch (host_id.type) {
      case mojom::HostID::HostType::kExtensions:
        // Note: If the type is kExtensions and host_id.id is empty, then the
        // call should affect all extensions. See comment in dispatcher.cc
        // UpdateAllBindings().
        if (host_id.id.empty() || context->GetExtensionID() == host_id.id) {
          ExecuteCallbackWithContext(context, render_frame, callback);
        }
        break;

      case mojom::HostID::HostType::kWebUi:
        DCHECK(host_id.id.empty());
        ExecuteCallbackWithContext(context, render_frame, callback);
        break;

      case mojom::HostID::HostType::kControlledFrameEmbedder:
        DCHECK(!host_id.id.empty());
        // Verify that host_id matches context->host_id.
        if (context->host_id().type == host_id.type &&
            context->host_id().id == host_id.id) {
          ExecuteCallbackWithContext(context, render_frame, callback);
        }
    }
  }
}

void ScriptContextSet::ExecuteCallbackWithContext(
    ScriptContext* context,
    content::RenderFrame* render_frame,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  CHECK(context);
  content::RenderFrame* context_render_frame = context->GetRenderFrame();
  if (!render_frame || render_frame == context_render_frame) {
    callback.Run(context);
  }
}

void ScriptContextSet::OnExtensionUnloaded(const ExtensionId& extension_id) {
  ScriptContextSetIterable::ForEach(
      GenerateHostIdFromExtensionId(extension_id),
      base::BindRepeating(&ScriptContextSet::Remove, base::Unretained(this)));
}

void ScriptContextSet::AddForTesting(std::unique_ptr<ScriptContext> context) {
  contexts_.insert(context.release());  // Takes ownership
}

const Extension* ScriptContextSet::GetExtensionFromFrameAndWorld(
    blink::WebLocalFrame* frame,
    int32_t world_id,
    bool use_effective_url) {
  ExtensionId extension_id;
  if (world_id != 0) {
    // Isolated worlds (content script).
    extension_id =
        IsolatedWorldManager::GetInstance().GetHostIdForIsolatedWorld(world_id);
  } else {
    // For looking up the extension associated with this frame, we either want
    // to use the current url or possibly the data source url (which this frame
    // may be navigating to shortly), depending on the security origin of the
    // frame. We don't always want to use the data source url because some
    // frames (eg iframes and windows created via window.open) briefly contain
    // an about:blank script context that is scriptable by their parent/opener
    // before they finish navigating.
    GURL frame_url = ScriptContext::GetAccessCheckedFrameURL(frame);
    frame_url = ScriptContext::GetEffectiveDocumentURLForContext(
        frame, frame_url, use_effective_url);
    extension_id =
        RendererExtensionRegistry::Get()->GetExtensionOrAppIDByURL(frame_url);
  }

  // There are conditions where despite a context being associated with an
  // extension, no extension actually gets found. Ignore "invalid" because CSP
  // blocks extension page loading by switching the extension ID to "invalid".
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension && !extension_id.empty() && extension_id != "invalid") {
    // TODO(kalman): Do something here?
  }
  return extension;
}

mojom::ContextType ScriptContextSet::ClassifyJavaScriptContext(
    const Extension* extension,
    int32_t world_id,
    const GURL& url,
    const blink::WebSecurityOrigin& origin,
    mojom::ViewType view_type,
    bool is_webview) {
  // WARNING: This logic must match `ProcessMap::GetMostLikelyContextType()`
  // as much as possible.

  // Worlds not within this range are not for content scripts, so ignore them.
  // TODO(devlin): Isolated worlds with a non-zero id could belong to
  // chrome-internal pieces, like dom distiller and translate. Do we need any
  // bindings (even those for basic web pages) for those?
  if (world_id >= ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId()) {
    // A non-main-world should (currently) only ever happen on the main thread.
    // We don't support injection of content scripts or user scripts into worker
    // contexts.
    CHECK_EQ(kMainThreadId, content::WorkerThread::GetCurrentId());
    std::optional<mojom::ExecutionWorld> execution_world =
        IsolatedWorldManager::GetInstance().GetExecutionWorldForIsolatedWorld(
            world_id);
    if (execution_world == mojom::ExecutionWorld::kUserScript) {
      CHECK(extension);
      return mojom::ContextType::kUserScript;
    }

    return extension ?  // TODO(kalman): when does this happen?
               mojom::ContextType::kContentScript
                     : mojom::ContextType::kUnspecified;
  }

  // We have an explicit check for sandboxed pages before checking whether the
  // extension is active in this process because:
  // 1. Sandboxed extension pages which are not listed in the extension's
  //    manifest sandbox section run in the same process as regular extension
  //    pages, so the extension is considered active. (In contrast,
  //    manifest-sandboxed pages run in a different process because they do not
  //    have API access.)
  // 2. ScriptContext creation (which triggers bindings injection) happens
  //    before the SecurityContext is updated with the sandbox flags (after
  //    reading the CSP header), so the caller can't check if the context's
  //    security origin is unique yet.
  if (ScriptContext::IsSandboxedPage(url)) {
    // TODO(https://crbug.com/347031402): it's weird returning kWebPage if
    // `extension` is non-null (which it is if `IsSandboxedPage` returns true).
    // It would be better to return kUnprivileged in that case.
    return mojom::ContextType::kWebPage;
  }

  if (extension && active_extension_ids_->count(extension->id()) > 0) {
    // |extension| is active in this process, but it could be either a true
    // extension process or within the extent of a hosted app. In the latter
    // case this would usually be considered a (privileged) web page context,
    // unless the extension in question is a component extension, in which case
    // we cheat and call it privileged.
    if (extension->is_hosted_app() &&
        extension->location() != mojom::ManifestLocation::kComponent) {
      return mojom::ContextType::kPrivilegedWebPage;
    }

    if (is_lock_screen_context_) {
      return mojom::ContextType::kLockscreenExtension;
    }

    if (is_webview) {
#if BUILDFLAG(ENABLE_PDF)
      // The PDF Viewer extension in a webview needs to be a privileged
      // extension in order to load.
      if (extension->id() == extension_misc::kPdfExtensionId) {
        return mojom::ContextType::kPrivilegedExtension;
      }
#endif  // BUILDFLAG(ENABLE_PDF)

      return mojom::ContextType::kUnprivilegedExtension;
    }

    if (view_type == mojom::ViewType::kOffscreenDocument) {
      return mojom::ContextType::kOffscreenExtension;
    }

    return mojom::ContextType::kPrivilegedExtension;
  }

  // None of the following feature types should ever be present in an
  // offscreen document.
  DCHECK_NE(mojom::ViewType::kOffscreenDocument, view_type);

  // TODO(kalman): This IsOpaque() check is wrong, it should be performed as
  // part of ScriptContext::IsSandboxedPage().
  if (!origin.IsOpaque() &&
      RendererExtensionRegistry::Get()->ExtensionBindingsAllowed(url)) {
    if (!extension)  // TODO(kalman): when does this happen?
      return mojom::ContextType::kUnspecified;
    return extension->is_hosted_app()
               ? mojom::ContextType::kPrivilegedWebPage
               : mojom::ContextType::kUnprivilegedExtension;
  }

  if (!url.is_valid()) {
    return mojom::ContextType::kUnspecified;
  }

  if (url.SchemeIs(content::kChromeUIScheme)) {
    return mojom::ContextType::kWebUi;
  }

  if (url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return mojom::ContextType::kUntrustedWebUi;
  }

  return mojom::ContextType::kWebPage;
}

}  // namespace extensions
