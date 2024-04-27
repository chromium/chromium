// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_
#define CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {
class SingleThreadTaskRunner;
struct UserMetricsAction;
}

namespace content {

// An abstract base class that contains logic shared between most child
// processes of the embedder.
class CONTENT_EXPORT ChildThread
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
    : public IPC::Sender
#endif
{
 public:
  // Returns the one child thread for this process.  Note that this can only be
  // accessed when running on the child thread itself.
  static ChildThread* Get();

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  ~ChildThread() override = default;
#else
  virtual ~ChildThread() = default;
#endif

  // Sends over a base::UserMetricsAction to be recorded by user metrics as
  // an action. Once a new user metric is added, run
  //   tools/metrics/actions/extract_actions.py
  // to add the metric to actions.xml, then update the <owner>s and
  // <description> sections. Make sure to include the actions.xml file when you
  // upload your code for review!
  //
  // WARNING: When using base::UserMetricsAction, base::UserMetricsAction
  // and a string literal parameter must be on the same line, e.g.
  //   RenderThread::Get()->RecordAction(
  //       base::UserMetricsAction("my extremely long action name"));
  // because otherwise our processing scripts won't pick up on new actions.
  virtual void RecordAction(const base::UserMetricsAction& action) = 0;

  // Sends over a string to be recorded by user metrics as a computed action.
  // When you use this you need to also update the rules for extracting known
  // actions in chrome/tools/extract_actions.py.
  virtual void RecordComputedAction(const std::string& action) = 0;

  // Asks the browser-side process host object to bind |receiver|. Whether or
  // not the interface type encapsulated by |receiver| is supported depends on
  // the process type and potentially on the Content embedder.
  //
  // Receivers passed into this method arrive in the browser process and are
  // taken through one of the following flows, stopping if any step decides to
  // bind the receiver:
  //
  //   For renderers:
  //       1. IO thread, IOThreadHostImpl::BindHostReceiver.
  //       2. Main thread, RenderProcessHostImpl::BindHostReceiver.
  //       3. Main thread, ContentBrowserClient::BindHostReceiverForRenderer.
  //
  // TODO(crbug.com/40633267): Document behavior for other process types when
  // their support is added.
  virtual void BindHostReceiver(mojo::GenericPendingReceiver receiver) = 0;

  virtual scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() = 0;

  // Tells the child process that a field trial was activated.
  virtual void SetFieldTrialGroup(const std::string& trial_name,
                                  const std::string& group_name) = 0;

#if BUILDFLAG(IS_WIN)
  // Request that the given font be loaded by the browser so it's cached by the
  // OS. Please see ChildProcessHost::PreCacheFont for details.
  virtual void PreCacheFont(const LOGFONT& log_font) = 0;

  // Release cached font.
  virtual void ReleaseCachedFonts() = 0;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_CHILD_THREAD_H_
