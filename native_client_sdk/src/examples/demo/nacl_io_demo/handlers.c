/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "handlers.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "nacl_io/osdirent.h"
#include "nacl_io/osinttypes.h"

#include "nacl_io_demo.h"

#define MAX_OPEN_FILES 10
#define MAX_OPEN_DIRS 10
#define MAX_PARAMS 4

#if defined(WIN32)
#define stat _stat
#endif

/**
 * A mapping from int -> FILE*, so the JavaScript messages can refer to an open
 * File.
 */
static FILE* g_OpenFiles[MAX_OPEN_FILES];

/**
 * A mapping from int -> DIR*, so the JavaScript messages can refer to an open
 * Directory.
 */
static void* g_OpenDirs[MAX_OPEN_DIRS];

/**
 * A collection of the most recently allocated parameter strings. This makes
 * the Handle* functions below easier to write because they don't have to
 * manually deallocate the strings they're using.
 */
static char* g_ParamStrings[MAX_PARAMS];

/**
 * Add |object| to |map| and return the index it was added at.
 * @param[in] map The map to add the object to.
 * @param[in] max_map_size The maximum map size.
 * @param[in] object The object to add to the map.
 * @return int The index of the added object, or -1 if there is no more space.
 */
static int AddToMap(void** map, int max_map_size, void* object) {
  int i;
  assert(object != NULL);
  for (i = 0; i < max_map_size; ++i) {
    if (map[i] == NULL) {
      map[i] = object;
      return i;
    }
  }

  return -1;
}

/**
 * Remove an object at index |i| from |map|.
 * @param[in] map The map to remove from.
 * @param[in] max_map_size The size of the map.
 * @param[in] i The index to remove.
 */
static void RemoveFromMap(void** map, int max_map_size, int i) {
  assert(i >= 0 && i < max_map_size);
  map[i] = NULL;
}

/**
 * Add the file to the g_OpenFiles map.
 * @param[in] file The file to add to g_OpenFiles.
 * @return int The index of the FILE in g_OpenFiles, or -1 if there are too many
 *             open files.
 */
static int AddFileToMap(FILE* file) {
  return AddToMap((void**)g_OpenFiles, MAX_OPEN_FILES, file);
}

/**
 * Remove the file from the g_OpenFiles map.
 * @param[in] i The index of the file handle to remove.
 */
static void RemoveFileFromMap(int i) {
  RemoveFromMap((void**)g_OpenFiles, MAX_OPEN_FILES, i);
}

/* Win32 doesn't support DIR/opendir/readdir/closedir. */
#if !defined(WIN32)
/**
 * Add the dir to the g_OpenDirs map.
 * @param[in] dir The dir to add to g_OpenDirs.
 * @return int The index of the DIR in g_OpenDirs, or -1 if there are too many
 *             open dirs.
 */
static int AddDirToMap(DIR* dir) {
  return AddToMap((void**)g_OpenDirs, MAX_OPEN_DIRS, dir);
}

/**
 * Remove the dir from the g_OpenDirs map.
 * @param[in] i The index of the dir handle to remove.
 */
static void RemoveDirFromMap(int i) {
  RemoveFromMap((void**)g_OpenDirs, MAX_OPEN_DIRS, i);
}
#endif

/**
 * Get the number of parameters.
 * @param[in] params The parameter array.
 * @return uint32_t The number of parameters in the array.
 */
static uint32_t GetNumParams(struct PP_Var params) {
  return g_ppb_var_array->GetLength(params);
}

/**
 * Get a parameter at |index| as a string.
 * @param[in] params The parameter array.
 * @param[in] index The index in |params| to get.
 * @param[out] out_string The output string.
 * @param[out] out_string_len The length of the output string.
 * @param[out] out_error An error message, if this operation failed.
 * @return int 0 if successful, otherwise 1.
 */
