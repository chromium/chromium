// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_HOST_IMPL_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/file/file_utilities.mojom.h"

namespace content {

class FileUtilitiesHostImpl : public blink::mojom::FileUtilitiesHost {
 public:
  explicit FileUtilitiesHostImpl(int process_id);
  ~FileUtilitiesHostImpl() override;

  static void Create(
      int process_id,
      mojo::PendingReceiver<blink::mojom::FileUtilitiesHost> receiver);

 private:
  // blink::mojom::FileUtilitiesHost implementation.
  void GetFileInfo(const base::FilePath& path,
                   GetFileInfoCallback callback) override;

  const int process_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_HOST_IMPL_H_
