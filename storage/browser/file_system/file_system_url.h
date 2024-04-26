// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_H_

#include <optional>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/common/file_system/file_system_mount_option.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

// The equivalent of a file path, but for the virtual file systems that this
// C++ API serves. `storage::FileSystemContext::CreateFileStreamReader` takes a
// `storage::FileSystemURL` argument the same way that fopen takes a file path.
//
//
// # Historical Usage
//
// Originally (https://www.w3.org/TR/2012/WD-file-system-api-20120417/ is from
// 2012), a `FileSystemURL` (the C++ object) represented a URL whose scheme was
// "filesystem" (instead of e.g. "http", "file" or "data").
//
// For example, running this JavaScript
// (https://gist.github.com/miketaylr/df58ae669abc4eec1b514f4cfc71fc21) on
// https://example.com could produce
//   "filesystem:https://example.com/temporary/coolguy.txt"
// which you could navigate to in the browser.
//
// Apart from some colons and slashes, this URL string literally concatenates:
//  - a scheme, "filesystem".
//  - an origin, "https://example.com".
//  - a mount type, "temporary".
//  - a relative path, also called a virtual path, "coolguy.txt". This example
//    has no slashes but, in general, the relative path could be "a/b/c.dat".
//
// The C++ object (in 2012) was basically just those fields:
// https://chromiumcodereview.appspot.com/10566002/diff/18001/webkit/fileapi/file_system_url.h
//
// Note that the "File System" and "File System Entry" JS APIs are different to
// the similarly named but more recent "File System Access" JS API. Similarly,
// "filesystem:etc" URLs are not the same as "file:etc" URLs.
//
//
// # Evolution
//
// The C++ object has gained additional fields since then:
//  - its optional `BucketLocator` configures partitioned storage.
//  - its 'origin' has grown from an `url::Origin` to be a `blink::StorageKey`.
//    The distinction can matter for web pages containing third-party iframes.
//  - see the "Cracking" section, below.
//
// Buckets and storage keys are relevant for the kFileSystemTypeTemporary mount
// type.
//
// Cracking is relevant for kFileSystemTypeIsolated and kFileSystemTypeExternal
// mount types.
//
// This extra data isn't part of the string form. Creating a `FileSystemURL`
// (from a factory method) and then optionally calling its setter methods
// (`SetBucket`) fills in these other fields. Converting such a `FileSystemURL`
// back to string form can lose information.
//
//
// # Current Usage
//
// Third party JavaScript can obtain a `FileSystemURL` (in string form) by
// calling the `FileSystemEntry.toURL()` JavaScript API.
// https://developer.mozilla.org/en-US/docs/Web/API/FileSystemEntry/toURL
//
// Per that mozilla.org page, its use (in web-facing JS) is deprecated in favor
// of the "File System Access" API.
//
// Outside of web-facing JS, Chrome and particularly ChromeOS still uses
// `FileSystemURL`s internally (as C++ objects) to be 'virtual file paths' in
// its virtual file systems. But serialization to and from the string or `GURL`
// form (and subsequent sharing with JS code) is a legacy concept. The "URL" in
// the "FileSystemURL" class name is part of its history but now inaccurate.
//
//
// # Cracking
//
// `FileSystemURL`s can wrap (provide an alternative name for) real (kernel
// visible) files, virtual files or even other `FileSystemURL`s. Cracking is
// the process of recursively unwrapping the outer layers to recover a name or
// identifier for the underlying, inner-most resource.
//
// Cracking is a concept at the C++ API level but is not part of the JS API. It
// applies when the original URL's `mount_type()` identifies a `MountPoints`
// subclass (`ExternalMountPoints` or `IsolatedContext`); its `virtual_path()`
// can then be further resolved to a lower level `type()` and `path()`.
//
// When recursion occurs, also known as having multiple rounds of cracking,
// then `mount_filesystem_id()` and `filesystem_id()` return information from
// the outer-most (first round) and inner-most (last round). If there is only
// one round of cracking then the two strings should be equal. When cracking
// does not apply then these strings should be empty.
//
// Permission checking on the top-level mount information should use
// `mount_filesystem_id()`. Low-level operations should use `filesystem_id()`,
// as well as the `type()`, `path()` and `mount_option()` that also come from
// the last round of cracking.
//
// Cracking is done at `FileSystemURL`-constructor time, although the
// non-trivial `FileSystemURL` constructors are private. Outside of test code,
// use the `<Friend>::CrackURL` or `<Friend>::CreateCrackedFileSystemURL`
// factory methods, where `<Friend>` is one of the friended classes.
//
//
// # Accessors
//
// For example, parsing "filesystem:http://example.com/temporary/bar/qux":
//  - `origin()` returns "http://example.com"
//  - `mount_type()` returns kFileSystemTypeTemporary
//  - `virtual_path()` returns "bar/qux"
//  - `type()` returns the same value as `mount_type()`
//  - `path()` returns the same value as `virtual_path()`
//
// For example, if path "/media/removable" is mounted at "my_mount_name" with
// type kFileSystemTypeFoo as an external file system, then parsing and
// cracking "filesystem:chrome://file-manager/external/my_mount_name/x/y":
//  - `origin()` returns "chrome://file-manager"
//  - `mount_type()` returns kFileSystemTypeExternal
//  - `virtual_path()` returns "my_mount_name/x/y"
//  - `type()` returns the kFileSystemTypeFoo
//  - `path()` returns "/media/removable/x/y"
//
// Additionally:
//  - `filesystem_id()` returns "my_mount_name".
//
// The naming is unfortunate, but URL paths (what `GURL::path()` returns) are
// not the same as what `FileSystemURL::path()` returns. The latter is not
// necessarily a 'substring' of the `FileSystemURL`'s string form.
// `FileSystemURL::path()` often locates a real file on the kernel-level file
// system but it does not have to and sometimes it's just a string identifier
// (presented in C++ as a `base::FilePath`) whose meaning depends on the
// `FileSystemURL::type()`. See the `TypeImpliesPathIsReal()` method.
//
// For example, `kFileSystemTypeProvided` (which corresponds to the
// https://developer.chrome.com/docs/extensions/reference/fileSystemProvider/
// JS API) does not serve real files. For that type, `path()` returns something
// like "/provided/extensionid:filesystemid:userhash/a/b/c.txt", where
// "/provided" is `ash::file_system_provider::util::kProvidedMountPointRoot`.
// The serving code path goes through the `ExternalMountPoints` class, but
// there's no "provided" directory in the root of the real file system.
//
//
// # Known Issues
//
// TODO(crbug.com/41454906): Look into making `virtual_path()` [and all
// FileSystem API virtual paths] just a `std::string`, to prevent platform-
// specific `base::FilePath` behavior from getting invoked by accident.
// Currently the `base::FilePath` returned here needs special treatment, as it
// may contain paths that are illegal on the current platform.
//
// To avoid problems, use `VirtualPath::BaseName` and
// `VirtualPath::GetComponents` instead of the `base::FilePath` methods.
class COMPONENT_EXPORT(STORAGE_BROWSER) FileSystemURL {
 public:
  FileSystemURL();

