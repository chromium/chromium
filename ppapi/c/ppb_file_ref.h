/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_file_ref.idl modified Wed Jan 29 20:50:29 2014. */

#ifndef PPAPI_C_PPB_FILE_REF_H_
#define PPAPI_C_PPB_FILE_REF_H_

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"

#define PPB_FILEREF_INTERFACE_1_0 "PPB_FileRef;1.0"
#define PPB_FILEREF_INTERFACE_1_1 "PPB_FileRef;1.1"
#define PPB_FILEREF_INTERFACE_1_2 "PPB_FileRef;1.2"
#define PPB_FILEREF_INTERFACE PPB_FILEREF_INTERFACE_1_2

/**
 * @file
 * This file defines the API to create a file reference or "weak pointer" to a
 * file in a file system.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * The <code>PP_MakeDirectoryFlags</code> enum contains flags used to control
 * behavior of <code>PPB_FileRef.MakeDirectory()</code>.
 */
typedef enum {
  PP_MAKEDIRECTORYFLAG_NONE = 0 << 0,
  /** Requests that ancestor directories are created if they do not exist. */
  PP_MAKEDIRECTORYFLAG_WITH_ANCESTORS = 1 << 0,
  /**
   * Requests that the PPB_FileRef.MakeDirectory() call fails if the directory
   * already exists.
   */
  PP_MAKEDIRECTORYFLAG_EXCLUSIVE = 1 << 1
} PP_MakeDirectoryFlags;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_FileRef</code> struct represents a "weak pointer" to a file in
 * a file system.  This struct contains a <code>PP_FileSystemType</code>
 * identifier and a file path string.
 */
struct PPB_FileRef_1_2 {
  /**
   * Create() creates a weak pointer to a file in the given file system. File
   * paths are POSIX style.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a file
   * system.
   * @param[in] path A path to the file. Must begin with a '/' character.
   *
   * @return A <code>PP_Resource</code> corresponding to a file reference if
   * successful or 0 if the path is malformed.
   */
  PP_Resource (*Create)(PP_Resource file_system, const char* path);
  /**
   * IsFileRef() determines if the provided resource is a file reference.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>PPB_FileRef</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some type other than <code>PPB_FileRef</code>.
   */
  PP_Bool (*IsFileRef)(PP_Resource resource);
  /**
   * GetFileSystemType() returns the type of the file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_FileSystemType</code> with the file system type if
   * valid or <code>PP_FILESYSTEMTYPE_INVALID</code> if the provided resource
   * is not a valid file reference.
   */
  PP_FileSystemType (*GetFileSystemType)(PP_Resource file_ref);
  /**
   * GetName() returns the name of the file.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Var</code> containing the name of the file.  The value
   * returned by this function does not include any path components (such as
   * the name of the parent directory, for example). It is just the name of the
   * file. Use GetPath() to get the full file path.
   */
  struct PP_Var (*GetName)(PP_Resource file_ref);
  /**
   * GetPath() returns the absolute path of the file.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Var</code> containing the absolute path of the file.
   * This function fails if the file system type is
   * <code>PP_FileSystemType_External</code>.
   */
  struct PP_Var (*GetPath)(PP_Resource file_ref);
  /**
   * GetParent() returns the parent directory of this file.  If
   * <code>file_ref</code> points to the root of the filesystem, then the root
   * is returned.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   *
   * @return A <code>PP_Resource</code> containing the parent directory of the
   * file. This function fails if the file system type is
   * <code>PP_FileSystemType_External</code>.
   */
  PP_Resource (*GetParent)(PP_Resource file_ref);
  /**
   * MakeDirectory() makes a new directory in the file system according to the
   * given <code>make_directory_flags</code>, which is a bit-mask of the
   * <code>PP_MakeDirectoryFlags</code> values.  It is not valid to make a
   * directory in the external file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] make_directory_flags A bit-mask of the
   * <code>PP_MakeDirectoryFlags</code> values.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of MakeDirectory().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*MakeDirectory)(PP_Resource directory_ref,
                           int32_t make_directory_flags,
                           struct PP_CompletionCallback callback);
  /**
   * Touch() Updates time stamps for a file.  You must have write access to the
   * file if it exists in the external filesystem.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] last_access_time The last time the file was accessed.
   * @param[in] last_modified_time The last time the file was modified.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Touch().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Touch)(PP_Resource file_ref,
                   PP_Time last_access_time,
                   PP_Time last_modified_time,
                   struct PP_CompletionCallback callback);
  /**
   * Delete() deletes a file or directory. If <code>file_ref</code> refers to
   * a directory, then the directory must be empty. It is an error to delete a
   * file or directory that is in use.  It is not valid to delete a file in
   * the external file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Delete().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Delete)(PP_Resource file_ref,
                    struct PP_CompletionCallback callback);
  /**
   * Rename() renames a file or directory.  Arguments <code>file_ref</code> and
   * <code>new_file_ref</code> must both refer to files in the same file
   * system. It is an error to rename a file or directory that is in use.  It
   * is not valid to rename a file in the external file system.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] new_file_ref A <code>PP_Resource</code> corresponding to a new
   * file reference.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Rename().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Rename)(PP_Resource file_ref,
                    PP_Resource new_file_ref,
                    struct PP_CompletionCallback callback);
  /**
   * Query() queries info about a file or directory. You must have access to
   * read this file or directory if it exists in the external filesystem.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[out] info A pointer to a <code>PP_FileInfo</code> which will be
   * populated with information about the file or directory.
   * @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
   * completion of Query().
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*Query)(PP_Resource file_ref,
                   struct PP_FileInfo* info,
                   struct PP_CompletionCallback callback);
  /**
   * ReadDirectoryEntries() reads all entries in a directory.
   *
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a directory
   * reference.
   * @param[in] output An output array which will receive
   * <code>PP_DirectoryEntry</code> objects on success.
   * @param[in] callback A <code>PP_CompletionCallback</code> to run on
   * completion.
   *
   * @return An int32_t containing an error code from <code>pp_errors.h</code>.
   */
  int32_t (*ReadDirectoryEntries)(PP_Resource file_ref,
                                  struct PP_ArrayOutput output,
                                  struct PP_CompletionCallback callback);
};

