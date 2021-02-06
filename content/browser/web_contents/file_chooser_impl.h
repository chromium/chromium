// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_FILE_CHOOSER_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_FILE_CHOOSER_IMPL_H_

#include "base/callback_helpers.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

namespace content {

class RenderFrameHostImpl;

// An implementation of blink::mojom::FileChooser and FileSelectListener
// associated to RenderFrameHost.
// TODO(sreejakshetty): Make FileChooserImpl per-frame and associate with
// RenderDocumentHostUserData to ensure that the state is correctly tracked and
// deleted.
class CONTENT_EXPORT FileChooserImpl : public blink::mojom::FileChooser,
                                       public WebContentsObserver {
  using FileChooserResult = blink::mojom::FileChooserResult;

 public:
  // A FileSelectListenerImpl instance is owned by a FileChooserImpl and/or a
  // WebContents.
  class CONTENT_EXPORT FileSelectListenerImpl : public FileSelectListener {
   public:
    explicit FileSelectListenerImpl(FileChooserImpl* owner) : owner_(owner) {}
    void SetFullscreenBlock(base::ScopedClosureRunner fullscreen_block);
    void ResetOwner() { owner_ = nullptr; }

    // FileSelectListener overrides:

    void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                      const base::FilePath& base_dir,
                      blink::mojom::FileChooserParams::Mode mode) override;

    void FileSelectionCanceled() override;

   protected:
    ~FileSelectListenerImpl() override;
    // This sets |was_file_select_listener_function_called_| to true so that
    // tests can pass with mocked overrides of this class.
    void SetListenerFunctionCalledTrueForTesting();

   private:
    FileChooserImpl* owner_;
    base::ScopedClosureRunner fullscreen_block_;
#if DCHECK_IS_ON()
    bool was_file_select_listener_function_called_ = false;
    bool was_fullscreen_block_set_ = false;
#endif
  };

  static void Create(RenderFrameHostImpl* render_frame_host,
                     mojo::PendingReceiver<blink::mojom::FileChooser> receiver);
  static mojo::Remote<blink::mojom::FileChooser> CreateBoundForTesting(
      RenderFrameHostImpl* render_frame_host);

  ~FileChooserImpl() override;

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode);

  void FileSelectionCanceled();

  // blink::mojom::FileChooser overrides:

  void OpenFileChooser(blink::mojom::FileChooserParamsPtr params,
                       OpenFileChooserCallback callback) override;
  void EnumerateChosenDirectory(
      const base::FilePath& directory_path,
      EnumerateChosenDirectoryCallback callback) override;

 private:
  explicit FileChooserImpl(RenderFrameHostImpl* render_frame_host);

  // WebContentsObserver overrides:

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  RenderFrameHostImpl* render_frame_host_;
  scoped_refptr<FileSelectListenerImpl> listener_impl_;
  base::OnceCallback<void(blink::mojom::FileChooserResultPtr)> callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_FILE_CHOOSER_IMPL_H_
