// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_REF_H_
#define PPAPI_CPP_FILE_REF_H_

#include <vector>

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API to create a file reference or "weak pointer" to a
/// file in a file system.

namespace pp {

class DirectoryEntry;
class FileSystem;
class CompletionCallback;
template <typename T> class CompletionCallbackWithOutput;

/// The <code>FileRef</code> class represents a "weak pointer" to a file in
/// a file system.
class FileRef : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>FileRef</code>
  /// object.
  FileRef() {}

  /// A constructor used when you have an existing PP_Resource for a FileRef
  /// and which to create a C++ object that takes an additional reference to
  /// the resource.
  ///
  /// @param[in] resource A PP_Resource corresponding to file reference.
  explicit FileRef(PP_Resource resource);

  /// A constructor used when you have received a PP_Resource as a return
  /// value that has already been reference counted.
  ///
  /// @param[in] resource A PP_Resource corresponding to file reference.
  FileRef(PassRef, PP_Resource resource);

  /// A constructor that creates a weak pointer to a file in the given file
  /// system. File paths are POSIX style.
  ///
  /// If the <code>path</code> is malformed, the resulting <code>FileRef</code>
  /// will have a null <code>PP_Resource</code>.
  ///
  /// @param[in] file_system A <code>FileSystem</code> corresponding to a file
  /// system type.
  /// @param[in] path A path to the file. Must begin with a '/' character.
  FileRef(const FileSystem& file_system, const char* path);

  /// The copy constructor for <code>FileRef</code>.
  ///
  /// @param[in] other A pointer to a <code>FileRef</code>.
  FileRef(const FileRef& other);
  FileRef& operator=(const FileRef& other);

  /// GetFileSystemType() returns the type of the file system.
  ///
  /// @return A <code>PP_FileSystemType</code> with the file system type if
  /// valid or <code>PP_FILESYSTEMTYPE_INVALID</code> if the provided resource
  /// is not a valid file reference.
  PP_FileSystemType GetFileSystemType() const;

  /// GetName() returns the name of the file.
  ///
  /// @return A <code>Var</code> containing the name of the file.  The value
  /// returned by this function does not include any path components (such as
  /// the name of the parent directory, for example). It is just the name of the
  /// file. Use GetPath() to get the full file path.
  Var GetName() const;

  /// GetPath() returns the absolute path of the file.
  ///
  /// @return A <code>Var</code> containing the absolute path of the file.
  /// This function fails if the file system type is
  /// <code>PP_FileSystemType_External</code>.
  Var GetPath() const;

  /// GetParent() returns the parent directory of this file.  If
  /// <code>file_ref</code> points to the root of the filesystem, then the root
  /// is returned.
  ///
  /// @return A <code>FileRef</code> containing the parent directory of the
  /// file. This function fails if the file system type is
  /// <code>PP_FileSystemType_External</code>.
  FileRef GetParent() const;

  /// MakeDirectory() makes a new directory in the file system according to the
  /// given <code>make_directory_flags</code>, which is a bit-mask of the
  /// <code>PP_MakeDirectoryFlags</code> values.  It is not valid to make a
  /// directory in the external file system.
  ///
  /// @param[in] make_directory_flags A bit-mask of the
  /// <code>PP_MakeDirectoryFlags</code> values.
  /// See <code>ppb_file_ref.h</code> for more details.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of MakeDirectory().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t MakeDirectory(int32_t make_directory_flags,
                        const CompletionCallback& cc);

  /// Touch() Updates time stamps for a file.  You must have write access to the
  /// file if it exists in the external filesystem.
  ///
  /// @param[in] last_access_time The last time the file was accessed.
  /// @param[in] last_modified_time The last time the file was modified.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Touch().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                const CompletionCallback& cc);

  /// Delete() deletes a file or directory. If <code>file_ref</code> refers to
  /// a directory, then the directory must be empty. It is an error to delete a
  /// file or directory that is in use.  It is not valid to delete a file in
  /// the external file system.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Delete().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Delete(const CompletionCallback& cc);

  /// Rename() renames a file or directory. Argument <code>new_file_ref</code>
  /// must refer to files in the same file system as in this object. It is an
  /// error to rename a file or directory that is in use.  It is not valid to
  /// rename a file in the external file system.
  ///
  /// @param[in] new_file_ref A <code>FileRef</code> corresponding to a new
  /// file reference.
  /// @param[in] cc A <code>CompletionCallback</code> to be called upon
  /// completion of Rename().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Rename(const FileRef& new_file_ref, const CompletionCallback& cc);

  /// Query() queries info about a file or directory. You must have access to
  /// read this file or directory if it exists in the external filesystem.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code>
  /// to be called upon completion of Query().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Query(const CompletionCallbackWithOutput<PP_FileInfo>& callback);

  /// ReadDirectoryEntries() Reads all entries in the directory.
  ///
  /// @param[in] cc A <code>CompletionCallbackWithOutput</code> to be called
  /// upon completion of ReadDirectoryEntries(). On success, the
  /// directory entries will be passed to the given function.
  ///
  /// Normally you would use a CompletionCallbackFactory to allow callbacks to
  /// be bound to your class. See completion_callback_factory.h for more
  /// discussion on how to use this. Your callback will generally look like:
  ///
  /// @code
  ///   void OnReadDirectoryEntries(
  ///       int32_t result,
  ///       const std::vector<DirectoryEntry>& entries) {
  ///     if (result == PP_OK)
  ///       // use entries...
  ///   }
  /// @endcode
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t ReadDirectoryEntries(
      const CompletionCallbackWithOutput< std::vector<DirectoryEntry> >&
          callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_REF_H_