typedef struct PPB_FileRef_1_2 PPB_FileRef;

struct PPB_FileRef_1_0 {
  PP_Resource (*Create)(PP_Resource file_system, const char* path);
  PP_Bool (*IsFileRef)(PP_Resource resource);
  PP_FileSystemType (*GetFileSystemType)(PP_Resource file_ref);
  struct PP_Var (*GetName)(PP_Resource file_ref);
  struct PP_Var (*GetPath)(PP_Resource file_ref);
  PP_Resource (*GetParent)(PP_Resource file_ref);
  int32_t (*MakeDirectory)(PP_Resource directory_ref,
                           PP_Bool make_ancestors,
                           struct PP_CompletionCallback callback);
  int32_t (*Touch)(PP_Resource file_ref,
                   PP_Time last_access_time,
                   PP_Time last_modified_time,
                   struct PP_CompletionCallback callback);
  int32_t (*Delete)(PP_Resource file_ref,
                    struct PP_CompletionCallback callback);
  int32_t (*Rename)(PP_Resource file_ref,
                    PP_Resource new_file_ref,
                    struct PP_CompletionCallback callback);
};

struct PPB_FileRef_1_1 {
  PP_Resource (*Create)(PP_Resource file_system, const char* path);
  PP_Bool (*IsFileRef)(PP_Resource resource);
  PP_FileSystemType (*GetFileSystemType)(PP_Resource file_ref);
  struct PP_Var (*GetName)(PP_Resource file_ref);
  struct PP_Var (*GetPath)(PP_Resource file_ref);
  PP_Resource (*GetParent)(PP_Resource file_ref);
  int32_t (*MakeDirectory)(PP_Resource directory_ref,
                           PP_Bool make_ancestors,
                           struct PP_CompletionCallback callback);
  int32_t (*Touch)(PP_Resource file_ref,
                   PP_Time last_access_time,
                   PP_Time last_modified_time,
                   struct PP_CompletionCallback callback);
  int32_t (*Delete)(PP_Resource file_ref,
                    struct PP_CompletionCallback callback);
  int32_t (*Rename)(PP_Resource file_ref,
                    PP_Resource new_file_ref,
                    struct PP_CompletionCallback callback);
  int32_t (*Query)(PP_Resource file_ref,
                   struct PP_FileInfo* info,
                   struct PP_CompletionCallback callback);
  int32_t (*ReadDirectoryEntries)(PP_Resource file_ref,
                                  struct PP_ArrayOutput output,
                                  struct PP_CompletionCallback callback);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_FILE_REF_H_ */

