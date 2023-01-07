// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOCK_EXTERNAL_PROVIDER_H_
#define EXTENSIONS_BROWSER_MOCK_EXTERNAL_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

namespace base {
class Version;
}

namespace extensions {

class MockExternalProvider : public ExternalProviderInterface {
 public:
  MockExternalProvider(VisitorInterface* visitor,
                       mojom::ManifestLocation location);

  MockExternalProvider(const MockExternalProvider&) = delete;
  MockExternalProvider& operator=(const MockExternalProvider&) = delete;

  ~MockExternalProvider() override;

  void UpdateOrAddExtension(const ExtensionId& id,
                            const std::string& version,
                            const base::FilePath& path);
  void UpdateOrAddExtension(std::unique_ptr<ExternalInstallInfoFile> info);
  void UpdateOrAddExtension(std::unique_ptr<ExternalInstallInfoUpdateUrl> info);
  void RemoveExtension(const ExtensionId& id);

  // ExternalProviderInterface implementation:
  void VisitRegisteredExtension() override;
  bool HasExtension(const std::string& id) const override;
  bool GetExtensionDetails(
      const std::string& id,
      mojom::ManifestLocation* location,
      std::unique_ptr<base::Version>* version) const override;
  void TriggerOnExternalExtensionFound() override;
  bool IsReady() const override;
  void ServiceShutdown() override {}

  int visit_count() const { return visit_count_; }
  void set_visit_count(int visit_count) { visit_count_ = visit_count; }

 private:
  using FileDataMap =
      std::map<ExtensionId, std::unique_ptr<ExternalInstallInfoFile>>;
  using UrlDataMap =
      std::map<ExtensionId, std::unique_ptr<ExternalInstallInfoUpdateUrl>>;
  FileDataMap file_extension_map_;
  UrlDataMap url_extension_map_;
  mojom::ManifestLocation location_;
  raw_ptr<VisitorInterface> visitor_;

  // visit_count_ tracks the number of calls to VisitRegisteredExtension().
  // Mutable because it must be incremented on each call to
  // VisitRegisteredExtension(), which must be a const method to inherit
  // from the class being mocked.
  mutable int visit_count_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOCK_EXTERNAL_PROVIDER_H_
