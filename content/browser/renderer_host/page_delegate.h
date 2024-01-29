// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_DELEGATE_H_

namespace content {

class PageImpl;

// Interface implemented by an object (in practice, WebContentsImpl) which
// owns (possibly indirectly) and is interested in knowing about the state of
// one or more Pages. It must outlive the Page.
class PageDelegate {
 public:
  // Called when a paint happens after the first non empty layout. In other
  // words, after the page has painted something.
  virtual void OnFirstVisuallyNonEmptyPaint(PageImpl& page) {}

  // Called when the theme color (from the theme-color meta tag) has changed.
  virtual void OnThemeColorChanged(PageImpl& page) {}

  // Called when the main document background color has changed.
  virtual void OnBackgroundColorChanged(PageImpl& page) {}

  // Called when the main document color scheme was inferred.
  virtual void DidInferColorScheme(PageImpl& page) {}

  // Called when the main document's virtual keyboard mode changes.
  virtual void OnVirtualKeyboardModeChanged(PageImpl& page) {}

  // Called when `page` becomes primary in its FrameTree.
  virtual void NotifyPageBecamePrimary(PageImpl& page) = 0;

  // Tells if `page` should be handled as in preview mode.
  virtual bool IsPageInPreviewMode() const = 0;

  // Notifies `BrowserView` about the resizable boolean having been set vith
  // `window.setResizable(bool)` API.
  virtual void OnCanResizeFromWebAPIChanged() = 0;

  // Notify the page uses a forbidden powerful API and cannot be shown in
  // preview mode.
  virtual void CancelPreviewByMojoBinderPolicy(
      const std::string& interface_name) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_DELEGATE_H_
