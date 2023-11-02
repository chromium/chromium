// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATION_SERVICE_IMPL_H_

#include <map>

#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_service.h"

namespace content {

class NotificationObserver;
class NotificationRegistrar;

class CONTENT_EXPORT NotificationServiceImpl : public NotificationService {
 public:
  static NotificationServiceImpl* current();

  // Normally instantiated when the thread is created.  Not all threads have
  // a NotificationService.  Only one instance should be created per thread.
  NotificationServiceImpl();

  NotificationServiceImpl(const NotificationServiceImpl&) = delete;
  NotificationServiceImpl& operator=(const NotificationServiceImpl&) = delete;

  ~NotificationServiceImpl() override;

  // NotificationService:
  void Notify(int type,
              const NotificationSource& source,
              const NotificationDetails& details) override;

 private:
  friend class NotificationRegistrar;

  typedef base::ObserverList<NotificationObserver>::Unchecked
      NotificationObserverList;
  typedef std::map<uintptr_t, NotificationObserverList*> NotificationSourceMap;
  typedef std::map<int, NotificationSourceMap> NotificationObserverMap;
  typedef std::map<int, int> NotificationObserverCount;

  // Convenience function to determine whether a source has a
  // NotificationObserverList in the given map;
  static bool HasKey(const NotificationSourceMap& map,
                     const NotificationSource& source);

  // NOTE: Rather than using this directly, you should use a
  // NotificationRegistrar.
  //
  // Registers a NotificationObserver to be called whenever a matching
  // notification is posted.  Observer is a pointer to an object subclassing
  // NotificationObserver to be notified when an event matching the other two
  // parameters is posted to this service.  Type is the type of events to be
  // notified about (or NOTIFICATION_ALL to receive events of all
  // types).
  // Source is a NotificationSource object (created using
  // "Source<classname>(pointer)"), if this observer only wants to
  // receive events from that object, or NotificationService::AllSources()
  // to receive events from all sources.
  //
  // A given observer can be registered only once for each combination of
  // type and source.  If the same object is registered more than once,
  // it must be removed for each of those combinations of type and source later.
  //
  // The caller retains ownership of the object pointed to by observer.
  void AddObserver(NotificationObserver* observer,
                   int type,
                   const NotificationSource& source);

  // NOTE: Rather than using this directly, you should use a
  // NotificationRegistrar.
  //
  // Removes the object pointed to by observer from receiving notifications
  // that match type and source.  If no object matching the parameters is
  // currently registered, this method is a no-op.
  void RemoveObserver(NotificationObserver* observer,
                      int type,
                      const NotificationSource& source);

  // Keeps track of the observers for each type of notification.
  // Until we get a prohibitively large number of notification types,
  // a simple array is probably the fastest way to dispatch.
  NotificationObserverMap observers_;

#ifndef NDEBUG
  // Used to check to see that AddObserver and RemoveObserver calls are
  // balanced.
  NotificationObserverCount observer_counts_;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATION_SERVICE_IMPL_H_
