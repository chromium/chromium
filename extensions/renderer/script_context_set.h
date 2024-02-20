// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_
#define EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context_set_iterable.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"
class GURL;

namespace blink {
class WebLocalFrame;
class WebSecurityOrigin;
}

namespace content {
class RenderFrame;
}

namespace extensions {
class Extension;
class ScriptContext;

// A container of ScriptContexts, responsible for both creating and managing
// them.
//
// Since calling JavaScript within a context can cause any number of contexts
// to be created or destroyed, this has additional smarts to help with the set
// changing underneath callers.
class ScriptContextSet : public ScriptContextSetIterable {
 public:
  explicit ScriptContextSet(
      // Set of the IDs of extensions that are active in this process.
      // Must outlive this. TODO(kalman): Combine this and |extensions|.
      ExtensionIdSet* active_extension_ids);

  ScriptContextSet(const ScriptContextSet&) = delete;
  ScriptContextSet& operator=(const ScriptContextSet&) = delete;

  ~ScriptContextSet() override;

  // Returns the number of contexts being tracked by this set.
  // This may also include invalid contexts. TODO(kalman): Useful?
  size_t size() const { return contexts_.size(); }

  // Creates and starts managing a new ScriptContext. Ownership is held.
  // Returns a weak reference to the new ScriptContext.
  ScriptContext* Register(blink::WebLocalFrame* frame,
                          const v8::Local<v8::Context>& v8_context,
                          int32_t world_id,
                          bool is_webview);

  // If the specified context is contained in this set, remove it, then delete
  // it asynchronously. After this call returns the context object will still
  // be valid, but its frame() pointer will be cleared.
  void Remove(ScriptContext* context);

  // Gets the ScriptContext corresponding to v8::Context::GetCurrent(), or
  // NULL if no such context exists.
  ScriptContext* GetCurrent() const;

  // Gets the ScriptContext corresponding to the specified
  // v8::Context or NULL if no such context exists.
  ScriptContext* GetByV8Context(const v8::Local<v8::Context>& context) const;
  // Static equivalent of the above.
  static ScriptContext* GetContextByV8Context(
      const v8::Local<v8::Context>& context);

  // Returns the ScriptContext corresponding to the V8 context that created the
  // given |object|.
  // Note: The provided |object| may belong to a v8::Context in another frame,
  // as can happen when a parent frame uses an object of an embedded iframe.
  // In this case, there may be no associated ScriptContext, since the child
  // frame can be hosted in another process. Thus, callers of this need to
  // null-check the result (and should also always check whether or not the
  // context has access to the other context).
  static ScriptContext* GetContextByObject(const v8::Local<v8::Object>& object);

  // Returns the ScriptContext corresponding to the main world of the
  // |render_frame|.
  static ScriptContext* GetMainWorldContextForFrame(
      content::RenderFrame* render_frame);

  // ScriptContextSetIterable:
  void ForEach(
      const mojom::HostID& host_id,
      content::RenderFrame* render_frame,
      const base::RepeatingCallback<void(ScriptContext*)>& callback) override;

  // Runs |callback| after verifying |render_frame| matches context's.
  void ExecuteCallbackWithContext(
      ScriptContext* context,
      content::RenderFrame* render_frame,
      const base::RepeatingCallback<void(ScriptContext*)>& callback);

  // Cleans up contexts belonging to an unloaded extension.
  void OnExtensionUnloaded(const ExtensionId& extension_id);

  void set_is_lock_screen_context(bool is_lock_screen_context) {
    is_lock_screen_context_ = is_lock_screen_context;
  }

  // Adds the given |context| for testing purposes.
  void AddForTesting(std::unique_ptr<ScriptContext> context);

 private:
  // Finds the extension for the JavaScript context associated with the
  // specified |frame| and isolated world. If |world_id| is zero, finds the
  // extension ID associated with the main world's JavaScript context. If the
  // JavaScript context isn't from an extension, returns empty string.
  const Extension* GetExtensionFromFrameAndWorld(blink::WebLocalFrame* frame,
                                                 int32_t world_id,
                                                 bool use_effective_url);

  // Returns the mojom::ContextType type of context for a JavaScript context.
  mojom::ContextType ClassifyJavaScriptContext(
      const Extension* extension,
      int32_t world_id,
      const GURL& url,
      const blink::WebSecurityOrigin& origin,
      mojom::ViewType view_type,
      bool is_webview);

  // Weak reference to all installed Extensions that are also active in this
  // process.
  raw_ptr<ExtensionIdSet> active_extension_ids_;

  // The set of all ScriptContexts we own.
  std::set<raw_ptr<ScriptContext, SetExperimental>> contexts_;

  // Whether the script context set is associated with the renderer active on
  // the Chrome OS lock screen.
  bool is_lock_screen_context_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_H_