  FileSystemURL(const FileSystemURL&);
  FileSystemURL(FileSystemURL&&) noexcept;
  FileSystemURL& operator=(const FileSystemURL&);
  FileSystemURL& operator=(FileSystemURL&&) noexcept;

  ~FileSystemURL();

  // Returns a new `FileSystemURL` that is a sibling (it has the same parent
  // directory) to this one. Starting from "filesystem:etc/foo/bar", it
  // produces "filesystem:etc/foo/sibling_name".
  //
  // Its `virtual_path()` and `path()` are derived from this `FileSystemURL`'s
  // `virtual_path()` and `path()`, with the base names changed over. The
  // `path()` is unchanged if empty. All other fields (whether parsed from the
  // URL string form or obtained from cracking) are copied from this.
  //
  // It returns an invalid `FileSystemURL` if `!is_valid()` or if `path()` is
  // non-empty and its base name does not match the `virtual_path()` base name.
  FileSystemURL CreateSibling(const base::SafeBaseName& sibling_name) const;

  // Methods for creating `FileSystemURL` without attempting to crack them.
  // Should be used only in tests.
  static FileSystemURL CreateForTest(const GURL& url);
  static FileSystemURL CreateForTest(const blink::StorageKey& storage_key,
                                     FileSystemType mount_type,
                                     const base::FilePath& virtual_path);
  static FileSystemURL CreateForTest(const blink::StorageKey& storage_key,
                                     FileSystemType mount_type,
                                     const base::FilePath& virtual_path,
                                     const std::string& mount_filesystem_id,
                                     FileSystemType cracked_type,
                                     const base::FilePath& cracked_path,
                                     const std::string& filesystem_id,
                                     const FileSystemMountOption& mount_option);

