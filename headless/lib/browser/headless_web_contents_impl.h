// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "headless/lib/browser/headless_window_tree_host.h"
#include "headless/public/headless_export.h"
#include "headless/public/headless_web_contents.h"

class SkBitmap;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace headless {
class HeadlessBrowserImpl;

// Exported for tests.
class HEADLESS_EXPORT HeadlessWebContentsImpl : public HeadlessWebContents {
 public:
  HeadlessWebContentsImpl(const HeadlessWebContentsImpl&) = delete;
  HeadlessWebContentsImpl& operator=(const HeadlessWebContentsImpl&) = delete;

  ~HeadlessWebContentsImpl() override;

  static HeadlessWebContentsImpl* From(HeadlessWebContents* web_contents);
  static HeadlessWebContentsImpl* From(content::WebContents* web_contents);

  static std::unique_ptr<HeadlessWebContentsImpl> Create(
      HeadlessWebContents::Builder* builder);

  // Takes ownership of |child_contents|.
  static std::unique_ptr<HeadlessWebContentsImpl> CreateForChildContents(
      HeadlessWebContentsImpl* parent,
      std::unique_ptr<content::WebContents> child_contents);

  content::WebContents* web_contents() const;
  bool OpenURL(const GURL& url);

  void Close() override;

  HeadlessBrowserImpl* browser() const;
  HeadlessBrowserContextImpl* browser_context() const;

  void set_window_tree_host(std::unique_ptr<HeadlessWindowTreeHost> host) {
    window_tree_host_ = std::move(host);
  }
  HeadlessWindowTreeHost* window_tree_host() const {
    return window_tree_host_.get();
  }
  int window_id() const { return window_id_; }
  void set_window_state(const std::string& state) {
    DCHECK(state == "normal" || state == "minimized" || state == "maximized" ||
           state == "fullscreen");
    window_state_ = state;
  }
  const std::string& window_state() const { return window_state_; }

  // Set bounds of WebContent's platform window.
  void SetBounds(const gfx::Rect& bounds);

  bool begin_frame_control_enabled() const {
    return begin_frame_control_enabled_;
  }

  using FrameFinishedCallback =
      base::OnceCallback<void(bool /* has_damage */,
                              std::unique_ptr<SkBitmap>,
                              std::string /* error_message*/)>;
  void BeginFrame(const base::TimeTicks& frame_timeticks,
                  const base::TimeTicks& deadline,
                  const base::TimeDelta& interval,
                  bool animate_only,
                  bool capture_screenshot,
                  FrameFinishedCallback frame_finished_callback);

 private:
  explicit HeadlessWebContentsImpl(
      std::unique_ptr<content::WebContents> web_contents);

  void InitializeWindow(const gfx::Rect& initial_bounds);

  uint64_t begin_frame_sequence_number_ =
      viz::BeginFrameArgs::kStartingFrameNumber;
  bool begin_frame_control_enabled_ = false;

  class Delegate;
  std::unique_ptr<Delegate> web_contents_delegate_;
  std::unique_ptr<HeadlessWindowTreeHost> window_tree_host_;
  int window_id_ = 0;
  std::string window_state_;
  std::unique_ptr<content::WebContents> const web_contents_;

  class PendingFrame;
  base::WeakPtr<PendingFrame> pending_frame_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WEB_CONTENTS_IMPL_H_
