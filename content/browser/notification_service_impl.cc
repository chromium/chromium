// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notification_service_impl.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace  content {

namespace {

ABSL_CONST_INIT thread_local NotificationServiceImpl* notification_service =
    nullptr;

}  // namespace

// static
NotificationServiceImpl* NotificationServiceImpl::current() {
  return notification_service;
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
  return base::Contains(map, source.map_key());
}

NotificationServiceImpl::NotificationServiceImpl()
    : resetter_(&notification_service, this, nullptr) {}

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
    if (observer_list->empty()) {
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
  // There's no particular reason for the order in which the different
  // classes of observers get notified here.

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
