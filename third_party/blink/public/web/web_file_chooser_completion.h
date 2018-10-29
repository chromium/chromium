/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FILE_CHOOSER_COMPLETION_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FILE_CHOOSER_COMPLETION_H_

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

template <typename T>
class WebVector;

// Gets called back when WebViewClient finished choosing a file.
class WebFileChooserCompletion {
 public:
  struct SelectedFileInfo {
    // The actual path of the selected file.
    base::FilePath file_path;

    // The display name of the file that is to be exposed as File.name in
    // the DOM layer. If it is empty the base part of the |path| is used.
    WebString display_name;

    // File system URL.
    WebURL file_system_url;

    // Metadata of non-native file.
    // 0 is Unix epoch, unit is sec.
    base::Time modification_time;
    long long length;
    bool is_directory;

    SelectedFileInfo() : length(0), is_directory(false) {}
  };

  // Called with zero or more file names. Zero-length vector means that
  // the user cancelled or that file choosing failed. The callback instance
  // is destroyed when this method is called.
  virtual void DidChooseFile(const WebVector<WebString>& file_names) = 0;

  // Called with zero or more files, given as a vector of SelectedFileInfo.
  // Zero-length vector means that the user cancelled or that file
  // choosing failed. The callback instance is destroyed when this method
  // is called.
  // FIXME: Deprecate either one of the didChooseFile (and rename it to
  // didChooseFile*s*).
  virtual void DidChooseFile(const WebVector<SelectedFileInfo>&) {}

 protected:
  virtual ~WebFileChooserCompletion() = default;
};

}  // namespace blink

#endif
