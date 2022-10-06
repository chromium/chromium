// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/notification_registrar.h"

#include <stddef.h>

#include <ostream>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "content/browser/notification_service_impl.h"

namespace content {

struct NotificationRegistrar::Record {
  bool operator==(const Record& other) const;

  raw_ptr<NotificationObserver> observer;
  int type;
  NotificationSource source;
};

bool NotificationRegistrar::Record::operator==(const Record& other) const {
  return observer == other.observer &&
         type == other.type &&
         source == other.source;
}

NotificationRegistrar::NotificationRegistrar() {
  // Force the NotificationService to be constructed (if it isn't already).
  // This ensures the NotificationService will be registered on the
  // AtExitManager before any objects which access it via NotificationRegistrar.
  // This in turn means it will be destroyed after these objects, so they will
  // never try to access the NotificationService after it's been destroyed.
  NotificationServiceImpl::current();
  // It is OK to create a NotificationRegistrar instance on one thread and then
  // use it (exclusively) on another, so we detach from the initial thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NotificationRegistrar::~NotificationRegistrar() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveAll();
}

void NotificationRegistrar::Add(NotificationObserver* observer,
                                int type,
                                const NotificationSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsRegistered(observer, type, source)) << "Duplicate registration.";
  DCHECK(NotificationServiceImpl::current());

  Record record = { observer, type, source };
  registered_.push_back(record);

  NotificationServiceImpl::current()->AddObserver(observer, type, source);
}

void NotificationRegistrar::Remove(NotificationObserver* observer,
                                   int type,
                                   const NotificationSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Record record = { observer, type, source };
  RecordVector::iterator found = base::ranges::find(registered_, record);
  DCHECK(found != registered_.end());

  registered_.erase(found);

  // This can be nullptr if our owner outlives the NotificationService, e.g. if
  // our owner is a Singleton.
  NotificationServiceImpl* service = NotificationServiceImpl::current();
  if (service)
    service->RemoveObserver(observer, type, source);
}

void NotificationRegistrar::RemoveAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Early-exit if no registrations, to avoid calling
  // NotificationService::current.  If we've constructed an object with a
  // NotificationRegistrar member, but haven't actually used the notification
  // service, and we reach prgram exit, then calling current() below could try
  // to initialize the service's lazy TLS pointer during exit, which throws
  // wrenches at things.
  if (registered_.empty())
    return;

  // This can be nullptr if our owner outlives the NotificationService, e.g. if
  // our owner is a Singleton.
  NotificationServiceImpl* service = NotificationServiceImpl::current();
  if (service) {
    for (size_t i = 0; i < registered_.size(); i++) {
      service->RemoveObserver(registered_[i].observer,
                              registered_[i].type,
                              registered_[i].source);
    }
  }
  registered_.clear();
}

bool NotificationRegistrar::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registered_.empty();
}

bool NotificationRegistrar::IsRegistered(NotificationObserver* observer,
                                         int type,
                                         const NotificationSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Record record = { observer, type, source };
  return base::Contains(registered_, record);
}

}  // namespace content
