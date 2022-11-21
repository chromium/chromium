// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context_set.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_injection.h"
#include "third_party/blink/public/web/blink.h"
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
    int32_t world_id) {
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
    // We should only find an offscreen document if the corresponding feature
    // is enabled.
    DCHECK(base::FeatureList::IsEnabled(
               extensions_features::kExtensionsOffscreenDocuments) ||
           view_type != mojom::ViewType::kOffscreenDocument);
  }
  GURL frame_url = ScriptContext::GetDocumentLoaderURLForFrame(frame);
  Feature::Context context_type = ClassifyJavaScriptContext(
      extension, world_id, frame_url, frame->GetDocument().GetSecurityOrigin(),
      view_type);
  Feature::Context effective_context_type = ClassifyJavaScriptContext(
      effective_extension, world_id,
      ScriptContext::GetEffectiveDocumentURLForContext(frame, frame_url, true),
      frame->GetDocument().GetSecurityOrigin(), view_type);

  ScriptContext* context =
      new ScriptContext(v8_context, frame, extension, context_type,
                        effective_extension, effective_context_type);
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
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
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
  v8::HandleScope handle_scope(blink::MainThreadIsolate());
  return GetContextByV8Context(
      render_frame->GetWebFrame()->MainWorldScriptContext());
}

void ScriptContextSet::ForEach(
    const std::string& extension_id,
    content::RenderFrame* render_frame,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  // We copy the context list, because calling into javascript may modify it
  // out from under us.
  std::set<ScriptContext*> contexts_copy = contexts_;

  for (ScriptContext* context : contexts_copy) {
    // For the same reason as above, contexts may become invalid while we run.
    if (!context->is_valid())
      continue;

    if (!extension_id.empty()) {
      const Extension* extension = context->extension();
      if (!extension || (extension_id != extension->id()))
        continue;
    }

    content::RenderFrame* context_render_frame = context->GetRenderFrame();
    if (render_frame && render_frame != context_render_frame)
      continue;

    callback.Run(context);
  }
}

void ScriptContextSet::OnExtensionUnloaded(const std::string& extension_id) {
  ScriptContextSetIterable::ForEach(
      extension_id,
      base::BindRepeating(&ScriptContextSet::Remove, base::Unretained(this)));
}

void ScriptContextSet::AddForTesting(std::unique_ptr<ScriptContext> context) {
  contexts_.insert(context.release());  // Takes ownership
}

const Extension* ScriptContextSet::GetExtensionFromFrameAndWorld(
    blink::WebLocalFrame* frame,
    int32_t world_id,
    bool use_effective_url) {
  std::string extension_id;
  if (world_id != 0) {
    // Isolated worlds (content script).
    extension_id = ScriptInjection::GetHostIdForIsolatedWorld(world_id);
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

Feature::Context ScriptContextSet::ClassifyJavaScriptContext(
    const Extension* extension,
    int32_t world_id,
    const GURL& url,
    const blink::WebSecurityOrigin& origin,
    mojom::ViewType view_type) {
  // WARNING: This logic must match ProcessMap::GetContextType, as much as
  // possible.

  // Worlds not within this range are not for content scripts, so ignore them.
  // TODO(devlin): Isolated worlds with a non-zero id could belong to
  // chrome-internal pieces, like dom distiller and translate. Do we need any
  // bindings (even those for basic web pages) for those?
  if (world_id >= ExtensionsRendererClient::Get()->GetLowestIsolatedWorldId()) {
    return extension ?  // TODO(kalman): when does this happen?
               Feature::CONTENT_SCRIPT_CONTEXT
                     : Feature::UNSPECIFIED_CONTEXT;
  }

  // We have an explicit check for sandboxed pages before checking whether the
  // extension is active in this process because:
  // 1. Sandboxed pages run in the same process as regular extension pages, so
  //    the extension is considered active.
  // 2. ScriptContext creation (which triggers bindings injection) happens
  //    before the SecurityContext is updated with the sandbox flags (after
  //    reading the CSP header), so the caller can't check if the context's
  //    security origin is unique yet.
  if (ScriptContext::IsSandboxedPage(url))
    return Feature::WEB_PAGE_CONTEXT;

  if (extension && active_extension_ids_->count(extension->id()) > 0) {
    // |extension| is active in this process, but it could be either a true
    // extension process or within the extent of a hosted app. In the latter
    // case this would usually be considered a (blessed) web page context,
    // unless the extension in question is a component extension, in which case
    // we cheat and call it blessed.
    if (extension->is_hosted_app() &&
        extension->location() != mojom::ManifestLocation::kComponent) {
      return Feature::BLESSED_WEB_PAGE_CONTEXT;
    }

    if (is_lock_screen_context_)
      return Feature::LOCK_SCREEN_EXTENSION_CONTEXT;
    if (view_type == mojom::ViewType::kOffscreenDocument)
      return Feature::OFFSCREEN_EXTENSION_CONTEXT;
    return Feature::BLESSED_EXTENSION_CONTEXT;
  }

  // None of the following feature types should ever be present in an
  // offscreen document.
  DCHECK_NE(mojom::ViewType::kOffscreenDocument, view_type);

  // TODO(kalman): This IsOpaque() check is wrong, it should be performed as
  // part of ScriptContext::IsSandboxedPage().
  if (!origin.IsOpaque() &&
      RendererExtensionRegistry::Get()->ExtensionBindingsAllowed(url)) {
    if (!extension)  // TODO(kalman): when does this happen?
      return Feature::UNSPECIFIED_CONTEXT;
    return extension->is_hosted_app() ? Feature::BLESSED_WEB_PAGE_CONTEXT
                                      : Feature::UNBLESSED_EXTENSION_CONTEXT;
  }

  if (!url.is_valid())
    return Feature::UNSPECIFIED_CONTEXT;

  if (url.SchemeIs(content::kChromeUIScheme))
    return Feature::WEBUI_CONTEXT;

  if (url.SchemeIs(content::kChromeUIUntrustedScheme))
    return Feature::WEBUI_UNTRUSTED_CONTEXT;

  return Feature::WEB_PAGE_CONTEXT;
}

}  // namespace extensions