static int GetParamString(struct PP_Var params,
                          uint32_t index,
                          char** out_string,
                          uint32_t* out_string_len,
                          const char** out_error) {
  if (index >= MAX_PARAMS) {
    *out_error = PrintfToNewString("Param index %u >= MAX_PARAMS (%d)",
                                   index, MAX_PARAMS);
    return 1;
  }

  struct PP_Var value = g_ppb_var_array->Get(params, index);
  if (value.type != PP_VARTYPE_STRING) {
    *out_error =
        PrintfToNewString("Expected param at index %d to be a string", index);
    return 1;
  }

  uint32_t length;
  const char* var_str = g_ppb_var->VarToUtf8(value, &length);

  char* string = (char*)malloc(length + 1);
  memcpy(string, var_str, length);
  string[length] = 0;

  /* Put the allocated string in g_ParamStrings. This keeps us from leaking
   * each parameter string, without having to do manual cleanup in every
   * Handle* function below.
   */
  free(g_ParamStrings[index]);
  g_ParamStrings[index] = string;


  *out_string = string;
  *out_string_len = length;
  return 0;
}

/**
 * Get a parameter at |index| as a FILE*.
 * @param[in] params The parameter array.
 * @param[in] index The index in |params| to get.
 * @param[out] out_file The output FILE*.
 * @param[out] out_file_index The index of the output FILE* in g_OpenFiles.
 * @param[out] out_error An error message, if this operation failed.
 * @return int 0 if successful, otherwise 1.
 */
static int GetParamFile(struct PP_Var params,
                        uint32_t index,
                        FILE** out_file,
                        int32_t* out_file_index,
                        const char** out_error) {
  struct PP_Var value = g_ppb_var_array->Get(params, index);
  if (value.type != PP_VARTYPE_INT32) {
    *out_error =
        PrintfToNewString("Expected param at index %d to be an int32", index);
    return 1;
  }

  int32_t file_index = value.value.as_int;
  if (file_index < 0 || file_index >= MAX_OPEN_FILES) {
    *out_error = PrintfToNewString("File index %d is out range", file_index);
    return 1;
  }

  if (g_OpenFiles[file_index] == NULL) {
    *out_error = PrintfToNewString("File index %d is not open", file_index);
    return 1;
  }

  *out_file = g_OpenFiles[file_index];
  *out_file_index = file_index;
  return 0;
}

/**
 * Get a parameter at |index| as a DIR*.
 * @param[in] params The parameter array.
 * @param[in] index The index in |params| to get.
 * @param[out] out_file The output DIR*.
 * @param[out] out_file_index The index of the output DIR* in g_OpenDirs.
 * @param[out] out_error An error message, if this operation failed.
 * @return int 0 if successful, otherwise 1.
 */
static int GetParamDir(struct PP_Var params,
                       uint32_t index,
                       DIR** out_dir,
                       int32_t* out_dir_index,
                       const char** out_error) {
  struct PP_Var value = g_ppb_var_array->Get(params, index);
  if (value.type != PP_VARTYPE_INT32) {
    *out_error =
        PrintfToNewString("Expected param at index %d to be an int32", index);
    return 1;
  }

  int32_t dir_index = value.value.as_int;
  if (dir_index < 0 || dir_index >= MAX_OPEN_DIRS) {
    *out_error = PrintfToNewString("Dir at index %d is out range", dir_index);
    return 1;
  }

  if (g_OpenDirs[dir_index] == NULL) {
    *out_error = PrintfToNewString("Dir index %d is not open", dir_index);
    return 1;
  }

  *out_dir = g_OpenDirs[dir_index];
  *out_dir_index = dir_index;
  return 0;
}

/**
 * Get a parameter at |index| as an int.
 * @param[in] params The parameter array.
 * @param[in] index The index in |params| to get.
 * @param[out] out_file The output int32_t.
 * @param[out] out_error An error message, if this operation failed.
 * @return int 0 if successful, otherwise 1.
 */
static int GetParamInt(struct PP_Var params,
                       uint32_t index,
                       int32_t* out_int,
                       const char** out_error) {
  struct PP_Var value = g_ppb_var_array->Get(params, index);
  if (value.type != PP_VARTYPE_INT32) {
    *out_error =
        PrintfToNewString("Expected param at index %d to be an int32", index);
    return 1;
  }

  *out_int = value.value.as_int;
  return 0;
}

/**
 * Create a response PP_Var to send back to JavaScript.
 * @param[out] response_var The response PP_Var.
 * @param[in] cmd The name of the function that is being executed.
 * @param[out] out_error An error message, if this call failed.
 */
