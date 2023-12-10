// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_FILE_CHOOSER_IMPL_H_
#define CONTENT_BROWSER_WEB_CONTENTS_FILE_CHOOSER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

namespace content {

class RenderFrameHostImpl;

// An implementation of blink::mojom::FileChooser and FileSelectListener
// associated to RenderFrameHost.
// TODO(sreejakshetty): Make FileChooserImpl per-frame and associate with
// DocumentUserData to ensure that the state is correctly tracked and
// deleted.
class CONTENT_EXPORT FileChooserImpl : public blink::mojom::FileChooser {
  using FileChooserResult = blink::mojom::FileChooserResult;

 public:
  // A FileSelectListenerImpl instance is owned by a FileChooserImpl and/or a
  // WebContents.
  class CONTENT_EXPORT FileSelectListenerImpl : public FileSelectListener {
   public:
    explicit FileSelectListenerImpl(FileChooserImpl* owner);
    void SetFullscreenBlock(base::ScopedClosureRunner fullscreen_block);

    // FileSelectListener overrides:

    // TODO(xiaochengh): Move |file| to the end of the argument list to match
    // the argument ordering of FileChooserImpl::FileSelected().
    void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                      const base::FilePath& base_dir,
                      blink::mojom::FileChooserParams::Mode mode) override;

    void FileSelectionCanceled() override;

    // This sets |was_file_select_listener_function_called_| to true so that
    // tests can pass with mocked overrides of this class.
    void SetListenerFunctionCalledTrueForTesting();

   protected:
    ~FileSelectListenerImpl() override;

   private:
    base::WeakPtr<FileChooserImpl> owner_;
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
  static std::pair<FileChooserImpl*, mojo::Remote<blink::mojom::FileChooser>>
  CreateForTesting(RenderFrameHostImpl* render_frame_host);

  ~FileChooserImpl() override;

  void FileSelected(const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode,
                    std::vector<blink::mojom::FileChooserFileInfoPtr> files);

  void FileSelectionCanceled();

  // blink::mojom::FileChooser overrides:

  void OpenFileChooser(blink::mojom::FileChooserParamsPtr params,
                       OpenFileChooserCallback callback) override;
  void EnumerateChosenDirectory(
      const base::FilePath& directory_path,
      EnumerateChosenDirectoryCallback callback) override;

  base::WeakPtr<FileChooserImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  explicit FileChooserImpl(RenderFrameHostImpl* render_frame_host);

  RenderFrameHostImpl* render_frame_host();

  const GlobalRenderFrameHostId render_frame_host_id_;
  scoped_refptr<FileSelectListenerImpl> listener_impl_;
  base::OnceCallback<void(blink::mojom::FileChooserResultPtr)> callback_;

  base::WeakPtrFactory<FileChooserImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_FILE_CHOOSER_IMPL_H_
