// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_FAKE_AREA_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_FAKE_AREA_SOURCE_H_

#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class FakeAreaSource : public GarbageCollected<FakeAreaSource>,
                       public CachedStorageArea::Source {
  USING_GARBAGE_COLLECTED_MIXIN(FakeAreaSource);

 public:
  explicit FakeAreaSource(const KURL& page_url) : page_url_(page_url) {}

  KURL GetPageUrl() const override { return page_url_; }
  bool EnqueueStorageEvent(const String& key,
                           const String& old_value,
                           const String& new_value,
                           const String& url) override {
    events.push_back(Event{key, old_value, new_value, url});
    return true;
  }

  blink::WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      const char* name,
      WebScopedVirtualTimePauser::VirtualTaskDuration duration) override {
    return blink::WebScopedVirtualTimePauser();
  }

  struct Event {
    String key, old_value, new_value, url;
  };

  Vector<Event> events;

 private:
  KURL page_url_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_TESTING_FAKE_AREA_SOURCE_H_
