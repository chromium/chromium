/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef __GLIBC__
#include <elf.h>
#include <link.h>
#endif  /* __GLIBC__ */

#include "irt.h"
#include "nacl/nacl_exception.h"

#include "error_handling/error_handling.h"
#include "error_handling/string_stream.h"


#define PAGE_CHUNK_SIZE (64 * 1024)
#define PAGE_CHUNK_MASK (PAGE_CHUNK_SIZE - 1)
#define STACK_SIZE_MIN (PAGE_CHUNK_SIZE * 4)

#define MAX_FRAME_SIZE (10 * 1024)
#define MAX_FRAME_CAP 128

static pthread_key_t s_eh_stack_info_key;
static EHJsonHandler s_eh_json_callback = NULL;
static int s_eh_exception_enabled = 0;
static struct nacl_irt_exception_handling s_exception_handling;


typedef struct {
  void* stack;
  size_t size;
} EHStackInfo;

static uintptr_t EHReadPointer(uintptr_t offset) {
  return *((uintptr_t*) offset);
}

static void EHPrintArch(sstream_t* ss, struct NaClExceptionContext* context) {
#if defined(__x86_64__)
  ssprintf(ss, "\"arch\": \"x86_64\",\n");
#elif defined(__i386__)
  ssprintf(ss, "\"arch\": \"x86_32\",\n");
#elif defined(__arm__)
  ssprintf(ss, "\"arch\": \"arm\",\n");
#elif defined(__mips__)
  ssprintf(ss, "\"arch\": \"mips\",\n");
#else
#error Unknown ARCH
#endif
}

static void EHPrintSegments(sstream_t* ss,
                            struct NaClExceptionContext* context) {
  ssprintf(ss, "\"segments\": [");
  ssprintf(ss, "],\n");
}

static void EHPrintFrame(sstream_t* ss, EHFrame* frame) {
  uintptr_t start;
  uintptr_t i;

  ssprintf(ss, "{\n");
  ssprintf(ss, "\"frame_ptr\": %u,\n", frame->frame_ptr);
  ssprintf(ss, "\"prog_ctr\": %u,\n", frame->prog_ctr);
  ssprintf(ss, "\"data\": [\n");

#if defined(__x86_64__)
  start = frame->frame_ptr + 8;
#else
  start = frame->frame_ptr + 16;
#endif
  /* Capture the stack, no mare than 128 bytes to keep the size sane. */
  for (i = start; i < frame->next_ptr && i - start < MAX_FRAME_CAP; i += 4) {
    if (i != start) {
      ssprintf(ss, ",");
    }
    ssprintf(ss, "%u\n", EHReadPointer(i));
  }
  ssprintf(ss, "]\n}\n");
}


static void EHPrintMainContext(sstream_t* ss,
                               struct NaClExceptionContext* context) {
  struct NaClExceptionPortableContext* portable_context =
      nacl_exception_context_get_portable(context);
  ssprintf(ss, "\"handler\": {\n");
  ssprintf(ss, "\"prog_ctr\": %u,\n", portable_context->prog_ctr);
  ssprintf(ss, "\"stack_ptr\": %u,\n", portable_context->stack_ptr);
  ssprintf(ss, "\"frame_ptr\": %u\n", portable_context->frame_ptr);
  ssprintf(ss, "},\n");
}


int EHGetTopFrame(sstream_t* ss, struct NaClExceptionContext* context,
                  EHFrame* frame) {
  struct NaClExceptionPortableContext* portable_context =
      nacl_exception_context_get_portable(context);

  frame->prog_ctr = portable_context->prog_ctr;
  frame->frame_ptr = portable_context->frame_ptr;
  frame->next_ptr = EHReadPointer(portable_context->frame_ptr);
  return 1;
}


int EHUnwindFrame(EHFrame* frame) {
  uintptr_t frame_ptr;
  uintptr_t next;

  // Verify the current frame
  if (NULL == frame) return 0;

  frame_ptr = frame->frame_ptr;
  next = frame->next_ptr;

  // Abort if done or unwind moves us in the wrong direction
  if (next <= frame_ptr || next == 0) return 0;

  // Abort if frame is > 10K
  if (next - frame_ptr > MAX_FRAME_SIZE) return 0;

  // Unwind the frame
  frame->frame_ptr = next;
  frame->next_ptr = EHReadPointer(frame->frame_ptr);
#if defined(__x86_64__)
  frame->prog_ctr = EHReadPointer(frame_ptr + 8);
#else
  frame->prog_ctr = EHReadPointer(frame_ptr + 4);
#endif
  return 1;
}


