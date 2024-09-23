// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/ei_event_watcher_glib.h"

#include <unistd.h>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "third_party/libei/cipd/include/libei.h"

namespace remoting {

constexpr GSourceFuncs EiEventWatcherGlib::kWatchSourceFuncs;

EiEventWatcherGlib::EiEventWatcherGlib(
    int fd,
    ei* ei,
    EiEventWatcherGlib::EiEventHandler* handler)
    : fd_(fd), ei_(ei_ref(ei)), handler_(handler) {}

EiEventWatcherGlib::~EiEventWatcherGlib() {
  StopProcessingEvents();
}

void EiEventWatcherGlib::StartProcessingEvents() {
  DCHECK(!ei_source_);
  ei_source_ = static_cast<GLibEiSource*>(g_source_new(
      const_cast<GSourceFuncs*>(&kWatchSourceFuncs), sizeof(GLibEiSource)));
  ei_source_->event_watcher = this;
  ei_source_->poll_fd = {
      .fd = fd_,
      .events = G_IO_IN,
      .revents = 0,
  };

  g_source_add_poll(ei_source_, &ei_source_->poll_fd);
  g_source_set_can_recurse(ei_source_, TRUE);
  auto* context = g_main_context_get_thread_default();
  if (!context) {
    context = g_main_context_default();
  }
  g_source_attach(ei_source_, context);
  g_source_set_priority(ei_source_, kPriorityFdWatch);
}

void EiEventWatcherGlib::StopProcessingEvents() {
  if (ei_source_) {
    close(fd_);
    g_source_destroy(ei_source_);
    g_source_unref(ei_source_);
    ei_source_ = nullptr;
  }
  if (ei_) {
    ei_unref(ei_);
    ei_ = nullptr;
  }
}

// static
gboolean EiEventWatcherGlib::WatchSourcePrepare(GSource* source,
                                                gint* timeout_ms) {
  // Set an infinite timeout.
  *timeout_ms = -1;

  auto* event_watcher_glib =
      static_cast<GLibEiSource*>(source)->event_watcher.get();
  return event_watcher_glib->Prepare();
}

// static
gboolean EiEventWatcherGlib::WatchSourceCheck(GSource* source) {
  auto* glib_ei_source = static_cast<GLibEiSource*>(source);
  gushort flags = glib_ei_source->poll_fd.revents;
  return flags & G_IO_IN;
}

// static
gboolean EiEventWatcherGlib::WatchSourceDispatch(GSource* source,
                                                 GSourceFunc unused_func,
                                                 gpointer data) {
  auto* event_watcher_glib =
      static_cast<GLibEiSource*>(source)->event_watcher.get();
  event_watcher_glib->Dispatch();
  return TRUE;
}

void EiEventWatcherGlib::WatchSourceFinalize(GSource* source) {
  auto* src = static_cast<GLibEiSource*>(source);
  // This is needed to ensure raw_ptr releases the pointers it holds upon the
  // GSource destruction.
  src->event_watcher = nullptr;
}

bool EiEventWatcherGlib::Prepare() {
  if (!ei_) {
    return false;
  }

  struct ei_event* event = ei_peek_event(ei_.get());
  if (event) {
    ei_event_unref(event);
    return true;
  }
  return false;
}

void EiEventWatcherGlib::Dispatch() {
  ei_dispatch(ei_);
  struct ei_event* event;
  // Checks for events in the queue (with `ei_peek_event`) and removes them from
  // the queue (using `ei_get_event`)
  while ((event = ei_peek_event(ei_))) {
    ei_event_unref(event);
    event = ei_get_event(ei_);

    if (handler_) {
      handler_->HandleEiEvent(event);
    }

    bool should_stop = ei_event_get_type(event) == EI_EVENT_DISCONNECT;
    ei_event_unref(event);

    if (should_stop) {
      StopProcessingEvents();
      break;
    }
  }
}

}  // namespace remoting
