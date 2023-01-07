// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

#include <stdlib.h>
#include <pthread.h>

static pthread_key_t s_h_errno_key;
static pthread_once_t s_h_errno_once = PTHREAD_ONCE_INIT;

static void __h_errno_create(void) {
  pthread_key_create(&s_h_errno_key, NULL);
}

int *__h_errno_location(void) {
  int* h_errno_ptr;
  pthread_once(&s_h_errno_once, __h_errno_create);
  h_errno_ptr = (int *) pthread_getspecific(s_h_errno_key);

  if (NULL == h_errno_ptr) {
    h_errno_ptr = (int *) malloc(sizeof(int));
    pthread_setspecific(s_h_errno_key, h_errno_ptr);
    *h_errno_ptr = 0;
  }

  return h_errno_ptr;
}

#if defined(__BIONIC__)
int *__get_h_errno(void) {
  return __h_errno_location();
}
#endif

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)
