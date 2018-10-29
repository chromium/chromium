/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FILE_CHOOSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FILE_CHOOSER_H_

#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-blink.h"
#include "third_party/blink/public/web/web_file_chooser_completion.h"
#include "third_party/blink/public/web/web_file_chooser_params.h"
#include "third_party/blink/renderer/core/page/popup_opening_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ChromeClientImpl;
class FileChooser;
class LocalFrame;

using FileChooserFileInfoList = Vector<mojom::blink::FileChooserFileInfoPtr>;

class CORE_EXPORT FileChooserClient : public PopupOpeningObserver {
 public:
  virtual void FilesChosen(const FileChooserFileInfoList&) = 0;
  virtual LocalFrame* FrameOrNull() const = 0;
  ~FileChooserClient() override;

 protected:
  FileChooser* NewFileChooser(const WebFileChooserParams&);
  bool HasConnectedFileChooser() const { return chooser_.get(); }

  // This should be called if a user chose files or cancel the dialog.
  void DisconnectFileChooser();

 private:
  scoped_refptr<FileChooser> chooser_;
};

class FileChooser : public RefCounted<FileChooser>,
                    public WebFileChooserCompletion {
 public:
  CORE_EXPORT static scoped_refptr<FileChooser> Create(
      FileChooserClient*,
      const WebFileChooserParams&);
  ~FileChooser() override;

  LocalFrame* FrameOrNull() const {
    return client_ ? client_->FrameOrNull() : nullptr;
  }
  void DisconnectClient() { client_ = nullptr; }

  const WebFileChooserParams& Params() const { return params_; }

  bool OpenFileChooser(ChromeClientImpl& chrome_client_impl);

 private:
  FileChooser(FileChooserClient*, const WebFileChooserParams&);

  void DidCloseChooser();

  // WebFileChooserCompletion implementation:
  void DidChooseFile(const WebVector<WebString>& file_names) override;
  void DidChooseFile(const WebVector<SelectedFileInfo>& files) override;

  // FIXME: We should probably just pass file paths that could be virtual paths
  // with proper display names rather than passing structs.
  void ChooseFiles(const FileChooserFileInfoList& files);

  WeakPersistent<FileChooserClient> client_;
  WebFileChooserParams params_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
};

CORE_EXPORT mojom::blink::FileChooserFileInfoPtr
CreateFileChooserFileInfoNative(const String& path,
                                const String& display_name = String());
CORE_EXPORT mojom::blink::FileChooserFileInfoPtr
CreateFileChooserFileInfoFileSystem(const KURL& url,
                                    base::Time modification_time,
                                    int64_t length);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_FILE_CHOOSER_H_
