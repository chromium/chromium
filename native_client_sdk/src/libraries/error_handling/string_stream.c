/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "error_handling/string_stream.h"
void ssinit(sstream_t* stream) {
  stream->data = NULL;
  stream->length = 0;
}

void ssfree(sstream_t* stream) {
  free(stream->data);
  stream->data = 0;
  stream->length = 0;
}

int ssvprintf(sstream_t* stream, const char* format, va_list args) {
  va_list hold;
  int len;
  char* outstr;

  va_copy(hold, args);
  len = vsnprintf(NULL, 0, format, args);

  outstr = malloc(stream->length + len + 1);
  if (stream->data) {
    memcpy(outstr, stream->data, stream->length);
    free(stream->data);
  }

  stream->data = outstr;
  vsprintf(&stream->data[stream->length], format, hold);
  stream->length += len;

  return len;
}

int ssprintf(sstream_t* stream, const char* format, ...) {
  int out;
  va_list args;
  va_start(args, format);
  out = ssvprintf(stream, format, args);
  va_end(args);

  return out;
}
