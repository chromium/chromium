// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SAVE_AS_WEB_BUNDLE_JOB_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SAVE_AS_WEB_BUNDLE_JOB_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/resource_snapshot_for_web_bundle.mojom.h"
#include "services/data_decoder/public/mojom/web_bundler.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

namespace content {

class WebContents;

// This class is used by WebContents::GenerateWebBundle() method to generate
// a Web Bundle file. The instances are created by Start() static method. Every
// instance is self-owned and responsible for deleting itself upon invoking
// OnFinished.
class SaveAsWebBundleJob {
 public:
  static void Start(
      WebContents* web_contents,
      const base::FilePath& file_path,
      base::OnceCallback<void(uint64_t /* file_size */,
                              data_decoder::mojom::WebBundlerError)> callback);

  SaveAsWebBundleJob(const SaveAsWebBundleJob&) = delete;
  SaveAsWebBundleJob& operator=(const SaveAsWebBundleJob&) = delete;

 private:
  SaveAsWebBundleJob(
      const base::FilePath& file_path,
      std::vector<mojo::PendingRemote<
          data_decoder::mojom::ResourceSnapshotForWebBundle>> snapshots,
      base::OnceCallback<void(uint64_t /* file_size */,
                              data_decoder::mojom::WebBundlerError)> callback);
  ~SaveAsWebBundleJob();

  void OnFileAvailable(base::File file);
  void OnConnectionError();
  void OnGenerated(uint64_t file_size,
                   data_decoder::mojom::WebBundlerError error);

  void OnFinished(uint64_t file_size,
                  data_decoder::mojom::WebBundlerError error);

  std::unique_ptr<data_decoder::DataDecoder> data_decoder_;
  std::vector<
      mojo::PendingRemote<data_decoder::mojom::ResourceSnapshotForWebBundle>>
      snapshots_;
  mojo::Remote<data_decoder::mojom::WebBundler> bundler_;
  base::OnceCallback<void(uint64_t /* file_size */,
                          data_decoder::mojom::WebBundlerError)>
      callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SAVE_AS_WEB_BUNDLE_JOB_H_