  // Whether a `FileSystemURL`'s `path()` refers to a real (kernel visible, can
  // be passed to https://man7.org/linux/man-pages/man2/open.2.html) file,
  // instead of being a string identifier in `base::FilePath` clothing.
  //
  // Must be called on a `FileSystemURL`'s `type()`. Do not call this method
  // with a `mount_type()`.
  static bool TypeImpliesPathIsReal(FileSystemType type);
  // Instance method is provided for convenience.
  bool TypeImpliesPathIsReal() const { return TypeImpliesPathIsReal(type()); }

  // Returns true if this instance represents a valid `FileSystemURL`.
  bool is_valid() const { return is_valid_; }

  // Returns the storage key. See the class comment for details.
  const blink::StorageKey& storage_key() const { return storage_key_; }

  // Returns the origin part of this URL. See the class comment for details.
  const url::Origin& origin() const { return storage_key_.origin(); }

  // Returns the type part of this URL. See the class comment for details.
  FileSystemType type() const { return type_; }

  // Returns the cracked path of this URL. See the class comment for details.
  const base::FilePath& path() const { return path_; }

  // Returns the original path part of this URL.
  // See the class comment for details.
  // TODO(crbug.com/41454906): this must return std::string.
  const base::FilePath& virtual_path() const { return virtual_path_; }

  // Returns the filesystem ID/mount name for isolated/external filesystem URLs.
  // See the class comment for details.
  const std::string& filesystem_id() const { return filesystem_id_; }
  const std::string& mount_filesystem_id() const {
    return mount_filesystem_id_;
  }

  FileSystemType mount_type() const { return mount_type_; }

  const FileSystemMountOption& mount_option() const { return mount_option_; }

  // Returns the `BucketLocator` for this URL's partitioned file location. In
  // the majority of cases, this will not be populated and the default storage
  // bucket will be used.
  const std::optional<BucketLocator>& bucket() const { return bucket_; }
  void SetBucket(const BucketLocator& bucket) { bucket_ = bucket; }

  // Returns either `bucket_` or a `BucketLocator` corresponding to the default
  // bucket for `storage_key_`.
  BucketLocator GetBucket() const;

  // Returns the formatted URL of this instance.
  GURL ToGURL() const;

  std::string DebugString() const;

  // Returns true if this URL is a strict parent of the `child`.
  bool IsParent(const FileSystemURL& child) const;

  bool IsInSameFileSystem(const FileSystemURL& other) const;

  bool operator==(const FileSystemURL& that) const;

  bool operator!=(const FileSystemURL& that) const { return !(*this == that); }

  struct COMPONENT_EXPORT(STORAGE_BROWSER) Comparator {
    bool operator()(const FileSystemURL& lhs, const FileSystemURL& rhs) const;
  };

 private:
  friend class FileSystemContext;
  friend class ExternalMountPoints;
  friend class IsolatedContext;

  FileSystemURL(const GURL& filesystem_url,
                const blink::StorageKey& storage_key);
  FileSystemURL(const blink::StorageKey& storage_key,
                FileSystemType mount_type,
                const base::FilePath& virtual_path);
  // Creates a cracked `FileSystemURL`.
  FileSystemURL(const blink::StorageKey& storage_key,
                FileSystemType mount_type,
                const base::FilePath& virtual_path,
                const std::string& mount_filesystem_id,
                FileSystemType cracked_type,
                const base::FilePath& cracked_path,
                const std::string& filesystem_id,
                const FileSystemMountOption& mount_option);

  // Used to determine if a `FileSystemURL` was default constructed.
  bool is_null_ = false;

  bool is_valid_;

  // Fields parsed from the original URL (the string form), although the
  // `url::Origin` has grown into a `blink::StorageKey`.
  blink::StorageKey storage_key_;
  FileSystemType mount_type_;
  base::FilePath virtual_path_;

  // Fields obtained from cracking.
  //
  // `mount_filesystem_id_` is set from the first round of cracking. The other
  // fields are set from recursive cracking (the last round).
  std::string mount_filesystem_id_;
  FileSystemType type_;
  base::FilePath path_;
  std::string filesystem_id_;
  FileSystemMountOption mount_option_;

  // Fields that must be explicitly set separately.
  std::optional<BucketLocator> bucket_;
};

using FileSystemURLSet = std::set<FileSystemURL, FileSystemURL::Comparator>;

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_H_
