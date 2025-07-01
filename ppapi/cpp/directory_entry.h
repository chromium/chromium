// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DIRECTORY_ENTRY_H_
#define PPAPI_CPP_DIRECTORY_ENTRY_H_

#include <vector>

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_directory_entry.h"
#include "ppapi/cpp/array_output.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/output_traits.h"
#include "ppapi/cpp/pass_ref.h"

/// @file
/// This file defines the API used to handle a directory entry.

namespace pp {

/// The <code>DirectoryEntry</code> class represents information about
/// a directory entry.
class DirectoryEntry {
 public:
  /// Default constructor for creating an is_null() <code>DirectoryEntry</code>
  /// object.
  DirectoryEntry();

  /// A constructor used when you have a <code>PP_DirectoryEntry</code> which
  /// contains a <code>FileRef</code> that has already been reference counted
  /// as a return value.
  ///
  /// @param[in] data A <code>PP_DirectoryEntry</code> to be copied.
  DirectoryEntry(PassRef, const PP_DirectoryEntry& data);

  /// A copy constructor for <code>DirectoryEntry</code>. This constructor
  /// increments a reference count of the <code>FileRef</code> held by this
  /// DirectoryEntry.
  ///
  /// @param[in] other A pointer to a <code>DirectoryEntry</code>.
  DirectoryEntry(const DirectoryEntry& other);

  /// A destructor that decrements a reference count of the <code>FileRef</code>
  /// held by this <code>DirectoryEntry</code>.
  ~DirectoryEntry();

  /// This function assigns one <code>DirectoryEntry</code> object to this
  /// <code>DirectoryEntry</code> object. This function increases the reference
  /// count of the <code>FileRef</code> of the other DirectoryEntry while
  /// decrementing the reference count of the FileRef of this DirectoryEntry.
  ///
  /// @param[in] other A pointer to a <code>DirectoryEntry</code>.
  ///
  /// @return A new <code>DirectoryEntry</code> object.
  DirectoryEntry& operator=(const DirectoryEntry& other);

  /// This function determines if this <code>DirectoryEntry</code> is a null
  /// value.
  ///
  /// @return true if this <code>DirectoryEntry</code> is null, otherwise false.
  bool is_null() const { return !data_.file_ref; }

  /// This function returns the <code>FileRef</code> held by this
  /// <code>DirectoryEntry</code>.
  ///
  /// @return A <code>FileRef</code> of the file.
  FileRef file_ref() const { return FileRef(data_.file_ref); }

  /// This function returns the <code>PP_FileType</code> of the file referenced
  /// by this <code>DirectoryEntry</code>.
  ///
  /// @return A <code>PP_FileType</code> of the file.
  PP_FileType file_type() const { return data_.file_type; }

 private:
  PP_DirectoryEntry data_;
};

namespace internal {

class DirectoryEntryArrayOutputAdapterWithStorage
    : public ArrayOutputAdapter<PP_DirectoryEntry> {
 public:
  DirectoryEntryArrayOutputAdapterWithStorage();
  virtual ~DirectoryEntryArrayOutputAdapterWithStorage();

  // Returns the final array of resource objects, converting the
  // PP_DirectoryEntry written by the browser to pp::DirectoryEntry
  // objects.
  //
  // This function should only be called once or we would end up converting
  // the array more than once, which would mess up the refcounting.
  std::vector<DirectoryEntry>& output();

 private:
  // The browser will write the PP_DirectoryEntrys into this array.
  std::vector<PP_DirectoryEntry> temp_storage_;

  // When asked for the output, the PP_DirectoryEntrys above will be
  // converted to the pp::DirectoryEntrys in this array for passing to the
  // calling code.
  std::vector<DirectoryEntry> output_storage_;
};

// A specialization of CallbackOutputTraits to provide the callback system the
// information on how to handle vectors of pp::DirectoryEntry. This converts
// PP_DirectoryEntry to pp::DirectoryEntry when passing to the plugin.
template <>
struct CallbackOutputTraits< std::vector<DirectoryEntry> > {
  typedef PP_ArrayOutput APIArgType;
  typedef DirectoryEntryArrayOutputAdapterWithStorage StorageType;

  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return t.pp_array_output();
  }

  static inline std::vector<DirectoryEntry>& StorageToPluginArg(
      StorageType& t) {
    return t.output();
  }

  static inline void Initialize(StorageType* /* t */) {}
};

}  // namespace internal
}  // namespace pp

#endif  // PPAPI_CPP_DIRECTORY_ENTRY_H_
