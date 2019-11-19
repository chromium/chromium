// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "components/guest_view/browser/guest_view.h"
#include "extensions/browser/guest_view/app_view/app_view_guest_delegate.h"
#include "extensions/browser/lazy_context_task_queue.h"

namespace extensions {
class Extension;

// An AppViewGuest provides the browser-side implementation of <appview> API.
// AppViewGuest is created on attachment. That is, when a guest WebContents is
// associated with a particular embedder WebContents. This happens on calls to
// the connect API.
class AppViewGuest : public guest_view::GuestView<AppViewGuest> {
 public:
  static const char Type[];

  // Completes the creation of a WebContents associated with the provided
  // |guest_extension_id| and |guest_instance_id| for the given
  // |browser_context|.
  // |guest_render_process_host| is the RenderProcessHost and |url| is the
  // resource GURL of the extension instance making this request. If there is
  // any mismatch between the expected |guest_instance_id| and
  // |guest_extension_id| provided and the recorded copies from when the the
  // <appview> was created, the RenderProcessHost of the extension instance
  // behind this request will be killed.
  static bool CompletePendingRequest(
      content::BrowserContext* browser_context,
      const GURL& url,
      int guest_instance_id,
      const std::string& guest_extension_id,
      content::RenderProcessHost* guest_render_process_host);

  static GuestViewBase* Create(content::WebContents* owner_web_contents);

  static std::vector<int> GetAllRegisteredInstanceIdsForTesting();

  // Sets the AppDelegate for this guest.
  void SetAppDelegateForTest(AppDelegate* delegate);

 private:
  explicit AppViewGuest(content::WebContents* owner_web_contents);

  ~AppViewGuest() override;

  // GuestViewBase implementation.
  void CreateWebContents(const base::DictionaryValue& create_params,
                         WebContentsCreatedCallback callback) final;
  void DidInitialize(const base::DictionaryValue& create_params) final;
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;

  // content::WebContentsDelegate implementation.
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) final;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) final;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) final;

  void CompleteCreateWebContents(const GURL& url,
                                 const Extension* guest_extension,
                                 WebContentsCreatedCallback callback);

  void LaunchAppAndFireEvent(
      std::unique_ptr<base::DictionaryValue> data,
      WebContentsCreatedCallback callback,
      std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info);

  GURL url_;
  std::string guest_extension_id_;
  std::unique_ptr<AppViewGuestDelegate> app_view_guest_delegate_;
  std::unique_ptr<AppDelegate> app_delegate_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<AppViewGuest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppViewGuest);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_GUEST_H_
