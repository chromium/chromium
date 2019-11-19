// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_COMPUTED_HASHES_H_
#define EXTENSIONS_BROWSER_COMPUTED_HASHES_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>


namespace base {
class FilePath;
class ListValue;
}

namespace extensions {

// A pair of classes for serialization of a set of SHA256 block hashes computed
// over the files inside an extension.
class ComputedHashes {
 public:
  class Reader {
   public:
    Reader();
    ~Reader();
    bool InitFromFile(const base::FilePath& path);

    // The block size and hashes for |relative_path| will be copied into the
    // out parameters.
    bool GetHashes(const base::FilePath& relative_path,
                   int* block_size,
                   std::vector<std::string>* hashes) const;

   private:
    typedef std::pair<int, std::vector<std::string> > HashInfo;

    // This maps a relative path to a pair of (block size, hashes)
    std::map<base::FilePath, HashInfo> data_;
  };

  class Writer {
   public:
    Writer();
    ~Writer();

    // Adds hashes for |relative_path|. Should not be called more than once
    // for a given |relative_path|.
    void AddHashes(const base::FilePath& relative_path,
                   int block_size,
                   const std::vector<std::string>& hashes);

    bool WriteToFile(const base::FilePath& path);

   private:
    // Each element of this list contains the path and block hashes for one
    // file.
    std::unique_ptr<base::ListValue> file_list_;
  };

  // Returns the SHA256 hash of each |block_size| chunk in |contents|.
  static std::vector<std::string> GetHashesForContent(
      const std::string& contents,
      size_t block_size);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_COMPUTED_HASHES_H_
