// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef HEADLESS_LIB_BROWSER_DIRECTORY_ENUMERATOR_H_
#define HEADLESS_LIB_BROWSER_DIRECTORY_ENUMERATOR_H_

#include "base/files/file_path.h"
#include "content/public/browser/file_select_listener.h"
#include "net/base/directory_lister.h"

namespace headless {

class DirectoryEnumerator
    : public net::DirectoryLister::DirectoryListerDelegate {
 public:
  static void Start(base::FilePath directory,
                    scoped_refptr<content::FileSelectListener> listener);

 private:
  DirectoryEnumerator(base::FilePath directory,
                      scoped_refptr<content::FileSelectListener> listener);
  ~DirectoryEnumerator() override;

  using DirectoryListerData = net::DirectoryLister::DirectoryListerData;

  // net::DirectoryLister::DirectoryListerDelegate implementation
  void OnListFile(const DirectoryListerData& data) override;
  void OnListDone(int error) override;

  const base::FilePath directory_;
  scoped_refptr<content::FileSelectListener> listener_;
  net::DirectoryLister directory_lister_;
  std::vector<blink::mojom::FileChooserFileInfoPtr> entries_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_DIRECTORY_ENUMERATOR_H_