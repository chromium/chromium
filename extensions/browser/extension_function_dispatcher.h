// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "ipc/ipc_sender.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}

namespace extensions {

class Extension;
class ExtensionAPI;
class ProcessMap;
class WindowController;

// ExtensionFunctionDispatcher receives requests to execute functions from
// Chrome extensions running in a RenderFrameHost and dispatches them to the
// appropriate handler. It lives entirely on the UI thread.
//
// ExtensionFunctionDispatcher should be a member of some class that hosts
// RenderFrameHosts and wants them to be able to display extension content.
// This class should also implement ExtensionFunctionDispatcher::Delegate.
//
// Note that a single ExtensionFunctionDispatcher does *not* correspond to a
// single RVH, a single extension, or a single URL. This is by design so that
// we can gracefully handle cases like WebContents, where the RVH, extension,
// and URL can all change over the lifetime of the tab. Instead, these items
// are all passed into each request.
class ExtensionFunctionDispatcher {
 public:
  class Delegate {
   public:
    // Returns the WindowController associated with this delegate, or NULL if no
    // window is associated with the delegate.
    virtual WindowController* GetExtensionWindowController() const;

    // Asks the delegate for any relevant WebContents associated with this
    // context. For example, the WebContents in which an infobar or
    // chrome-extension://<id> URL are being shown. Callers must check for a
    // NULL return value (as in the case of a background page).
    virtual content::WebContents* GetAssociatedWebContents() const;

    // If the associated web contents is not null, returns that. Otherwise,
    // returns the next most relevant visible web contents. Callers must check
    // for a NULL return value (as in the case of a background page).
    virtual content::WebContents* GetVisibleWebContents() const;

   protected:
    virtual ~Delegate() {}
  };

  // Public constructor. Callers must ensure that:
  // - This object outlives any RenderFrameHost's passed to created
  //   ExtensionFunctions.
  explicit ExtensionFunctionDispatcher(
      content::BrowserContext* browser_context);
  ~ExtensionFunctionDispatcher();

  // Dispatches a request and the response is sent in |callback| that is a reply
  // of mojom::LocalFrameHost::Request.
  void Dispatch(mojom::RequestParamsPtr params,
                content::RenderFrameHost& frame,
                mojom::LocalFrameHost::RequestCallback callback);

  // Message handlers.
  // Dispatches a request for service woker and the response is sent to the
  // corresponding render process in an ExtensionMsg_ResponseWorker message.
  void DispatchForServiceWorker(const mojom::RequestParams& params,
                                int render_process_id);

  // Called when an ExtensionFunction is done executing, after it has sent
  // a response (if any) to the extension.
  void OnExtensionFunctionCompleted(const Extension* extension,
                                    bool is_from_service_worker,
                                    const char* name);

  // See the Delegate class for documentation on these methods.
  // TODO(devlin): None of these belong here. We should kill
  // ExtensionFunctionDispatcher::Delegate.
  WindowController* GetExtensionWindowController() const;
  content::WebContents* GetAssociatedWebContents() const;
  content::WebContents* GetVisibleWebContents() const;

  // The BrowserContext that this dispatcher is associated with.
  content::BrowserContext* browser_context() { return browser_context_; }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Adds a function object to the set of objects waiting for
  // responses from the renderer.
  void AddWorkerResponseTarget(ExtensionFunction* func);

  // Processes a Service Worker response from a renderer.
  void ProcessServiceWorkerResponse(int request_id,
                                    int64_t service_worker_version_id);

  base::WeakPtr<ExtensionFunctionDispatcher> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // For a given RenderFrameHost instance, ResponseCallbackWrapper
  // creates ExtensionFunction::ResponseCallback instances which send responses
  // to the corresponding render view in ExtensionMsg_Response messages.
  // This class tracks the lifespan of the RenderFrameHost instance, and will be
  // destroyed automatically when it goes away.
  class ResponseCallbackWrapper;

  // Same as ResponseCallbackWrapper above, but applies to an extension
  // function from an extension Service Worker.
  class WorkerResponseCallbackWrapper;

  // Key used to store WorkerResponseCallbackWrapper in the map
  // |response_callback_wrappers_for_worker_|.
  struct WorkerResponseCallbackMapKey;

  // Helper to create an ExtensionFunction to handle the function given by
  // |params|. Can be called on any thread.
  // Does not set subclass properties, or include_incognito.
  static scoped_refptr<ExtensionFunction> CreateExtensionFunction(
      const mojom::RequestParams& params,
      const Extension* extension,
      int requesting_process_id,
      bool is_worker_request,
      const GURL* rfh_url,
      const ProcessMap& process_map,
      ExtensionAPI* api,
      ExtensionFunction::ResponseCallback callback,
      content::RenderFrameHost* render_frame_host);

  void DispatchWithCallbackInternal(
      const mojom::RequestParams& params,
      content::RenderFrameHost* render_frame_host,
      int render_process_id,
      ExtensionFunction::ResponseCallback callback);

  void RemoveWorkerCallbacksForProcess(int render_process_id);

  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;

  raw_ptr<Delegate, DanglingUntriaged> delegate_;

  // This map doesn't own either the keys or the values. When a RenderFrameHost
  // instance goes away, the corresponding entry in this map (if exists) will be
  // removed.
  typedef std::map<content::RenderFrameHost*,
                   std::unique_ptr<ResponseCallbackWrapper>>
      ResponseCallbackWrapperMap;
  ResponseCallbackWrapperMap response_callback_wrappers_;

  using WorkerResponseCallbackWrapperMap =
      std::map<WorkerResponseCallbackMapKey,
               std::unique_ptr<WorkerResponseCallbackWrapper>>;
  // TODO(lazyboy): The map entries are cleared upon RenderProcessHost shutown,
  // we should really be clearing it on service worker shutdown.
  WorkerResponseCallbackWrapperMap response_callback_wrappers_for_worker_;

  // The set of ExtensionFunction instances waiting for responses from
  // the renderer. These are removed once the response is processed.
  // The lifetimes of the instances are managed by the instances themselves.
  std::set<ExtensionFunction*> worker_response_targets_;

  base::WeakPtrFactory<ExtensionFunctionDispatcher> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_
