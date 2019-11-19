// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_IO_DATA_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_IO_DATA_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "extensions/browser/content_verifier_delegate.h"

namespace extensions {

// A helper class for keeping track of data for the ContentVerifier that should
// only be accessed on the IO thread.
class ContentVerifierIOData {
 public:
  struct ExtensionData {
    // Set of images file paths used within the browser process.
    std::unique_ptr<std::set<base::FilePath>> browser_image_paths;
    // Set of file paths used as background scripts, pages or content scripts.
    std::unique_ptr<std::set<base::FilePath>> background_or_content_paths;
    base::Version version;
    ContentVerifierDelegate::VerifierSourceType source_type;

    ExtensionData(
        std::unique_ptr<std::set<base::FilePath>> browser_image_paths,
        std::unique_ptr<std::set<base::FilePath>> background_or_content_paths,
        const base::Version& version,
        ContentVerifierDelegate::VerifierSourceType source_type);
    ~ExtensionData();
  };

  ContentVerifierIOData();
  ~ContentVerifierIOData();

  void AddData(const std::string& extension_id,
               std::unique_ptr<ExtensionData> data);
  void RemoveData(const std::string& extension_id);
  void Clear();

  // This should be called on the IO thread, and the return value should not
  // be retained or used on other threads.
  const ExtensionData* GetData(const std::string& extension_id);

 private:
  std::map<std::string, std::unique_ptr<ExtensionData>> data_map_;

  DISALLOW_COPY_AND_ASSIGN(ContentVerifierIOData);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_IO_DATA_H_
