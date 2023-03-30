// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_FILE_SELECT_HELPER_H_
#define CONTENT_SHELL_BROWSER_SHELL_FILE_SELECT_HELPER_H_

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"

namespace content {

class RenderFrameHost;

// This class handles file-selection requests coming from renderer processes.
// It implements both the initialisation and listener functions for
// file-selection dialogs.
//
// Since ShellFileSelectHelper listens to observations of a widget, it needs to
// live on and be destroyed on the UI thread. References to
// ShellFileSelectHelper may be passed on to other threads.
// TODO(crbug.com/1412107): Implement SelectFileDialog::Listener
class ShellFileSelectHelper
    : public base::RefCountedThreadSafe<ShellFileSelectHelper,
                                        BrowserThread::DeleteOnUIThread> {
 public:
  ShellFileSelectHelper(const ShellFileSelectHelper&) = delete;
  ShellFileSelectHelper& operator=(const ShellFileSelectHelper&) = delete;

  // Show the file chooser dialog.
  static void RunFileChooser(content::RenderFrameHost* render_frame_host,
                             scoped_refptr<FileSelectListener> listener,
                             const blink::mojom::FileChooserParams& params);

 private:
  friend class base::RefCountedThreadSafe<ShellFileSelectHelper>;
  friend class base::DeleteHelper<ShellFileSelectHelper>;
  friend struct content::BrowserThread::DeleteOnThread<BrowserThread::UI>;

  ShellFileSelectHelper();
  virtual ~ShellFileSelectHelper();

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<FileSelectListener> listener,
                      blink::mojom::FileChooserParamsPtr params);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_FILE_SELECT_HELPER_H_
