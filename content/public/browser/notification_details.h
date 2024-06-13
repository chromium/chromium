// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the type used to provide details for NotificationService
// notifications.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_DETAILS_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"

namespace content {

// Do not declare a NotificationDetails directly--use either
// "Details<detailsclassname>(detailsclasspointer)" or
// NotificationService::NoDetails().
class CONTENT_EXPORT NotificationDetails {
 public:
  NotificationDetails(const NotificationDetails& other) = default;
  ~NotificationDetails() = default;

 protected:
   explicit NotificationDetails(const void* ptr) : ptr_(ptr) {}

  // Declaring this const allows Details<T> to be used with both T = Foo and
  // T = const Foo.
   raw_ptr<const void> ptr_;
};

template <class T>
class Details : public NotificationDetails {
 public:
  explicit Details(T* ptr) : NotificationDetails(ptr) {}
  explicit Details(const NotificationDetails& other)
      : NotificationDetails(other) {}

  T* operator->() const { return ptr(); }
  // The casts here allow this to compile with both T = Foo and T = const Foo.
  T* ptr() const { return static_cast<T*>(const_cast<void*>(ptr_.get())); }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_DETAILS_H_
