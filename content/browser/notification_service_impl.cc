// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notification_service_impl.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"

namespace  content {

namespace {

base::LazyInstance<base::ThreadLocalPointer<NotificationServiceImpl>>::
    DestructorAtExit lazy_tls_ptr = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
NotificationServiceImpl* NotificationServiceImpl::current() {
  return lazy_tls_ptr.Pointer()->Get();
}

// static
NotificationService* NotificationService::current() {
  return NotificationServiceImpl::current();
}

// static
NotificationService* NotificationService::Create() {
  return new NotificationServiceImpl;
}

// static
bool NotificationServiceImpl::HasKey(const NotificationSourceMap& map,
                                     const NotificationSource& source) {
  return map.find(source.map_key()) != map.end();
}

NotificationServiceImpl::NotificationServiceImpl() {
  DCHECK(current() == nullptr);
  lazy_tls_ptr.Pointer()->Set(this);
}

void NotificationServiceImpl::AddObserver(NotificationObserver* observer,
                                          int type,
                                          const NotificationSource& source) {
  // We have gotten some crashes where the observer pointer is NULL. The problem
  // is that this happens when we actually execute a notification, so have no
  // way of knowing who the bad observer was. We want to know when this happens
  // in release mode so we know what code to blame the crash on (since this is
  // guaranteed to crash later).
  CHECK(observer);

  NotificationObserverList* observer_list;
  if (HasKey(observers_[type], source)) {
    observer_list = observers_[type][source.map_key()];
  } else {
    observer_list = new NotificationObserverList;
    observers_[type][source.map_key()] = observer_list;
  }

  observer_list->AddObserver(observer);
#ifndef NDEBUG
  ++observer_counts_[type];
#endif
}

void NotificationServiceImpl::RemoveObserver(NotificationObserver* observer,
                                             int type,
                                             const NotificationSource& source) {
  // This is a very serious bug.  An object is most likely being deleted on
  // the wrong thread, and as a result another thread's NotificationServiceImpl
  // has its deleted pointer in its map.  A garbge object will be called in the
  // future.
  // NOTE: when this check shows crashes, use BrowserThread::DeleteOnIOThread or
  // other variants as the trait on the object.
  CHECK(HasKey(observers_[type], source));

  NotificationObserverList* observer_list =
      observers_[type][source.map_key()];
  if (observer_list) {
    observer_list->RemoveObserver(observer);
    if (!observer_list->might_have_observers()) {
      observers_[type].erase(source.map_key());
      delete observer_list;
    }
#ifndef NDEBUG
    --observer_counts_[type];
#endif
  }
}

void NotificationServiceImpl::Notify(int type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK_GT(type, NOTIFICATION_ALL) <<
      "Allowed for observing, but not posting.";

  // There's no particular reason for the order in which the different
  // classes of observers get notified here.

  // Notify observers of all types and all sources
  if (HasKey(observers_[NOTIFICATION_ALL], AllSources()) &&
      source != AllSources()) {
    for (auto& observer : *observers_[NOTIFICATION_ALL][AllSources().map_key()])
      observer.Observe(type, source, details);
  }

  // Notify observers of all types and the given source
  if (HasKey(observers_[NOTIFICATION_ALL], source)) {
    for (auto& observer : *observers_[NOTIFICATION_ALL][source.map_key()])
      observer.Observe(type, source, details);
  }

  // Notify observers of the given type and all sources
  if (HasKey(observers_[type], AllSources()) &&
      source != AllSources()) {
    for (auto& observer : *observers_[type][AllSources().map_key()])
      observer.Observe(type, source, details);
  }

  // Notify observers of the given type and the given source
  if (HasKey(observers_[type], source)) {
    for (auto& observer : *observers_[type][source.map_key()])
      observer.Observe(type, source, details);
  }
}


NotificationServiceImpl::~NotificationServiceImpl() {
  lazy_tls_ptr.Pointer()->Set(nullptr);

#ifndef NDEBUG
  for (int i = 0; i < static_cast<int>(observer_counts_.size()); i++) {
    if (observer_counts_[i] > 0) {
      // This may not be completely fixable -- see
      // http://code.google.com/p/chromium/issues/detail?id=11010 .
      VLOG(1) << observer_counts_[i] << " notification observer(s) leaked "
                 "of notification type " << i;
    }
  }
#endif

  for (int i = 0; i < static_cast<int>(observers_.size()); i++) {
    NotificationSourceMap omap = observers_[i];
    for (auto it = omap.begin(); it != omap.end(); ++it)
      delete it->second;
  }
}

}  // namespace content
