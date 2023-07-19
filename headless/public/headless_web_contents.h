// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
#define HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_

#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "headless/public/headless_export.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace headless {
class HeadlessBrowserContextImpl;
class HeadlessBrowserImpl;

// Class representing contents of a browser tab. Should be accessed from browser
// main thread.
class HEADLESS_EXPORT HeadlessWebContents {
 public:
  class HEADLESS_EXPORT Builder;

  HeadlessWebContents(const HeadlessWebContents&) = delete;
  HeadlessWebContents& operator=(const HeadlessWebContents&) = delete;

  virtual ~HeadlessWebContents() {}

  class HEADLESS_EXPORT Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // All the following notifications will be called on browser main thread.

    // Indicates that this HeadlessWebContents instance is now ready to be
    // inspected.
    // TODO(altimin): Support this event for pages that aren't created by us.
    virtual void DevToolsTargetReady() {}
    // This method is invoked when the process of the observed RenderProcessHost
    // exits (either normally or with a crash). To determine if the process
    // closed normally or crashed, examine the |status| parameter.
    //
    // If |status| is TERMINATION_STATUS_LAUNCH_FAILED then |exit_code| will
    // contain a platform specific launch failure error code. Otherwise, it will
    // contain the exit code for the process.
    virtual void RenderProcessExited(base::TerminationStatus status,
                                     int exit_code) {}

   protected:
    Observer() {}
    virtual ~Observer() {}
  };

  // Add or remove an observer to receive events from this WebContents.
  // |observer| must outlive this class or be removed prior to being destroyed.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Close this page. |HeadlessWebContents| object will be destroyed.
  virtual void Close() = 0;

 protected:
  HeadlessWebContents() {}
};

class HEADLESS_EXPORT HeadlessWebContents::Builder {
 public:
  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  ~Builder();
  Builder(Builder&&);

  // Set an initial URL to ensure that the renderer gets initialized and
  // eventually becomes ready to be inspected. See
  // HeadlessWebContents::Observer::DevToolsTargetReady. The default URL is
  // about:blank.
  Builder& SetInitialURL(const GURL& initial_url);

  // Specify the initial window size (default is configured in browser options).
  Builder& SetWindowSize(const gfx::Size& size);

  // Specify whether BeginFrames should be controlled via DevTools commands.
  Builder& SetEnableBeginFrameControl(bool enable_begin_frame_control);

  // Specify whether to create the CDP target of type "tab".
  Builder& SetUseTabTarget(bool use_tab_target);

  // The returned object is owned by HeadlessBrowser. Call
  // HeadlessWebContents::Close() to dispose it.
  HeadlessWebContents* Build();

 private:
  friend class HeadlessBrowserImpl;
  friend class HeadlessBrowserContextImpl;
  friend class HeadlessWebContentsImpl;

  explicit Builder(HeadlessBrowserContextImpl* browser_context);

  raw_ptr<HeadlessBrowserContextImpl, AcrossTasksDanglingUntriaged>
      browser_context_;

  GURL initial_url_ = GURL("about:blank");
  gfx::Size window_size_;
  bool enable_begin_frame_control_ = false;
  bool use_tab_target_ = false;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_WEB_CONTENTS_H_