static void CreateResponse(struct PP_Var* response_var,
                           const char* cmd,
                           const char** out_error) {
  PP_Bool result;

  struct PP_Var dict_var = g_ppb_var_dictionary->Create();
  struct PP_Var cmd_key = CStrToVar("cmd");
  struct PP_Var cmd_value = CStrToVar(cmd);

  result = g_ppb_var_dictionary->Set(dict_var, cmd_key, cmd_value);
  g_ppb_var->Release(cmd_key);
  g_ppb_var->Release(cmd_value);

  if (!result) {
    g_ppb_var->Release(dict_var);
    *out_error =
        PrintfToNewString("Unable to set \"cmd\" key in result dictionary");
    return;
  }

  struct PP_Var args_key = CStrToVar("args");
  struct PP_Var args_value = g_ppb_var_array->Create();
  result = g_ppb_var_dictionary->Set(dict_var, args_key, args_value);
  g_ppb_var->Release(args_key);
  g_ppb_var->Release(args_value);

  if (!result) {
    g_ppb_var->Release(dict_var);
    *out_error =
        PrintfToNewString("Unable to set \"args\" key in result dictionary");
    return;
  }

  *response_var = dict_var;
}

/**
 * Append a PP_Var to the response dictionary.
 * @param[in,out] response_var The response PP_var.
 * @param[in] value The value to add to the response args.
 * @param[out] out_error An error message, if this call failed.
 */
static void AppendResponseVar(struct PP_Var* response_var,
                              struct PP_Var value,
                              const char** out_error) {
  struct PP_Var args_value = GetDictVar(*response_var, "args");
  uint32_t args_length = g_ppb_var_array->GetLength(args_value);
  PP_Bool result = g_ppb_var_array->Set(args_value, args_length, value);
  if (!result) {
    // Release the dictionary that was there before.
    g_ppb_var->Release(*response_var);

    // Return an error message instead.
    *response_var = PP_MakeUndefined();
    *out_error = PrintfToNewString("Unable to append value to result");
    return;
  }
}

/**
 * Append an int to the response dictionary.
 * @param[in,out] response_var The response PP_var.
 * @param[in] value The value to add to the response args.
 * @param[out] out_error An error message, if this call failed.
 */
static void AppendResponseInt(struct PP_Var* response_var,
                              int32_t value,
                              const char** out_error) {
  AppendResponseVar(response_var, PP_MakeInt32(value), out_error);
}

/**
 * Append a string to the response dictionary.
 * @param[in,out] response_var The response PP_var.
 * @param[in] value The value to add to the response args.
 * @param[out] out_error An error message, if this call failed.
 */
static void AppendResponseString(struct PP_Var* response_var,
                                 const char* value,
                                 const char** out_error) {
  struct PP_Var value_var = CStrToVar(value);
  AppendResponseVar(response_var, value_var, out_error);
  g_ppb_var->Release(value_var);
}

#define CHECK_PARAM_COUNT(name, expected)                                   \
  if (GetNumParams(params) != expected) {                                   \
    *out_error = PrintfToNewString(#name " takes " #expected " parameters." \
                                   " Got %d", GetNumParams(params));        \
    return 1;                                                               \
  }

