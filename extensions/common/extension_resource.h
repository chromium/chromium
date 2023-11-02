// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_RESOURCE_H_
#define EXTENSIONS_COMMON_EXTENSION_RESOURCE_H_

#include "base/files/file_path.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Represents a resource inside an extension. Hence a resource pointing to the
// root extension directory isn't a valid ExtensionResource.
// Examples include an image, or a JavaScript file. This is more complicated
// than just a simple FilePath because extension resources can come from
// multiple physical file locations depending on locale.
class ExtensionResource {
 public:
  // SymlinkPolicy decides whether we'll allow resources to be a symlink to
  // anywhere, or whether they must end up within the extension root.
  enum SymlinkPolicy {
    SYMLINKS_MUST_RESOLVE_WITHIN_ROOT,
    FOLLOW_SYMLINKS_ANYWHERE,
  };

  ExtensionResource();
  ExtensionResource(const ExtensionId& extension_id,
                    const base::FilePath& extension_root,
                    const base::FilePath& relative_path);
  ExtensionResource(const ExtensionResource& other);
  ExtensionResource(ExtensionResource&& other);
  ExtensionResource& operator=(ExtensionResource&& other);

  ~ExtensionResource();

  // set_follow_symlinks_anywhere allows the resource to be a symlink to
  // anywhere in the filesystem. By default, resources have to be within
  // |extension_root| after resolving symlinks.
  void set_follow_symlinks_anywhere();

  // Returns actual path to the resource (default or locale specific). In the
  // browser process, this will DCHECK if not called on the file thread. To
  // easily load extension images on the UI thread, see ImageLoader.
  const base::FilePath& GetFilePath() const;

  // Gets the physical file path for the extension resource, taking into account
  // localization. In the browser process, this will DCHECK if not called on the
  // file thread. To easily load extension images on the UI thread, see
  // ImageLoader.
  //
  // The relative path must not resolve to a location outside of
  // |extension_root|. Iff |file_can_symlink_outside_root| is true, then the
  // file can be a symlink that links outside of |extension_root|.
  static base::FilePath GetFilePath(const base::FilePath& extension_root,
                                    const base::FilePath& relative_path,
                                    SymlinkPolicy symlink_policy);

  // Getters
  const ExtensionId& extension_id() const { return extension_id_; }

  // Note that this might be empty for a valid ExtensionResource since dummy
  // Extension objects may be created with an empty extension root path in code.
  const base::FilePath& extension_root() const { return extension_root_; }

  const base::FilePath& relative_path() const { return relative_path_; }

  bool empty() const { return relative_path_.empty(); }

 private:
  // The id of the extension that this resource is associated with.
  ExtensionId extension_id_;

  // Extension root.
  base::FilePath extension_root_;

  // Relative path to resource.
  base::FilePath relative_path_;

  // If |follow_symlinks_anywhere_| is true then the resource itself must be
  // within |extension_root|, but it can be a symlink to a file that is not.
  bool follow_symlinks_anywhere_;

  // Full path to extension resource. Starts empty.
  mutable base::FilePath full_resource_path_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_RESOURCE_H_
