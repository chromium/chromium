// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_function.h"
#include "ipc/ipc_sender.h"

struct ExtensionHostMsg_Request_Params;

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
class ExtensionFunctionDispatcher
    : public base::SupportsWeakPtr<ExtensionFunctionDispatcher> {
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

  // Message handlers.
  // The response is sent to the corresponding render view in an
  // ExtensionMsg_Response message.
  void Dispatch(const ExtensionHostMsg_Request_Params& params,
                content::RenderFrameHost* render_frame_host,
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

 private:
  // For a given RenderFrameHost instance, UIThreadResponseCallbackWrapper
  // creates ExtensionFunction::ResponseCallback instances which send responses
  // to the corresponding render view in ExtensionMsg_Response messages.
  // This class tracks the lifespan of the RenderFrameHost instance, and will be
  // destroyed automatically when it goes away.
  class UIThreadResponseCallbackWrapper;

  // Same as UIThreadResponseCallbackWrapper above, but applies to an extension
  // function from an extension Service Worker.
  class UIThreadWorkerResponseCallbackWrapper;

  // Key used to store UIThreadWorkerResponseCallbackWrapper in the map
  // |ui_thread_response_callback_wrappers_for_worker_|.
  struct WorkerResponseCallbackMapKey;

  // Helper to check whether an ExtensionFunction has the required permissions.
  // This should be called after the function is fully initialized.
  // If the check fails, |callback| is run with an access-denied error and false
  // is returned. |function| must not be run in that case.
  static bool CheckPermissions(
      ExtensionFunction* function,
      const ExtensionHostMsg_Request_Params& params,
      const ExtensionFunction::ResponseCallback& callback);

  // Helper to create an ExtensionFunction to handle the function given by
  // |params|. Can be called on any thread.
  // Does not set subclass properties, or include_incognito.
  static scoped_refptr<ExtensionFunction> CreateExtensionFunction(
      const ExtensionHostMsg_Request_Params& params,
      const Extension* extension,
      int requesting_process_id,
      const ProcessMap& process_map,
      ExtensionAPI* api,
      void* profile_id,
      const ExtensionFunction::ResponseCallback& callback);

  // Helper to run the response callback with an access denied error. Can be
  // called on any thread.
  static void SendAccessDenied(
      const ExtensionFunction::ResponseCallback& callback);

  void DispatchWithCallbackInternal(
      const ExtensionHostMsg_Request_Params& params,
      content::RenderFrameHost* render_frame_host,
      int render_process_id,
      const ExtensionFunction::ResponseCallback& callback);

  void RemoveWorkerCallbacksForProcess(int render_process_id);

  content::BrowserContext* browser_context_;

  Delegate* delegate_;

  // This map doesn't own either the keys or the values. When a RenderFrameHost
  // instance goes away, the corresponding entry in this map (if exists) will be
  // removed.
  typedef std::map<content::RenderFrameHost*,
                   std::unique_ptr<UIThreadResponseCallbackWrapper>>
      UIThreadResponseCallbackWrapperMap;
  UIThreadResponseCallbackWrapperMap ui_thread_response_callback_wrappers_;

  using UIThreadWorkerResponseCallbackWrapperMap =
      std::map<WorkerResponseCallbackMapKey,
               std::unique_ptr<UIThreadWorkerResponseCallbackWrapper>>;
  // TODO(lazyboy): The map entries are cleared upon RenderProcessHost shutown,
  // we should really be clearing it on service worker shutdown.
  UIThreadWorkerResponseCallbackWrapperMap
      ui_thread_response_callback_wrappers_for_worker_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_DISPATCHER_H_
