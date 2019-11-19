// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_H
#define CRAZY_LINKER_H

// This is the crazy linker, a custom dynamic linker that can be used
// by NDK applications to load shared libraries (not executables) with
// a twist.
//
// Compared to the dynamic linker, the crazy one has the following
// features:
//
//   - It can use an arbitrary search path.
//
//   - It can load a library at a memory fixed address.
//
//   - It can load libraries from zip archives (as long as they are
//     page aligned and uncompressed). Even when running on pre-Android M
//     systems.
//
//   - It can share the RELRO section between two libraries
//     loaded at the same address in two distinct processes.
//
#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function attribute to indicate that it needs to be exported by
// the library.
#define _CRAZY_PUBLIC __attribute__((__visibility__("default")))

// Maximum path length of a file in a zip file.
const size_t kMaxFilePathLengthInZip = 256;

// Status values returned by crazy linker functions to indicate
// success or failure. They were chosen to match boolean values,
// this allows one to test for failures with:
//
//    if (!crazy_linker_function(....)) {
//       ... an error occured.
//    }
//
// If the function called used a crazy_context_t, it is possible to
// retrieve the error details with crazy_context_get_error().
typedef enum {
  CRAZY_STATUS_FAILURE = 0,
  CRAZY_STATUS_SUCCESS = 1
} crazy_status_t;

// Opaque handle to a context object that will hold parameters
// for the crazy linker's operations. For example, this is where
// you would set the explicit load address, and other user-provided
// values before calling functions like crazy_library_open().
//
// The context holds a list of library search paths, initialized to
// the content of the LD_LIBRARY_PATH variable on creation.
//
// The context also holds a string buffer to hold error messages that
// can be queried with crazy_context_get_error().
typedef struct crazy_context_t crazy_context_t;

// Create a new context object.
crazy_context_t* crazy_context_create(void) _CRAZY_PUBLIC;

// Return current error string, or NULL if there was no error.
const char* crazy_context_get_error(crazy_context_t* context) _CRAZY_PUBLIC;

// Clear error in a given context.
void crazy_context_clear_error(crazy_context_t* context) _CRAZY_PUBLIC;

// Set the explicit load address in a context object. Value 0 means
// the address is randomized. NOTE: This will achieve a best-effort load,
// if the address range is reserved, the library will still be loaded at
// a different address. Use crazy_context_set_reserved_map() if you want
// to ensure that the library can only be loaded at a fixed address.
void crazy_context_set_load_address(crazy_context_t* context,
                                    size_t load_address) _CRAZY_PUBLIC;

// Return the current load address in a context.
size_t crazy_context_get_load_address(crazy_context_t* context) _CRAZY_PUBLIC;

// Set the explicit library file descriptor in a context object. Values >= 0
// will be used during the next crazy_library_open() call to read the library
// file, instead of opening it using its path.
void crazy_context_set_library_fd(crazy_context_t* context,
                                  int fd) _CRAZY_PUBLIC;

// Return the current library file descriptor in a context.
int crazy_context_get_library_fd(crazy_context_t* context) _CRAZY_PUBLIC;

// Set an explicit reserved memory mapping to be used on the next library
// load. |reserved_address| is the page-aligned reserved address,
// |reserved_size| is the page-aligned reserved size, and if |load_fallback|
// is true, then the linker will try to load the library at a different
// address if it fails to load its segments at the reserved address range.
void crazy_context_set_reserved_map(crazy_context_t* context,
                                    uintptr_t reserved_address,
                                    size_t reserved_size,
                                    bool load_fallback) _CRAZY_PUBLIC;

// Destroy a given context object.
void crazy_context_destroy(crazy_context_t* context) _CRAZY_PUBLIC;

// Record the value of |java_vm| inside of |context|. If this is not NULL,
// which is the default, then after loading any library, the crazy linker
// will look for a "JNI_OnLoad" symbol within it, and, if it exists, will call
// it, passing the value of |java_vm| to it. If the function returns with
// a jni version number that is smaller than |minimum_jni_version|, then
// the library load will fail with an error.
//
// The |java_vm| field is also saved in the crazy_library_t object, and
// used at unload time to call JNI_OnUnload() if it exists.
void crazy_set_java_vm(void* java_vm, int minimum_jni_version);

// Retrieves the last values set with crazy_context_set_java_vm().
// A value of NUMM in |*java_vm| means the feature is disabled.
void crazy_get_java_vm(void** java_vm, int* minimum_jni_version);

// Add one or more paths to the global list of library search paths. |path| is a
// string using a column (:) as a list separator. As with the PATH variable,
// an empty list item is equivalent to '.', the current directory.
// This can fail if too many paths are added to the context.
//
// NOTE: Calling this function appends new paths to the global search list,
// but all paths added with this function will be searched before
// the ones listed in LD_LIBRARY_PATH.
crazy_status_t crazy_add_search_path(const char* file_path) _CRAZY_PUBLIC;

// Find the ELF binary that contains |address|, and add its directory
// path to the global list of search directories. This is useful to
// load libraries in the same directory as the current program itself.
crazy_status_t crazy_add_search_path_for_address(void* address) _CRAZY_PUBLIC;

// Reset the search paths to the value of the LD_LIBRARY_PATH
// environment variable. This essentially removes any paths
// that were added with crazy_add_search_path() or
// crazy_context_add_search_path_for_address().
void crazy_reset_search_paths(void) _CRAZY_PUBLIC;

// Opaque handle to a library as seen/loaded by the crazy linker.
typedef struct crazy_library_t crazy_library_t;

