// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/web_bundler.h"

#include "base/big_endian.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "components/web_package/web_bundle_builder.h"

namespace data_decoder {

// WebBundler does not permit body size larder than ~1GB.
const uint64_t kMaxBodySize = (1 << 30);

WebBundler::WebBundler() = default;
WebBundler::~WebBundler() = default;

void WebBundler::Generate(
    std::vector<mojo::PendingRemote<mojom::ResourceSnapshotForWebBundle>>
        snapshots,
    base::File file,
    GenerateCallback callback) {
  DCHECK(snapshots_.empty());
  DCHECK(!snapshots.empty());
  for (auto& pending_snapshot : snapshots) {
    mojo::Remote<mojom::ResourceSnapshotForWebBundle> snapshot(
        std::move(pending_snapshot));
    snapshot.set_disconnect_handler(
        base::BindOnce(&WebBundler::OnConnectionError, base::Unretained(this)));
    snapshots_.emplace_back(std::move(snapshot));
  }
  file_ = std::move(file);
  callback_ = std::move(callback);
  GetNextResourceCount();
}

void WebBundler::OnConnectionError() {
  if (callback_) {
    std::move(callback_).Run(0, mojom::WebBundlerError::kConnectionError);
  }
}

void WebBundler::GetNextResourceCount() {
  if (snapshots_.size() == resources_.size()) {
    WriteWebBundleIndex();
    return;
  }
  snapshots_[resources_.size()]->GetResourceCount(
      base::BindOnce(&WebBundler::OnGetResourceCount, base::Unretained(this)));
}

void WebBundler::OnGetResourceCount(uint64_t count) {
  pending_resource_count_ = count;
  resources_.emplace_back();
  bodies_.emplace_back();
  GetNextResourceInfo();
}

void WebBundler::GetNextResourceInfo() {
  if (pending_resource_count_ == 0) {
    GetNextResourceCount();
    return;
  }
  snapshots_[resources_.size() - 1]->GetResourceInfo(
      resources_.rbegin()->size(),
      base::BindOnce(&WebBundler::OnGetResourceInfo, base::Unretained(this)));
}

void WebBundler::OnGetResourceInfo(mojom::SerializedResourceInfoPtr info) {
  resources_.rbegin()->emplace_back(std::move(info));
  snapshots_[bodies_.size() - 1]->GetResourceBody(
      bodies_.rbegin()->size(),
      base::BindOnce(&WebBundler::OnGetResourceBody, base::Unretained(this)));
}

void WebBundler::OnGetResourceBody(absl::optional<mojo_base::BigBuffer> body) {
  if (body->size() > kMaxBodySize) {
    std::move(callback_).Run(0, mojom::WebBundlerError::kInvalidInput);
    return;
  }
  bodies_.rbegin()->emplace_back(std::move(body));
  --pending_resource_count_;
  GetNextResourceInfo();
}

void WebBundler::WriteWebBundleIndex() {
  if (!callback_) {
    return;
  }
  GURL url = resources_[0][0]->url;
  GURL::Replacements replacements;
  replacements.ClearRef();
  url = url.ReplaceComponents(replacements);
  std::set<GURL> url_set;
  CHECK_EQ(resources_.size(), bodies_.size());
  std::vector<mojom::SerializedResourceInfoPtr> resources;
  std::vector<absl::optional<mojo_base::BigBuffer>> bodies;
  for (size_t i = 0; i < resources_.size(); ++i) {
    auto& info_list = resources_[i];
    auto& body_list = bodies_[i];
    CHECK_EQ(info_list.size(), body_list.size());
    for (size_t j = 0; j < info_list.size(); ++j) {
      auto& info = info_list[j];
      auto& body = body_list[j];
      if (url_set.find(info->url) == url_set.end() && info->url.is_valid() &&
          info->url.SchemeIsHTTPOrHTTPS()) {
        url_set.insert(info->url);
        resources.emplace_back(std::move(info));
        bodies.emplace_back(std::move(body));
      }
    }
  }

  CHECK_EQ(resources.size(), bodies.size());
  web_package::WebBundleBuilder builder(web_package::BundleVersion::kB2);
  builder.AddPrimaryURL(url);
  for (size_t i = 0; i < resources.size(); ++i) {
    const auto& info = resources[i];
    const auto& body = bodies[i];
    web_package::WebBundleBuilder::Headers headers = {
        {":status", "200"}, {"content-type", info->mime_type}};
    web_package::WebBundleBuilder::ResponseLocation response_location =
        builder.AddResponse(
            headers, body ? base::StringPiece(
                                reinterpret_cast<const char*>(body->data()),
                                body->size())
                          : "");
    GURL::Replacements resource_replacements;
    resource_replacements.ClearRef();
    GURL resource_url = info->url.ReplaceComponents(resource_replacements);
    builder.AddIndexEntry(resource_url, response_location);
  }
  std::vector<uint8_t> bundle = builder.CreateBundle();
  int written_size = file_.WriteAtCurrentPos(
      reinterpret_cast<const char*>(bundle.data()), bundle.size());
  DCHECK_EQ(static_cast<int>(bundle.size()), written_size);
  std::move(callback_).Run(written_size, mojom::WebBundlerError::kOK);
}

}  // namespace data_decoder