static void EHStackInfoDestructor(void *arg) {
  EHStackInfo* info = (EHStackInfo*) arg;

  if (info) {
    munmap(info->stack, info->size);
  }
  free(info);
}


void EHDefaultJsonHandler(struct NaClExceptionContext* context) {
  if (s_eh_json_callback) {
    EHFrame frame;
    sstream_t ss;
    ssinit(&ss);

    ssprintf(&ss, "{\n");
    EHPrintArch(&ss, context);
    EHPrintSegments(&ss, context);
    EHPrintMainContext(&ss, context);

    EHGetTopFrame(&ss, context, &frame);
    int first = 1;

    ssprintf(&ss, "\"frames\": [\n");
    do {
      if (!first) ssprintf(&ss, ",");
      EHPrintFrame(&ss, &frame);
      first = 0;
    } while (EHUnwindFrame(&frame));

    /* End frame LIST and context DICT */
    ssprintf(&ss, "]\n}\n");
    s_eh_json_callback(ss.data);

    ssfree(&ss);
    while(1) sleep(9999);
  }
}


void EHRequestExceptionsRaw(EHRawHandler callback) {
  size_t interface_size = sizeof(s_exception_handling);
  if (s_eh_exception_enabled) {
    fprintf(stderr, "ERROR: EHInit already initialized.\n");
    return;
  }
  if (NULL == callback) {
    fprintf(stderr, "ERROR: EHInit called with NULL callback.\n");
    return;
  }

  /* Expect an exact match on the interface structure size. */
  if (nacl_interface_query(NACL_IRT_EXCEPTION_HANDLING_v0_1,
                           &s_exception_handling,
                           interface_size) != interface_size) {
    fprintf(stderr, "ERROR: EHInit failed nacl_interface_query\n");
    return;
  }

  if (s_exception_handling.exception_handler(callback, NULL) != 0) {
    fprintf(stderr, "ERROR: EHInit failed to install exception_handler\n");
    return;
  }

  s_eh_exception_enabled = 1;

  // Create a TLS key for storing per thread stack info
  pthread_key_create(&s_eh_stack_info_key, EHStackInfoDestructor);
}


void *EHRequestExceptionStackOnThread(size_t stack_size) {
  void* stack;
  void* guard;
  EHStackInfo* stack_info;

  // Set the stack size
  stack_size = (stack_size + PAGE_CHUNK_MASK) & PAGE_CHUNK_MASK;
  if (stack_size < STACK_SIZE_MIN) stack_size = STACK_SIZE_MIN;

  // Allocate stack + guard page
  stack = mmap(NULL, stack_size + PAGE_CHUNK_SIZE,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (MAP_FAILED == stack) return MAP_FAILED;

  // Remap to mprotect which may not be available
  guard = mmap(stack, PAGE_CHUNK_SIZE,
      PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (guard != stack) {
    fprintf(stderr, "WARNING: Failed to add guard page for alt stack.\n");
  }

  if (!s_exception_handling.exception_stack(stack, stack_size)) {
    fprintf(stderr, "ERROR: Failed to assign stack.\n");
    munmap(stack, stack_size);
    return MAP_FAILED;
  }

  // Allocate stack tracking information for this thread
  stack_info = (EHStackInfo*) malloc(sizeof(EHStackInfo));
  stack_info->stack = stack;
  stack_info->size = stack_size + PAGE_CHUNK_SIZE;
  pthread_setspecific(s_eh_stack_info_key, stack_info);
  return stack;
}


void EHRequestExceptionsJson(EHJsonHandler callback) {
  if (NULL == callback) return;

  EHRequestExceptionsRaw(EHDefaultJsonHandler);
  if (s_eh_exception_enabled) s_eh_json_callback = callback;
}


int EHHanderInstalled() {
  return s_eh_exception_enabled;
}
