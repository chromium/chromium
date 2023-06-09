// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_EI_EVENT_WATCHER_GLIB_H_
#define REMOTING_HOST_LINUX_EI_EVENT_WATCHER_GLIB_H_

#include <glib.h>
#include "base/memory/raw_ptr.h"

struct ei;
struct ei_event;

// The priorities of the event sources are important to be set correctly so that
// GTK event source is able to process the events it requires. This uses
// the same priority as MessagePumpGlib for fd watching.
constexpr int kPriorityFdWatch = G_PRIORITY_DEFAULT_IDLE - 10;

namespace remoting {

// Implementation to do Glib based polling/dispatching for libei events.
class EiEventWatcherGlib {
 public:
  class EiEventHandler {
   public:
    virtual ~EiEventHandler() = default;
    virtual void HandleEiEvent(struct ei_event* event) = 0;
  };

  EiEventWatcherGlib(int fd, ei* ei, EiEventHandler* handler);
  EiEventWatcherGlib(const EiEventWatcherGlib&) = delete;
  EiEventWatcherGlib& operator=(const EiEventWatcherGlib&) = delete;
  ~EiEventWatcherGlib();

  // Starts the event processing loop. Event processing will automatically stop
  // if `EI_EVENT_DISCONNECT` event is observed.
  void StartProcessingEvents();
  // Stops the event processing loop, if it was not already stopped, otherwise
  // it is a no-op.
  void StopProcessingEvents();

 private:
  struct GLibEiSource : public GSource {
    // Note: The GLibEiSource is created and destroyed by GLib. So its
    // constructor/destructor may or may not get called. Similarly, the user
    // data that is accepted by `g_source_new` is not deleted by Glib (i.e. Glib
    // doesn't take ownership of this data).
    raw_ptr<EiEventWatcherGlib> event_watcher;
    GPollFD poll_fd;
  };

  static gboolean WatchSourcePrepare(GSource* source, gint* timeout_ms);
  static gboolean WatchSourceCheck(GSource* source);
  static gboolean WatchSourceDispatch(GSource* source,
                                      GSourceFunc unused_func,
                                      gpointer data);
  static void WatchSourceFinalize(GSource* source);

  static constexpr GSourceFuncs kWatchSourceFuncs = {
      WatchSourcePrepare,
      WatchSourceCheck,
      WatchSourceDispatch,
      WatchSourceFinalize,
  };

  bool Prepare();
  void Dispatch();

  // The GLib event source for EI events.
  raw_ptr<GLibEiSource> ei_source_ = nullptr;

  // FD to watch for events.
  int fd_ = -1;

  raw_ptr<ei> ei_ = nullptr;
  raw_ptr<EiEventHandler> handler_ = nullptr;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_EI_EVENT_WATCHER_GLIB_H_
