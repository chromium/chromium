// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/directory_enumerator.h"

#include <utility>

#include "net/base/net_errors.h"

namespace headless {

// static
void DirectoryEnumerator::Start(
    base::FilePath path,
    scoped_refptr<content::FileSelectListener> listener) {
  // Self deleting.
  auto* instance =
      new DirectoryEnumerator(std::move(path), std::move(listener));
  instance->directory_lister_.Start();
}

DirectoryEnumerator::DirectoryEnumerator(
    base::FilePath directory,
    scoped_refptr<content::FileSelectListener> listener)
    : directory_(std::move(directory)),
      listener_(std::move(listener)),
      directory_lister_(directory_,
                        net::DirectoryLister::NO_SORT_RECURSIVE,
                        this) {}

DirectoryEnumerator::~DirectoryEnumerator() = default;

void DirectoryEnumerator::OnListFile(const DirectoryListerData& data) {
  if (data.info.IsDirectory())
    return;
  entries_.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
      blink::mojom::NativeFileInfo::New(data.path, /*display_name=*/u"")));
}

void DirectoryEnumerator::OnListDone(int error) {
  if (error != net::Error::OK) {
    listener_->FileSelectionCanceled();
  } else {
    listener_->FileSelected(
        std::move(entries_), directory_,
        blink::mojom::FileChooserParams::Mode::kUploadFolder);
  }
  delete this;
}

}  // namespace headless
