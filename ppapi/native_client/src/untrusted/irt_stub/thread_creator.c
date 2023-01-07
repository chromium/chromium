/*
 * Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/untrusted/irt_stub/thread_creator.h"

#include <pthread.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/untrusted/irt/irt.h"


static int thread_create(uintptr_t *tid,
                         void (*func)(void *thread_argument),
                         void *thread_argument) {
  /*
   * We know that newlib and glibc use a small pthread_t type, so we
   * do not need to wrap pthread_t values.
   */
  NACL_ASSERT_SAME_SIZE(pthread_t, uintptr_t);

  return pthread_create((pthread_t *) tid, NULL,
                        (void *(*)(void *thread_argument)) func,
                        thread_argument);
}

static int thread_join(uintptr_t tid) {
  return pthread_join((pthread_t) tid, NULL);
}

static const struct PP_ThreadFunctions thread_funcs = {
  thread_create,
  thread_join
};

/*
 * We cannot tell at link time whether the application uses PPB_Audio,
 * because of the way that PPAPI is defined via runtime interface
 * query rather than a set of static functions.  This means that we
 * register the audio thread functions unconditionally.  This adds the
 * small overhead of pulling in pthread_create() even if the
 * application does not use PPB_Audio or libpthread.
 *
 * If an application developer wants to avoid that cost, they can
 * override this function with an empty definition.
 */
void __nacl_register_thread_creator(const struct nacl_irt_ppapihook *hooks) {
  hooks->ppapi_register_thread_creator(&thread_funcs);
}
