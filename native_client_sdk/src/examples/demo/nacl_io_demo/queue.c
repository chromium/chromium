/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "queue.h"

#include <pthread.h>
#include <stdlib.h>

#include "ppapi/c/pp_var.h"

#define MAX_QUEUE_SIZE 16

/** A mutex that guards |g_queue|. */
static pthread_mutex_t g_queue_mutex;

/** A condition variable that is signalled when |g_queue| is not empty. */
static pthread_cond_t g_queue_not_empty_cond;

/** A circular queue of messages from JavaScript to be handled.
 *
 * If g_queue_start < g_queue_end:
 *   all elements in the range [g_queue_start, g_queue_end) are valid.
 * If g_queue_start > g_queue_end:
 *   all elements in the ranges [0, g_queue_end) and
 *   [g_queue_start, MAX_QUEUE_SIZE) are valid.
 * If g_queue_start == g_queue_end, and g_queue_size > 0:
 *   all elements in the g_queue are valid.
 * If g_queue_start == g_queue_end, and g_queue_size == 0:
 *   No elements are valid. */
static struct PP_Var g_queue[MAX_QUEUE_SIZE];

/** The index of the head of the queue. */
static int g_queue_start = 0;

/** The index of the tail of the queue, non-inclusive. */
static int g_queue_end = 0;

/** The size of the queue. */
static int g_queue_size = 0;

/** Return whether the queue is empty.
 *
 * NOTE: this function assumes g_queue_mutex lock is held.
 * @return non-zero if the queue is empty. */
static int IsQueueEmpty() { return g_queue_size == 0; }

/** Return whether the queue is full.
 *
 * NOTE: this function assumes g_queue_mutex lock is held.
 * @return non-zero if the queue is full. */
static int IsQueueFull() { return g_queue_size == MAX_QUEUE_SIZE; }

/** Initialize the message queue. */
void InitializeMessageQueue() {
  pthread_mutex_init(&g_queue_mutex, NULL);
  pthread_cond_init(&g_queue_not_empty_cond, NULL);
}

/** Enqueue a message (i.e. add to the end)
 *
 * If the queue is full, the message will be dropped.
 *
 * NOTE: this function assumes g_queue_mutex is _NOT_ held.
 * @param[in] message The message to enqueue.
 * @return non-zero if the message was added to the queue. */
int EnqueueMessage(struct PP_Var message) {
  pthread_mutex_lock(&g_queue_mutex);

  /* We shouldn't block the main thread waiting for the queue to not be full,
   * so just drop the message. */
  if (IsQueueFull()) {
    pthread_mutex_unlock(&g_queue_mutex);
    return 0;
  }

  g_queue[g_queue_end] = message;
  g_queue_end = (g_queue_end + 1) % MAX_QUEUE_SIZE;
  g_queue_size++;

  pthread_cond_signal(&g_queue_not_empty_cond);

  pthread_mutex_unlock(&g_queue_mutex);

  return 1;
}

/** Dequeue a message and return it.
 *
 * This function blocks until a message is available. It should not be called
 * on the main thread.
 *
 * NOTE: this function assumes g_queue_mutex is _NOT_ held.
 * @return The message at the head of the queue. */
struct PP_Var DequeueMessage() {
  struct PP_Var message;

  pthread_mutex_lock(&g_queue_mutex);

  while (IsQueueEmpty()) {
    pthread_cond_wait(&g_queue_not_empty_cond, &g_queue_mutex);
  }

  message = g_queue[g_queue_start];
  g_queue_start = (g_queue_start + 1) % MAX_QUEUE_SIZE;
  g_queue_size--;

  pthread_mutex_unlock(&g_queue_mutex);

  return message;
}
