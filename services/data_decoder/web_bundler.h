// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_WEB_BUNDLER_H_
#define SERVICES_DATA_DECODER_WEB_BUNDLER_H_

#include <vector>

#include "base/files/file.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/resource_snapshot_for_web_bundle.mojom.h"
#include "services/data_decoder/public/mojom/web_bundler.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace data_decoder {

class WebBundler : public mojom::WebBundler {
 public:
  WebBundler();
  ~WebBundler() override;

  WebBundler(const WebBundler&) = delete;
  WebBundler& operator=(const WebBundler&) = delete;

 private:
  // mojom::WebBundler implementation.
  void Generate(
      std::vector<mojo::PendingRemote<mojom::ResourceSnapshotForWebBundle>>
          snapshots,
      base::File file,
      GenerateCallback callback) override;

  void OnConnectionError();
  void GetNextResourceCount();
  void OnGetResourceCount(uint64_t count);
  void GetNextResourceInfo();
  void OnGetResourceInfo(mojom::SerializedResourceInfoPtr info);
  void OnGetResourceBody(absl::optional<mojo_base::BigBuffer> body);
  void WriteWebBundleIndex();

  std::vector<mojo::Remote<mojom::ResourceSnapshotForWebBundle>> snapshots_;
  base::File file_;
  GenerateCallback callback_;
  std::vector<std::vector<mojom::SerializedResourceInfoPtr>> resources_;
  std::vector<std::vector<absl::optional<mojo_base::BigBuffer>>> bodies_;
  uint64_t pending_resource_count_;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_WEB_BUNDLER_H_
