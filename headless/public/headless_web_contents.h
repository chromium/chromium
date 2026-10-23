// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
#define HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "base/process/kill.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_window_state.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {
class SiteInstance;
}  // namespace content

namespace headless {
class HeadlessBrowserContext;

// Class representing contents of a browser tab. Should be accessed from
// browser main thread.
class HEADLESS_EXPORT HeadlessWebContents {
 public:
  // Used to configure web contents during creation. This exposes implementation
  // details to the public interface so should ideally be declared in the
  // implementation file. Consider moving this struct to
  // headless_web_contents_impl.h.
  struct HEADLESS_EXPORT CreateParams {
    STACK_ALLOCATED();

   public:
    explicit CreateParams(HeadlessBrowserContext* browser_context);
    CreateParams(HeadlessBrowserContext* browser_context,
                 const GURL& initial_url);
    ~CreateParams();

    CreateParams(const CreateParams&) = delete;
    CreateParams& operator=(const CreateParams&) = delete;
    CreateParams(CreateParams&&);
    CreateParams& operator=(CreateParams&&);

    // Associated browser context. CreateParams should not outlive the
    // browser context to avoid dangling pointer issues.
    raw_ptr<HeadlessBrowserContext> browser_context;

    // Initial URL to ensure that the renderer gets initialized and eventually
    // becomes ready to be inspected. If nullopt, the web contents is not
    // navigated upon creation and the caller is expected to navigate it right
    // away. An invalid URL makes creation fail.
    std::optional<GURL> initial_url = GURL("about:blank");

    // Initial window bounds. The default size is configured in browser
    // options.
    gfx::Rect window_bounds;

    // Initial window state.
    HeadlessWindowState window_state = HeadlessWindowState::kNormal;

    // Whether BeginFrames should be controlled via DevTools commands.
    bool enable_begin_frame_control = false;

    // Optional source SiteInstance.
    scoped_refptr<content::SiteInstance> source_site_instance = nullptr;
  };

  HeadlessWebContents(const HeadlessWebContents&) = delete;
  HeadlessWebContents& operator=(const HeadlessWebContents&) = delete;

  virtual ~HeadlessWebContents() {}

  // Close this page. |HeadlessWebContents| object will be destroyed.
  virtual void Close() = 0;

 protected:
  HeadlessWebContents() {}
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
