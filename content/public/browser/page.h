// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAGE_H_
#define CONTENT_PUBLIC_BROWSER_PAGE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "url/gurl.h"

namespace content {

// Page represents a collection of documents with the same main document.

// At the moment some navigations might create a new blink::Document in the
// existing RenderFrameHost, which will lead to a creation of a new Page
// associated with the same main RenderFrameHost. See the comment in
// |DocumentUserData| for more details and crbug.com/936696 for the
// progress on always creating a new RenderFrameHost for each new document.

// Page is created when a main document is created, which can happen in the
// following ways:
// 1) Main RenderFrameHost is created.
// 2) A cross-document non-bfcached navigation is committed in the same
//    RenderFrameHost.
// 3) Main RenderFrameHost is re-created after crash.

// Page is deleted in the following cases:
// 1) Main RenderFrameHost is deleted. Note that this might be different from
//    when the navigation commits, see the comment in
//    RenderFrameHost::LifecycleState::kPendingDeletion for more details.
// 2) A cross-document non-bfcached navigation is committed in the same
//    RenderFrameHost.
// 3) Before main RenderFrameHost is re-created after crash.

// If a method can be called only for main RenderFrameHosts or if its behaviour
// is identical when called on the parent / child RenderFrameHosts, it should
// be added to Page(Impl).

// With Multiple Page Architecture (MPArch), each WebContents may have
// additional FrameTrees which will have their own associated Page. Please take
// into consideration when assuming that Page is appropriate for storing
// something that's common for all frames you see on a tab.
// See docs/frame_trees.md for more details.

// NOTE: Depending on the process model, the cross-origin iframes are likely to
// be hosted in a different renderer process than the main document, so a given
// page is hosted in multiple renderer processes at the same time.
// RenderViewHost / `blink::WebView` / blink::Page (which are all 1:1:1)
// represent a part of a given content::Page in a given renderer process (note,
// however, that like RenderFrameHosts, these objects at the moment can be
// reused for a new content::Page for a cross-document same-site main-frame
// navigation).
class CONTENT_EXPORT Page : public base::SupportsUserData {
 public:
  ~Page() override = default;

  // The GURL for the page's web application manifest.
  // See https://w3c.github.io/manifest/#web-application-manifest
  virtual const std::optional<GURL>& GetManifestUrl() const = 0;

  // The callback invoked when the renderer responds to a request for the main
  // frame document's manifest. The url will be empty if the document specifies
  // no manifest, and the manifest will be empty if any other failures occurred.
  using GetManifestCallback =
      base::OnceCallback<void(blink::mojom::ManifestRequestResult,
                              const GURL&,
                              blink::mojom::ManifestPtr)>;

  // Requests the manifest URL and the Manifest of the main frame's document.
  // |callback| may be called after the WebContents has been destroyed.
  // This must be invoked on the UI thread, |callback| will be invoked on the UI
  // thread.
  virtual void GetManifest(GetManifestCallback callback) = 0;

  // Returns true iff this Page is primary for the associated `WebContents`
  // (i.e. web_contents->GetPrimaryPage() == this_page). Non-primary pages
  // include pages in bfcache, prerendering, fenced frames, pending commit and
  // pending deletion pages. See WebContents::GetPrimaryPage for more details.
  virtual bool IsPrimary() const = 0;

  // Returns the main RenderFrameHost associated with this Page.
  RenderFrameHost& GetMainDocument() { return GetMainDocumentHelper(); }

  // Write a description of this Page into the provided |context|.
  virtual void WriteIntoTrace(perfetto::TracedValue context) = 0;

  virtual base::WeakPtr<Page> GetWeakPtr() = 0;

  // Whether the most recent page scale factor sent by the main frame's renderer
  // is 1 (i.e. no magnification).
  virtual bool IsPageScaleFactorOne() = 0;

  // Returns the MIME type bound to the Page contents after a navigation.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Test version of `PageImpl::SetResizable` to allow tests outside of
  // //content to simulate the value normally set by the
  // window.setResizable(bool) API.
  virtual void SetResizableForTesting(std::optional<bool> resizable) = 0;
  // Returns the value set by `window.setResizable(bool)` API or `std::nullopt`
  // if unset which can override `BrowserView::CanResize`.
  virtual std::optional<bool> GetResizable() = 0;

 private:
  // This method is needed to ensure that PageImpl can both implement a Page's
  // method and define a new GetMainDocument() returning RenderFrameHostImpl.
  // Covariant types can't be used here due to circular includes as
  // RenderFrameHost::GetPage and RenderFrameHostImpl::GetPage already return
  // Page& and PageImpl& respectively, which means that page_impl.h can't
  // include render_frame_host_impl.h.
  virtual RenderFrameHost& GetMainDocumentHelper() = 0;

  // This interface should only be implemented inside content.
  friend class PageImpl;
  Page() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAGE_H_
