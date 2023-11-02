// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_TYPES_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_TYPES_H_

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/types/id_type.h"
#include "url/gurl.h"

namespace content {

class SavePackage;
using SavePackageId = base::IdType32<SavePackage>;

class SaveItem;
using SaveItemId = base::IdType32<SaveItem>;

// Map from save_item_id into final file path.
using FinalNamesMap =
    std::unordered_map<SaveItemId, base::FilePath, SaveItemId::Hasher>;

// This structure is used to handle and deliver some info
// when processing each save item job.
struct SaveFileCreateInfo {
  enum SaveFileSource {
    // This type indicates the source is not set.
    SAVE_FILE_FROM_UNKNOWN = -1,
    // This type indicates the save item needs to be retrieved from the network.
    SAVE_FILE_FROM_NET = 0,
    // This type indicates the save item needs to be retrieved from serializing
    // DOM.
    SAVE_FILE_FROM_DOM,
  };

  // Constructor for SAVE_FILE_FROM_DOM and/or SAVE_FILE_FROM_FILE.
  SaveFileCreateInfo(const base::FilePath& path,
                     const GURL& url,
                     SaveItemId save_item_id,
                     SavePackageId save_package_id,
                     int render_process_id,
                     int render_frame_routing_id,
                     SaveFileSource save_source);

  // Constructor for SAVE_FILE_FROM_NET case.
  SaveFileCreateInfo(const GURL& url,
                     const GURL& final_url,
                     SaveItemId save_item_id,
                     SavePackageId save_package_id,
                     int render_process_id,
                     int render_frame_routing_id,
                     const std::string& content_disposition);

  SaveFileCreateInfo(const SaveFileCreateInfo& other);

  ~SaveFileCreateInfo();

  // SaveItem fields.
  // The local file path of saved file.
  base::FilePath path;
  // Original URL of the saved resource.
  GURL url;
  // Final URL of the saved resource since some URL might be redirected.
  GURL final_url;
  // The unique identifier of SaveItem object associated with this job.
  SaveItemId save_item_id;
  // ID of SavePackage object.
  SavePackageId save_package_id;
  // IDs for looking up the contents we are associated with.
  int render_process_id;
  int render_frame_routing_id;
  // Disposition info from HTTP response.
  std::string content_disposition;
  // Source type of saved file.
  SaveFileSource save_source;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_TYPES_H_