#define PARAM_STRING(index, var)                                    \
  char* var;                                                        \
  uint32_t var##_len;                                               \
  if (GetParamString(params, index, &var, &var##_len, out_error)) { \
    return 1;                                                       \
  }

#define PARAM_FILE(index, var)                                      \
  FILE* var;                                                        \
  int32_t var##_index;                                              \
  if (GetParamFile(params, index, &var, &var##_index, out_error)) { \
    return 1;                                                       \
  }

#define PARAM_DIR(index, var)                                      \
  DIR* var;                                                        \
  int32_t var##_index;                                             \
  if (GetParamDir(params, index, &var, &var##_index, out_error)) { \
    return 1;                                                      \
  }

#define PARAM_INT(index, var)                        \
  int32_t var;                                       \
  if (GetParamInt(params, index, &var, out_error)) { \
    return 1;                                        \
  }

#define CREATE_RESPONSE(name) CreateResponse(output, #name, out_error)
#define RESPONSE_STRING(var) AppendResponseString(output, var, out_error)
#define RESPONSE_INT(var) AppendResponseInt(output, var, out_error)

/**
 * Handle a call to fopen() made by JavaScript.
 *
 * fopen expects 2 parameters:
 *   0: the path of the file to open
 *   1: the mode string
 * on success, fopen returns a result in |output|:
 *   0: "fopen"
 *   1: the filename opened
 *   2: the file index
 * on failure, fopen returns an error string in |out_error|.
 */
int HandleFopen(struct PP_Var params,
                struct PP_Var* output,
                const char** out_error) {
  CHECK_PARAM_COUNT(fopen, 2);
  PARAM_STRING(0, filename);
  PARAM_STRING(1, mode);

  FILE* file = fopen(filename, mode);

  if (!file) {
    *out_error = PrintfToNewString("fopen returned a NULL FILE*");
    return 1;
  }

  int file_index = AddFileToMap(file);
  if (file_index == -1) {
    *out_error = PrintfToNewString("Example only allows %d open file handles",
                                   MAX_OPEN_FILES);
    return 1;
  }

  CREATE_RESPONSE(fopen);
  RESPONSE_STRING(filename);
  RESPONSE_INT(file_index);
  return 0;
}

/**
 * Handle a call to fwrite() made by JavaScript.
 *
 * fwrite expects 2 parameters:
 *   0: The index of the file (which is mapped to a FILE*)
 *   1: A string to write to the file
 * on success, fwrite returns a result in |output|:
 *   0: "fwrite"
 *   1: the file index
 *   2: the number of bytes written
 * on failure, fwrite returns an error string in |out_error|.
 */
int HandleFwrite(struct PP_Var params,
                 struct PP_Var* output,
                 const char** out_error) {
  CHECK_PARAM_COUNT(fwrite, 2);
  PARAM_FILE(0, file);
  PARAM_STRING(1, data);

  size_t bytes_written = fwrite(data, 1, data_len, file);
  if (ferror(file)) {
    *out_error = PrintfToNewString(
        "Wrote %" PRIuS " bytes, but ferror() returns true", bytes_written);
    return 1;
  }

  CREATE_RESPONSE(fwrite);
  RESPONSE_INT(file_index);
  RESPONSE_INT(bytes_written);
  return 0;
}

/**
 * Handle a call to fread() made by JavaScript.
 *
 * fread expects 2 parameters:
 *   0: The index of the file (which is mapped to a FILE*)
 *   1: The number of bytes to read from the file.
 * on success, fread returns a result in |output|:
 *   0: "fread"
 *   1: the file index
 *   2: the data read from the file
 * on failure, fread returns an error string in |out_error|.
 */
int HandleFread(struct PP_Var params,
                struct PP_Var* output,
                const char** out_error) {
  CHECK_PARAM_COUNT(fread, 2);
  PARAM_FILE(0, file);
  PARAM_INT(1, data_len);

  char* buffer = (char*)malloc(data_len + 1);
  size_t bytes_read = fread(buffer, 1, data_len, file);
  buffer[bytes_read] = 0;

  if (ferror(file)) {
    *out_error = PrintfToNewString(
        "Read %" PRIuS " bytes, but ferror() returns true", bytes_read);
    free(buffer);
    return 1;
  }

  CREATE_RESPONSE(fread);
  RESPONSE_INT(file_index);
  RESPONSE_STRING(buffer);
  free(buffer);
  return 0;
}

/**
 * Handle a call to fseek() made by JavaScript.
 *
 * fseek expects 3 parameters:
 *   0: The index of the file (which is mapped to a FILE*)
 *   1: The offset to seek to
 *   2: An integer representing the whence parameter of standard fseek.
 *      whence = 0: seek from the beginning of the file
 *      whence = 1: seek from the current file position
 *      whence = 2: seek from the end of the file
 * on success, fseek returns a result in |output|:
 *   0: "fseek"
 *   1: the file index
 *   2: The new file position
 * on failure, fseek returns an error string in |out_error|.
 */
int HandleFseek(struct PP_Var params,
                struct PP_Var* output,
                const char** out_error) {
  CHECK_PARAM_COUNT(fseek, 3);
  PARAM_FILE(0, file);
  PARAM_INT(1, offset);
  PARAM_INT(2, whence);

  int result = fseek(file, offset, whence);
  if (result) {
    *out_error = PrintfToNewString("fseek returned error %d", result);
    return 1;
  }

  offset = ftell(file);
  if (offset < 0) {
    *out_error = PrintfToNewString(
        "fseek succeeded, but ftell returned error %d", offset);
    return 1;
  }

  CREATE_RESPONSE(fseek);
  RESPONSE_INT(file_index);
  RESPONSE_INT(offset);
  return 0;
}

/**
 * Handle a call to fflush() made by JavaScript.
 *
 * fflush expects 1 parameters:
 *   0: The index of the file (which is mapped to a FILE*)
 * on success, fflush returns a result in |output|:
 *   0: "fflush"
 *   1: the file index
 * on failure, fflush returns an error string in |out_error|.
 */
int HandleFflush(struct PP_Var params,
                 struct PP_Var* output,
                 const char** out_error) {
  CHECK_PARAM_COUNT(fflush, 1);
  PARAM_FILE(0, file);

  fflush(file);

  CREATE_RESPONSE(fflush);
  RESPONSE_INT(file_index);
  return 0;
}

/**
 * Handle a call to fclose() made by JavaScript.
 *
 * fclose expects 1 parameter:
 *   0: The index of the file (which is mapped to a FILE*)
 * on success, fclose returns a result in |output|:
 *   0: "fclose"
 *   1: the file index
 * on failure, fclose returns an error string in |out_error|.
 */
int HandleFclose(struct PP_Var params,
                 struct PP_Var* output,
                 const char** out_error) {
  CHECK_PARAM_COUNT(fclose, 1);
  PARAM_FILE(0, file);

  int result = fclose(file);
  if (result) {
    *out_error = PrintfToNewString("fclose returned error %d", result);
    return 1;
  }

  RemoveFileFromMap(file_index);

  CREATE_RESPONSE(fclose);
  RESPONSE_INT(file_index);
  return 0;
}

/**
 * Handle a call to stat() made by JavaScript.
 *
 * stat expects 1 parameter:
 *   0: The name of the file
 * on success, stat returns a result in |output|:
 *   0: "stat"
 *   1: the file name
 *   2: the size of the file
 * on failure, stat returns an error string in |out_error|.
 */
int HandleStat(struct PP_Var params,
               struct PP_Var* output,
               const char** out_error) {
  CHECK_PARAM_COUNT(stat, 1);
  PARAM_STRING(0, filename);

  struct stat buf;
  memset(&buf, 0, sizeof(buf));
  int result = stat(filename, &buf);

  if (result == -1) {
    *out_error = PrintfToNewString("stat returned error %d", errno);
    return 1;
  }

  CREATE_RESPONSE(stat);
  RESPONSE_STRING(filename);
  RESPONSE_INT(buf.st_size);
  return 0;
}

/**
 * Handle a call to opendir() made by JavaScript.
 *
 * opendir expects 1 parameter:
 *   0: The name of the directory
 * on success, opendir returns a result in |output|:
 *   0: "opendir"
 *   1: the directory name
 *   2: the index of the directory
 * on failure, opendir returns an error string in |out_error|.
 */
int HandleOpendir(struct PP_Var params,
                  struct PP_Var* output,
                  const char** out_error) {
#if defined(WIN32)
  *out_error = PrintfToNewString("Win32 does not support opendir");
  return 1;
#else
  CHECK_PARAM_COUNT(opendir, 1);
  PARAM_STRING(0, dirname);

  DIR* dir = opendir(dirname);

  if (!dir) {
    *out_error = PrintfToNewString("opendir returned a NULL DIR*");
    return 1;
  }

  int dir_index = AddDirToMap(dir);
  if (dir_index == -1) {
    *out_error = PrintfToNewString("Example only allows %d open dir handles",
                                   MAX_OPEN_DIRS);
    return 1;
  }

  CREATE_RESPONSE(opendir);
  RESPONSE_STRING(dirname);
  RESPONSE_INT(dir_index);
  return 0;
#endif
}

/**
 * Handle a call to readdir() made by JavaScript.
 *
 * readdir expects 1 parameter:
 *   0: The index of the directory (which is mapped to a DIR*)
 * on success, opendir returns a result in |output|:
 *   0: "readdir"
 *   1: the inode number of the entry
 *   2: the name of the entry
 * if there are no more entries, |output| contains:
 *   0: "readdir"
 * on failure, readdir returns an error string in |out_error|.
 */
int HandleReaddir(struct PP_Var params,
                  struct PP_Var* output,
                  const char** out_error) {
#if defined(WIN32)
  *out_error = PrintfToNewString("Win32 does not support readdir");
  return 1;
#else
  CHECK_PARAM_COUNT(readdir, 1);
  PARAM_DIR(0, dir);

  struct dirent* entry = readdir(dir);

  CREATE_RESPONSE(readdir);
  RESPONSE_INT(dir_index);
  if (entry != NULL) {
    RESPONSE_INT(entry->d_ino);
    RESPONSE_STRING(entry->d_name);
  }
  return 0;
#endif
}

/**
 * Handle a call to closedir() made by JavaScript.
 *
 * closedir expects 1 parameter:
 *   0: The index of the directory (which is mapped to a DIR*)
 * on success, closedir returns a result in |output|:
 *   0: "closedir"
 *   1: the name of the directory
 * on failure, closedir returns an error string in |out_error|.
 */
int HandleClosedir(struct PP_Var params,
                   struct PP_Var* output,
                   const char** out_error) {
#if defined(WIN32)
  *out_error = PrintfToNewString("Win32 does not support closedir");
  return 1;
#else
  CHECK_PARAM_COUNT(closedir, 1);
  PARAM_DIR(0, dir);

  int result = closedir(dir);
  if (result) {
    *out_error = PrintfToNewString("closedir returned error %d", result);
    return 1;
  }

  RemoveDirFromMap(dir_index);

  CREATE_RESPONSE(closedir);
  RESPONSE_INT(dir_index);
  return 0;
#endif
}

/**
 * Handle a call to mkdir() made by JavaScript.
 *
 * mkdir expects 1 parameter:
 *   0: The name of the directory
 *   1: The mode to use for the new directory, in octal.
 * on success, mkdir returns a result in |output|:
 *   0: "mkdir"
 *   1: the name of the directory
 * on failure, mkdir returns an error string in |out_error|.
 */
int HandleMkdir(struct PP_Var params,
                struct PP_Var* output,
                const char** out_error) {
  CHECK_PARAM_COUNT(mkdir, 2);
  PARAM_STRING(0, dirname);
  PARAM_INT(1, mode);

  int result = mkdir(dirname, mode);

  if (result != 0) {
    *out_error = PrintfToNewString("mkdir returned error: %d", errno);
    return 1;
  }

  CREATE_RESPONSE(mkdir);
  RESPONSE_STRING(dirname);
  return 0;
}

/**
 * Handle a call to rmdir() made by JavaScript.
 *
 * rmdir expects 1 parameter:
 *   0: The name of the directory to remove
 * on success, rmdir returns a result in |output|:
 *   0: "rmdir"
 *   1: the name of the directory
 * on failure, rmdir returns an error string in |out_error|.
 */
int HandleRmdir(struct PP_Var params,
                struct PP_Var* output,
                const char** out_error) {
  CHECK_PARAM_COUNT(rmdir, 1);
  PARAM_STRING(0, dirname);

  int result = rmdir(dirname);

  if (result != 0) {
    *out_error = PrintfToNewString("rmdir returned error: %d", errno);
    return 1;
  }

  CREATE_RESPONSE(rmdir);
  RESPONSE_STRING(dirname);
  return 0;
}

/**
 * Handle a call to chdir() made by JavaScript.
 *
 * chdir expects 1 parameter:
 *   0: The name of the directory
 * on success, chdir returns a result in |output|:
 *   0: "chdir"
 *   1: the name of the directory
 * on failure, chdir returns an error string in |out_error|.
 */
int HandleChdir(struct PP_Var params,
                struct PP_Var* output,
                const char** out_error) {
  CHECK_PARAM_COUNT(chdir, 1);
  PARAM_STRING(0, dirname);

  int result = chdir(dirname);

  if (result != 0) {
    *out_error = PrintfToNewString("chdir returned error: %d", errno);
    return 1;
  }

  CREATE_RESPONSE(chdir);
  RESPONSE_STRING(dirname);
  return 0;
}

/**
 * Handle a call to getcwd() made by JavaScript.
 *
 * getcwd expects 0 parameters.
 * on success, getcwd returns a result in |output|:
 *   0: "getcwd"
 *   1: the current working directory
 * on failure, getcwd returns an error string in |out_error|.
 */
int HandleGetcwd(struct PP_Var params,
                 struct PP_Var* output,
                 const char** out_error) {
  CHECK_PARAM_COUNT(getcwd, 0);

  char cwd[PATH_MAX];
  char* result = getcwd(cwd, PATH_MAX);
  if (result == NULL) {
    *out_error = PrintfToNewString("getcwd returned error: %d", errno);
    return 1;
  }

  CREATE_RESPONSE(getcwd);
  RESPONSE_STRING(cwd);
  return 0;
}

/**
 * Handle a call to getaddrinfo() made by JavaScript.
 *
 * getaddrinfo expects 1 parameter:
 *   0: The name of the host to look up.
 * on success, getaddrinfo returns a result in |output|:
 *   0: "getaddrinfo"
 *   1: The canonical name
 *   2*n+2: Host name
 *   2*n+3: Address type (either "AF_INET" or "AF_INET6")
 * on failure, getaddrinfo returns an error string in |out_error|.
 */
int HandleGetaddrinfo(struct PP_Var params,
                      struct PP_Var* output,
                      const char** out_error) {
  CHECK_PARAM_COUNT(getaddrinfo, 2);
  PARAM_STRING(0, name);
  PARAM_STRING(1, family);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_CANONNAME;
  if (!strcmp(family, "AF_INET"))
    hints.ai_family = AF_INET;
  else if (!strcmp(family, "AF_INET6"))
    hints.ai_family = AF_INET6;
  else if (!strcmp(family, "AF_UNSPEC"))
    hints.ai_family = AF_UNSPEC;
  else {
    *out_error = PrintfToNewString("getaddrinfo uknown family: %s", family);
    return 1;
  }

  struct addrinfo* ai;
  int rtn = getaddrinfo(name, NULL, &hints, &ai);
  if (rtn != 0) {
    *out_error = PrintfToNewString("getaddrinfo failed, error is \"%s\"",
                                   gai_strerror(rtn));
    return 2;
  }

  CREATE_RESPONSE(getaddrinfo);
  RESPONSE_STRING(ai->ai_canonname);
  struct addrinfo* current = ai;
  while (current) {
    char addr_str[INET6_ADDRSTRLEN];
    if (ai->ai_family == AF_INET6) {
      struct sockaddr_in6* in6 = (struct sockaddr_in6*)current->ai_addr;
      inet_ntop(
          ai->ai_family, &in6->sin6_addr.s6_addr, addr_str, sizeof(addr_str));
    } else if (ai->ai_family == AF_INET) {
      struct sockaddr_in* in = (struct sockaddr_in*)current->ai_addr;
      inet_ntop(ai->ai_family, &in->sin_addr, addr_str, sizeof(addr_str));
    }

    RESPONSE_STRING(addr_str);
    RESPONSE_STRING(ai->ai_family == AF_INET ? "AF_INET" : "AF_INET6");

    current = current->ai_next;
  }

  freeaddrinfo(ai);
  return 0;
}

/**
 * Handle a call to gethostbyname() made by JavaScript.
 *
 * gethostbyname expects 1 parameter:
 *   0: The name of the host to look up.
 * on success, gethostbyname returns a result in |output|:
 *   0: "gethostbyname"
 *   1: Host name
 *   2: Address type (either "AF_INET" or "AF_INET6")
 *   3: The first address.
 *   4+ The second, third, etc. addresses.
 * on failure, gethostbyname returns an error string in |out_error|.
 */
int HandleGethostbyname(struct PP_Var params,
                        struct PP_Var* output,
                        const char** out_error) {
  CHECK_PARAM_COUNT(gethostbyname, 1);
  PARAM_STRING(0, name);

  struct hostent* info = gethostbyname(name);
  if (!info) {
    *out_error = PrintfToNewString("gethostbyname failed, error is \"%s\"",
                                   hstrerror(h_errno));
    return 1;
  }

  CREATE_RESPONSE(gethostbyname);
  RESPONSE_STRING(info->h_name);
  RESPONSE_STRING(info->h_addrtype == AF_INET ? "AF_INET" : "AF_INET6");

  struct in_addr** addr_list = (struct in_addr**)info->h_addr_list;
  int i;
  for (i = 0; addr_list[i] != NULL; i++) {
    if (info->h_addrtype == AF_INET) {
      RESPONSE_STRING(inet_ntoa(*addr_list[i]));
    } else {  // IPv6
      char addr_str[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, addr_list[i], addr_str, sizeof(addr_str));
      RESPONSE_STRING(addr_str);
    }
  }
  return 0;
}

/**
 * Handle a call to connect() made by JavaScript.
 *
 * connect expects 2 parameters:
 *   0: The hostname to connect to.
 *   1: The port number to connect to.
 * on success, connect returns a result in |output|:
 *   0: "connect"
 *   1: The socket file descriptor.
 * on failure, connect returns an error string in |out_error|.
 */
int HandleConnect(struct PP_Var params,
                  struct PP_Var* output,
                  const char** out_error) {
  CHECK_PARAM_COUNT(connect, 2);
  PARAM_STRING(0, hostname);
  PARAM_INT(1, port);

  // Lookup host
  struct hostent* hostent = gethostbyname(hostname);
  if (hostent == NULL) {
    *out_error = PrintfToNewString("gethostbyname() returned error: %d", errno);
    return 1;
  }

  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  memcpy(&addr.sin_addr.s_addr, hostent->h_addr_list[0], hostent->h_length);

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    *out_error = PrintfToNewString("socket() failed: %s", strerror(errno));
    return 1;
  }

  int result = connect(sock, (struct sockaddr*)&addr, addrlen);
  if (result != 0) {
    *out_error = PrintfToNewString("connect() failed: %s", strerror(errno));
    close(sock);
    return 1;
  }

  CREATE_RESPONSE(connect);
  RESPONSE_INT(sock);
  return 0;
}

/**
 * Handle a call to send() made by JavaScript.
 *
 * send expects 2 parameters:
 *   0: The socket file descriptor to send using.
 *   1: The NULL terminated string to send.
 * on success, send returns a result in |output|:
 *   0: "send"
 *   1: The number of bytes sent.
 * on failure, send returns an error string in |out_error|.
 */
int HandleSend(struct PP_Var params,
               struct PP_Var* output,
               const char** out_error) {
  CHECK_PARAM_COUNT(send, 2);
  PARAM_INT(0, sock);
  PARAM_STRING(1, buffer);

  int result = send(sock, buffer, strlen(buffer), 0);
  if (result <= 0) {
    *out_error = PrintfToNewString("send failed: %s", strerror(errno));
    return 1;
  }

  CREATE_RESPONSE(send);
  RESPONSE_INT(result);
  return 0;
}

/**
 * Handle a call to recv() made by JavaScript.
 *
 * recv expects 2 parameters:
 *   0: The socket file descriptor to recv from.
 *   1: The size of the buffer to pass to recv.
 * on success, send returns a result in |output|:
 *   0: "recv"
 *   1: The number of bytes received.
 *   2: The data received.
 * on failure, recv returns an error string in |out_error|.
 */
int HandleRecv(struct PP_Var params,
               struct PP_Var* output,
               const char** out_error) {
  CHECK_PARAM_COUNT(recv, 2);
  PARAM_INT(0, sock);
  PARAM_INT(1, buffersize);

  if (buffersize < 0 || buffersize > 65 * 1024) {
    *out_error =
        PrintfToNewString("recv buffersize must be between 0 and 65k.");
    return 1;
  }

  char* buffer = alloca(buffersize);
  memset(buffer, 0, buffersize);
  int result = recv(sock, buffer, buffersize, 0);
  if (result <= 0) {
    *out_error = PrintfToNewString("recv failed: %s", strerror(errno));
    return 1;
  }

  CREATE_RESPONSE(recv);
  RESPONSE_INT(result);
  RESPONSE_STRING(buffer);
  return 0;
}

/**
 * Handle a call to close() made by JavaScript.
 *
 * close expects 1 parameters:
 *   0: The socket file descriptor to close.
 * on success, close returns a result in |output|:
 *   0: "close"
 *   1: The socket file descriptor closed.
 * on failure, close returns an error string in |out_error|.
 */
int HandleClose(struct PP_Var params,
                struct PP_Var* output,
                const char** out_error) {
  CHECK_PARAM_COUNT(close, 1);
  PARAM_INT(0, sock);

  int result = close(sock);
  if (result != 0) {
    *out_error = PrintfToNewString("close returned error: %d", errno);
    return 1;
  }

  CREATE_RESPONSE(close);
  RESPONSE_INT(sock);
  return 0;
}
