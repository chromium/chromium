// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This is a list of environment variables which the ELF loader unsets when
// loading a SUID binary. Because they are unset rather than just ignored, they
// aren't passed to child processes of SUID processes either.
//
// We need to save these environment variables before running a SUID sandbox
// and restore them before running child processes (but after dropping root).
//
// List gathered from glibc sources (00ebd7ed58df389a78e41dece058048725cb585e):
//   sysdeps/unix/sysv/linux/i386/dl-librecon.h
//   sysdeps/generic/unsecvars.h

#ifndef SANDBOX_LINUX_SUID_COMMON_SUID_UNSAFE_ENVIRONMENT_VARIABLES_H_
#define SANDBOX_LINUX_SUID_COMMON_SUID_UNSAFE_ENVIRONMENT_VARIABLES_H_

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memcpy

static const char* const kSUIDUnsafeEnvironmentVariables[] = {
  "LD_AOUT_LIBRARY_PATH",
  "LD_AOUT_PRELOAD",
  "GCONV_PATH",
  "GETCONF_DIR",
  "HOSTALIASES",
  "LD_AUDIT",
  "LD_DEBUG",
  "LD_DEBUG_OUTPUT",
  "LD_DYNAMIC_WEAK",
  "LD_LIBRARY_PATH",
  "LD_ORIGIN_PATH",
  "LD_PRELOAD",
  "LD_PROFILE",
  "LD_SHOW_AUXV",
  "LD_USE_LOAD_BIAS",
  "LOCALDOMAIN",
  "LOCPATH",
  "MALLOC_TRACE",
  "NIS_PATH",
  "NLSPATH",
  "RESOLV_HOST_CONF",
  "RES_OPTIONS",
  "TMPDIR",
  "TZDIR",
  NULL,
};

// Return a malloc allocated string containing the 'saved' environment variable
// name for a given environment variable.
static inline char* SandboxSavedEnvironmentVariable(const char* envvar) {
  const size_t envvar_len = strlen(envvar);
  const size_t kMaxSizeT = (size_t) -1;

  if (envvar_len > kMaxSizeT - 1 - 8)
    return NULL;

  const size_t saved_envvarlen = envvar_len + 1 /* NUL terminator */ +
                                              8 /* strlen("SANDBOX_") */;
  char* const saved_envvar = (char*) malloc(saved_envvarlen);
  if (!saved_envvar)
    return NULL;

  memcpy(saved_envvar, "SANDBOX_", 8);
  memcpy(saved_envvar + 8, envvar, envvar_len);
  saved_envvar[8 + envvar_len] = 0;

  return saved_envvar;
}

#endif  // SANDBOX_LINUX_SUID_COMMON_SUID_UNSAFE_ENVIRONMENT_VARIABLES_H_
