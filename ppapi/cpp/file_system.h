// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_SYSTEM_H_
#define PPAPI_CPP_FILE_SYSTEM_H_

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create a file system associated with a file.

namespace pp {

class CompletionCallback;

/// The <code>FileSystem</code> class identifies the file system type
/// associated with a file.
class FileSystem : public Resource {
 public:
  /// Constructs an is_null() filesystem resource. If you use this constructor,
  /// you will have to assign it to a "real" FileSystem object before you can
  /// use it.
  FileSystem();

  /// The copy constructor for <code>FileSystem</code>.
  ///
  /// @param[in] other A reference to a <code>FileSystem</code>.
  FileSystem(const FileSystem& other);

  /// Constructs a <code>FileSystem</code> from a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>Resource</code> containing a file system.
  explicit FileSystem(const Resource& resource);

  /// A constructor used when you have received a PP_Resource as a return
  /// value that has already been reference counted.
  ///
  /// @param[in] resource A PP_Resource corresponding to a PPB_FileSystem.
  FileSystem(PassRef, PP_Resource resource);

  /// This constructor creates a file system object of the given type.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  ///
  /// @param[in] type A file system type as defined by
  /// <code>PP_FileSystemType</code> enum.
  FileSystem(const InstanceHandle& instance, PP_FileSystemType type);

  /// Open() opens the file system. A file system must be opened before running
  /// any other operation on it.
  ///
  /// @param[in] expected_size The expected size of the file system. Note that
  /// this does not request quota; to do that, you must either invoke
  /// requestQuota from JavaScript:
  /// http://www.html5rocks.com/en/tutorials/file/filesystem/#toc-requesting-quota
  /// or set the unlimitedStorage permission for Chrome Web Store apps:
  /// http://code.google.com/chrome/extensions/manifest.html#permissions
  ///
  /// @param[in] cc A <code>PP_CompletionCallback</code> to be called upon
  /// completion of Open().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Open(int64_t expected_size, const CompletionCallback& cc);

  /// Checks whether a <code>Resource</code> is a file system, to test whether
  /// it is appropriate for use with the <code>FileSystem</code> constructor.
  ///
  /// @param[in] resource A <code>Resource</code> to test.
  ///
  /// @return True if <code>resource</code> is a file system.
  static bool IsFileSystem(const Resource& resource);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_SYSTEM_H_
