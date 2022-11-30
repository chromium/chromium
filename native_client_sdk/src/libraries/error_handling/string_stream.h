/*
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_ERROR_HANDLING_STRING_STREAM_H_
#define LIBRARIES_ERROR_HANDLING_STRING_STREAM_H_

/*
 * Support for a stream stream in 'C', which is appended to via an sprintf-like
 * function.
 */

#include <stdarg.h>
#include <stdint.h>

typedef struct {
  char* data;
  size_t length;
} sstream_t;

void ssinit(sstream_t* stream);
void ssfree(sstream_t* stream);

/* Returns the number of bytes added to the stream. */
int ssvprintf(sstream_t* sstream, const char* format, va_list args);
int ssprintf(sstream_t* sstream, const char* format, ...);

#endif  // LIBRARIES_ERROR_HANDLING_STRING_STREAM_H_
