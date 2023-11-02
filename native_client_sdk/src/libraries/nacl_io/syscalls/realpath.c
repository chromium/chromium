/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sdk_util/macros.h"

EXTERN_C_BEGIN

#if defined(__native_client__)

// TODO(binji): glibc has realpath, but it fails for all tests. Investigate.

char* realpath(const char* path, char* resolved_path) {
  if (path == NULL) {
    errno = EINVAL;
    return NULL;
  }

  int needs_free = 0;
  if (resolved_path == NULL) {
    resolved_path = (char*)malloc(PATH_MAX);
    needs_free = 1;
  }

  struct stat statbuf;
  const char* in = path;
  char* out = resolved_path;
  char* out_end = resolved_path + PATH_MAX - 1;
  int done = 0;

  *out = 0;

  if (*in == '/') {
    // Absolute path.
    strcat(out, "/");
    in++;
    out++;
  } else {
    // Relative path.
    if (getcwd(out, out_end - out) == NULL)
      goto fail;

    out += strlen(out);
  }

  if (stat(resolved_path, &statbuf) != 0)
    goto fail;

  while (!done) {
    const char* next_slash = strchr(in, '/');
    size_t namelen;
    const char* next_in;
    if (next_slash) {
      namelen = next_slash - in;
      next_in = next_slash + 1;
    } else {
      namelen = strlen(in);
      next_in = in + namelen;  // Move to the '\0'
      done = 1;
    }

    if (namelen == 0) {
      // Empty name, do nothing.
    } else if (namelen == 1 && strncmp(in, ".", 1) == 0) {
      // Current directory, do nothing.
    } else if (namelen == 2 && strncmp(in, "..", 2) == 0) {
      // Parent directory, find previous slash in resolved_path.
      char* prev_slash = strrchr(resolved_path, '/');
      assert(prev_slash != NULL);

      out = prev_slash;
      if (prev_slash == resolved_path) {
        // Moved to the root. Keep the slash.
        ++out;
      }

      *out = 0;
    } else {
      // Append a slash if not at root.
      if (out != resolved_path + 1) {
        if (out + 1 > out_end) {
          errno = ENAMETOOLONG;
          goto fail;
        }

        strncat(out, "/", namelen);
        out++;
      }

      if (out + namelen > out_end) {
        errno = ENAMETOOLONG;
        goto fail;
      }

      strncat(out, in, namelen);
      out += namelen;
    }

    in = next_in;

    if (stat(resolved_path, &statbuf) != 0)
      goto fail;

    // If there is more to the path, then the current path must be a directory.
    if (!done && !S_ISDIR(statbuf.st_mode)) {
      errno = ENOTDIR;
      goto fail;
    }
  }

  return resolved_path;

fail:
  if (needs_free) {
    free(resolved_path);
  }
  return NULL;
}

EXTERN_C_END

#endif  // defined(__native_client__)
