/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef EXAMPLES_DEMO_NACL_IO_DEMO_QUEUE_H_
#define EXAMPLES_DEMO_NACL_IO_DEMO_QUEUE_H_

#include "ppapi/c/pp_var.h"

/* This file implements a single-producer/single-consumer queue, using a mutex
 * and a condition variable.
 *
 * There are techniques to implement a queue like this without using memory
 * barriers or locks on x86, but ARM's memory system is different from x86, so
 * we cannot make the same assumptions about visibility order of writes. Using a
 * mutex is slower, but also simpler.
 *
 * We make the assumption that messages are only enqueued on the main thread
 * and consumed on the worker thread. Because we don't want to block the main
 * thread, EnqueueMessage will return zero if the message could not be enqueued.
 *
 * DequeueMessage will block until a message is available using a condition
 * variable. Again, this may not be as fast as spin-waiting, but will consume
 * much less CPU (and battery), which is important to consider for ChromeOS
 * devices. */

void InitializeMessageQueue();
int EnqueueMessage(struct PP_Var message);
struct PP_Var DequeueMessage();

#endif  // EXAMPLES_DEMO_NACL_IO_DEMO_QUEUE_H_