// Try to open or load a library with the crazy linker.
// |lib_name| if the library name or path. If it contains a directory
// separator (/), this is treated as a explicit file path, otherwise
// it is treated as a base name, and the context's search path list
// will be used to locate the corresponding file.
// |context| is a linker context handle. Can be NULL for defaults.
// On success, return CRAZY_STATUS_SUCCESS and sets |*library|.
// Libraries are reference-counted, trying to open the same library
// twice will return the same library handle.
//
// NOTE: The load address and file offset from the context only apply
// to the library being loaded (when not already in the process). If the
// operations needs to load any dependency libraries, these will use
// offset and address values of 0 to do so.
//
// NOTE: It is possible to open NDK system libraries (e.g. "liblog.so")
// with this function, but they will be loaded with the system dlopen().
crazy_status_t crazy_library_open(crazy_library_t** library,
                                  const char* lib_name,
                                  crazy_context_t* context) _CRAZY_PUBLIC;

// A structure used to hold information about a given library.
// |load_address| is the library's actual (page-aligned) load address.
// |load_size| is the library's actual (page-aligned) size.
// |relro_start| is the address of the library's RELRO section in memory.
// |relso_size| is the size of the library's RELRO section (or 0 if none).
// |relro_fd| is the ashmem file descriptor for the shared section, if one
// was created with crazy_library_enable_relro_sharing(), -1 otherwise.
typedef struct {
  size_t load_address;
  size_t load_size;
  size_t relro_start;
  size_t relro_size;
} crazy_library_info_t;

// Retrieve information about a given library.
// |library| is a library handle.
// |context| will get an error message on failure.
// On success, return true and sets |*info|.
// Note that this function will fail for system libraries.
crazy_status_t crazy_library_get_info(crazy_library_t* library,
                                      crazy_context_t* context,
                                      crazy_library_info_t* info);

// Create an ashmem region containing a copy of the RELRO section for a given
// |library|. This can be used with crazy_library_use_shared_relro().
// |load_address| can be specified as non-0 to ensure that the content of the
// ashmem region corresponds to a RELRO relocated for a new load address.
// on success, return CRAZY_STATUS_SUCCESS and sets |*relro_start| to the
// start of the RELRO section in memory, |*relro_size| to its size in bytes
// and |*relro_fd| to a file descriptor to a read-only ashmem region containing
// the data. On failure, return false and set error message in |context|.
// NOTE: On success, the caller becomes the owner of |*relro_fd| and is shall
// close it appropriately.
crazy_status_t crazy_library_create_shared_relro(crazy_library_t* library,
                                                 crazy_context_t* context,
                                                 size_t load_address,
                                                 size_t* relro_start,
                                                 size_t* relro_size,
                                                 int* relro_fd) _CRAZY_PUBLIC;

// Use the shared RELRO section of the same library loaded in a different
// address space. On success, return CRAZY_STATUS_SUCCESS and owns |relro_fd|.
// On failure, return CRAZY_STATUS_FAILURE and sets error message in |context|.
// |library| is the library handle.
// |relro_start| is the address of the RELRO section in memory.
// |relro_size| is the size of the RELRO section.
// |relro_fd| is the file descriptor for the shared RELRO ashmem region.
// |context| will receive an error in case of failure.
// NOTE: This will fail if this is a system library, or if the RELRO
// parameters do not match the library's actual load address.
// NOTE: The caller is responsible for closing the file descriptor after this
// call.
crazy_status_t crazy_library_use_shared_relro(crazy_library_t* library,
                                              crazy_context_t* context,
                                              size_t relro_start,
                                              size_t relro_size,
                                              int relro_fd) _CRAZY_PUBLIC;

// Look for a library named |library_name| in the set of currently
// loaded libraries, and return a handle for it in |*library| on success.
// Note that this increments the reference count on the library, thus
// the caller shall call crazy_library_close() when it's done with it.
crazy_status_t crazy_library_find_by_name(const char* library_name,
                                          crazy_library_t** library);

// Find the library that contains a given |address| in memory.
// On success, return CRAZY_STATUS_SUCCESS and sets |*library|.
crazy_status_t crazy_linker_find_library_from_address(
    void* address,
    crazy_library_t** library) _CRAZY_PUBLIC;

// Lookup a symbol's address by its |symbol_name| in a given library.
// This only looks at the symbols in |library|.
// On success, returns CRAZY_STATUS_SUCCESS and sets |*symbol_address|,
// which could be NULL for some symbols.
crazy_status_t crazy_library_find_symbol(crazy_library_t* library,
                                         const char* symbol_name,
                                         void** symbol_address) _CRAZY_PUBLIC;

// Lookup a symbol's address in all libraries known by the crazy linker.
// |symbol_name| is the symbol name. On success, returns CRAZY_STATUS_SUCCESS
// and sets |*symbol_address|.
// NOTE: This will _not_ look into system libraries that were not opened
// with the crazy linker.
crazy_status_t crazy_linker_find_symbol(const char* symbol_name,
                                        void** symbol_address) _CRAZY_PUBLIC;

// Find the in-process library that contains a given memory address.
// Note that this works even if the memory is inside a system library that
// was not previously opened with crazy_library_open().
// |address| is the memory address.
// On success, returns CRAZY_STATUS_SUCCESS and sets |*library|.
// The caller muyst call crazy_library_close() once it's done with the
// library.
crazy_status_t crazy_library_find_from_address(
    void* address,
    crazy_library_t** library) _CRAZY_PUBLIC;

// Close a library. This decrements its reference count. If it reaches
// zero, the library be unloaded from the process.
void crazy_library_close(crazy_library_t* library) _CRAZY_PUBLIC;

// Close a library, with associated context to support delayed operations.
void crazy_library_close_with_context(crazy_library_t* library,
                                      crazy_context_t* context) _CRAZY_PUBLIC;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CRAZY_LINKER_H */
