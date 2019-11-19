// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace base {
class File;
}  // namespace base

namespace content {

// A class to abstract required information to access a BundledExchanges.
class CONTENT_EXPORT BundledExchangesSource {
 public:
  enum class Type {
    kTrustedFile,
    kFile,
    kNetwork,
  };

  // Used only for testing navigation to a trustable BundledExchanges source
  // with --trustable-bundled-exchanges-file-url flag. Returns null when failed
  // to get the filename from the |url|.
  static std::unique_ptr<BundledExchangesSource> MaybeCreateFromTrustedFileUrl(
      const GURL& url);

  // Returns a new BundledExchangesSource for the |url| if the scheme of |url|
  // is file: (or content: on Android). Otherwise returns null.
  static std::unique_ptr<BundledExchangesSource> MaybeCreateFromFileUrl(
      const GURL& url);

  // Returns a new BundledExchangesSource for the |url| if the scheme of |url|
  // is https: or localhost http:. Otherwise returns null.
  static std::unique_ptr<BundledExchangesSource> MaybeCreateFromNetworkUrl(
      const GURL& url);

  ~BundledExchangesSource() = default;

  std::unique_ptr<BundledExchangesSource> Clone() const;

  std::unique_ptr<base::File> OpenFile() const;

  Type type() const { return type_; }
  bool is_trusted_file() const { return type_ == Type::kTrustedFile; }
  bool is_file() const { return type_ == Type::kFile; }
  bool is_network() const { return type_ == Type::kNetwork; }

  const GURL& url() const { return url_; }

 private:
  BundledExchangesSource(Type type,
                         const base::FilePath& file_path,
                         const GURL& url);

  const Type type_;
  const base::FilePath file_path_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
