// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_file_select_helper.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

namespace content {

// static
void ShellFileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // ShellFileSelectHelper will keep itself alive until it sends the result
  // message.
  scoped_refptr<ShellFileSelectHelper> file_select_helper(
      new ShellFileSelectHelper());
  file_select_helper->RunFileChooser(render_frame_host, std::move(listener),
                                     params.Clone());
}

ShellFileSelectHelper::ShellFileSelectHelper() = default;

ShellFileSelectHelper::~ShellFileSelectHelper() = default;

void ShellFileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    blink::mojom::FileChooserParamsPtr params) {
  // TODO(crbug.com/1412107): Create SelectFileDialog.
  listener->FileSelectionCanceled();
}

}  // namespace content
