// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/file_ref.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/directory_entry.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileRef_1_0>() {
  return PPB_FILEREF_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_FileRef_1_1>() {
  return PPB_FILEREF_INTERFACE_1_1;
}

template <> const char* interface_name<PPB_FileRef_1_2>() {
  return PPB_FILEREF_INTERFACE_1_2;
}

}  // namespace

FileRef::FileRef(PP_Resource resource) : Resource(resource) {
}

FileRef::FileRef(PassRef, PP_Resource resource) : Resource(PASS_REF, resource) {
}

FileRef::FileRef(const FileSystem& file_system,
                 const char* path) {
  if (has_interface<PPB_FileRef_1_2>()) {
    PassRefFromConstructor(get_interface<PPB_FileRef_1_2>()->Create(
        file_system.pp_resource(), path));
  } else if (has_interface<PPB_FileRef_1_1>()) {
    PassRefFromConstructor(get_interface<PPB_FileRef_1_1>()->Create(
        file_system.pp_resource(), path));
  } else if (has_interface<PPB_FileRef_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_FileRef_1_0>()->Create(
        file_system.pp_resource(), path));
  }
}

FileRef::FileRef(const FileRef& other) : Resource(other) {}

FileRef& FileRef::operator=(const FileRef& other) {
  Resource::operator=(other);
  return *this;
}

PP_FileSystemType FileRef::GetFileSystemType() const {
  if (has_interface<PPB_FileRef_1_2>())
    return get_interface<PPB_FileRef_1_2>()->GetFileSystemType(pp_resource());
  if (has_interface<PPB_FileRef_1_1>())
    return get_interface<PPB_FileRef_1_1>()->GetFileSystemType(pp_resource());
  if (has_interface<PPB_FileRef_1_0>())
    return get_interface<PPB_FileRef_1_0>()->GetFileSystemType(pp_resource());
  return PP_FILESYSTEMTYPE_EXTERNAL;
}

Var FileRef::GetName() const {
  if (has_interface<PPB_FileRef_1_2>()) {
    return Var(PASS_REF,
               get_interface<PPB_FileRef_1_2>()->GetName(pp_resource()));
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return Var(PASS_REF,
               get_interface<PPB_FileRef_1_1>()->GetName(pp_resource()));
  }
  if (has_interface<PPB_FileRef_1_0>()) {
    return Var(PASS_REF,
               get_interface<PPB_FileRef_1_0>()->GetName(pp_resource()));
  }
  return Var();
}

Var FileRef::GetPath() const {
  if (has_interface<PPB_FileRef_1_2>()) {
    return Var(PASS_REF,
               get_interface<PPB_FileRef_1_2>()->GetPath(pp_resource()));
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return Var(PASS_REF,
               get_interface<PPB_FileRef_1_1>()->GetPath(pp_resource()));
  }
  if (has_interface<PPB_FileRef_1_0>()) {
    return Var(PASS_REF,
               get_interface<PPB_FileRef_1_0>()->GetPath(pp_resource()));
  }
  return Var();
}

FileRef FileRef::GetParent() const {
  if (has_interface<PPB_FileRef_1_2>()) {
    return FileRef(PASS_REF,
                   get_interface<PPB_FileRef_1_2>()->GetParent(pp_resource()));
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return FileRef(PASS_REF,
                   get_interface<PPB_FileRef_1_1>()->GetParent(pp_resource()));
  }
  if (has_interface<PPB_FileRef_1_0>()) {
    return FileRef(PASS_REF,
                   get_interface<PPB_FileRef_1_0>()->GetParent(pp_resource()));
  }
  return FileRef();
}

int32_t FileRef::MakeDirectory(int32_t make_directory_flags,
                               const CompletionCallback& cc) {
  if (has_interface<PPB_FileRef_1_2>()) {
    return get_interface<PPB_FileRef_1_2>()->MakeDirectory(
        pp_resource(),
        make_directory_flags,
        cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    if (make_directory_flags & ~PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS)
      return cc.MayForce(PP_ERROR_NOTSUPPORTED);
    return get_interface<PPB_FileRef_1_1>()->MakeDirectory(
        pp_resource(),
        PP_FromBool(make_directory_flags & PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS),
        cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_0>()) {
    if (make_directory_flags & ~PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS)
      return cc.MayForce(PP_ERROR_NOTSUPPORTED);
    return get_interface<PPB_FileRef_1_0>()->MakeDirectory(
        pp_resource(),
        PP_FromBool(make_directory_flags & PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS),
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileRef::Touch(PP_Time last_access_time,
                       PP_Time last_modified_time,
                       const CompletionCallback& cc) {
  if (has_interface<PPB_FileRef_1_2>()) {
    return get_interface<PPB_FileRef_1_2>()->Touch(
        pp_resource(), last_access_time, last_modified_time,
        cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return get_interface<PPB_FileRef_1_1>()->Touch(
        pp_resource(), last_access_time, last_modified_time,
        cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_0>()) {
    return get_interface<PPB_FileRef_1_0>()->Touch(
        pp_resource(), last_access_time, last_modified_time,
        cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileRef::Delete(const CompletionCallback& cc) {
  if (has_interface<PPB_FileRef_1_2>()) {
    return get_interface<PPB_FileRef_1_2>()->Delete(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return get_interface<PPB_FileRef_1_1>()->Delete(
        pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_0>()) {
    return get_interface<PPB_FileRef_1_0>()->Delete(
        pp_resource(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileRef::Rename(const FileRef& new_file_ref,
                        const CompletionCallback& cc) {
  if (has_interface<PPB_FileRef_1_2>()) {
    return get_interface<PPB_FileRef_1_2>()->Rename(
        pp_resource(), new_file_ref.pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return get_interface<PPB_FileRef_1_1>()->Rename(
        pp_resource(), new_file_ref.pp_resource(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_0>()) {
    return get_interface<PPB_FileRef_1_0>()->Rename(
        pp_resource(), new_file_ref.pp_resource(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileRef::Query(const CompletionCallbackWithOutput<PP_FileInfo>& cc) {
  if (has_interface<PPB_FileRef_1_2>()) {
    return get_interface<PPB_FileRef_1_2>()->Query(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return get_interface<PPB_FileRef_1_1>()->Query(
        pp_resource(), cc.output(), cc.pp_completion_callback());
  }
  return cc.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t FileRef::ReadDirectoryEntries(
    const CompletionCallbackWithOutput<std::vector<DirectoryEntry> >&
        callback) {
  if (has_interface<PPB_FileRef_1_2>()) {
    return get_interface<PPB_FileRef_1_2>()->ReadDirectoryEntries(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  if (has_interface<PPB_FileRef_1_1>()) {
    return get_interface<PPB_FileRef_1_1>()->ReadDirectoryEntries(
        pp_resource(), callback.output(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
